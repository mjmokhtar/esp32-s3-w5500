/*
 * ethernet_app.c
 *
 *  Created on: Jul 25, 2024
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"

#include "ethernet_app.h"
#include "http_server.h"
#include "tasks_common.h"
#include "app_nvs.h"

// Tag used for ESP serial console messages
static const char TAG[] = "eth_app";

// Ethernet application callback
static ethernet_connected_event_callback_t ethernet_connected_event_cb;

// Used to track if the SPI bus has been initialized
static bool spi_bus_initialized = false;

// Used to track if the GPIO ISR service has been installed
static bool gpio_isr_service_installed = false;

// Ethernet handle
static esp_eth_handle_t s_eth_handle = NULL;

// DHCP timeout timer
static TimerHandle_t s_dhcp_timer = NULL;

// Current Ethernet IP configuration
static eth_ip_config_t s_eth_ip_config = {
    .ip = ETH_DEFAULT_IP,
    .gateway = ETH_DEFAULT_GATEWAY,
    .netmask = ETH_DEFAULT_NETMASK,
    .dns = ETH_DEFAULT_DNS,
    .dhcp_enabled = true
};

/**
 * Ethernet application event group handle and status bits
 */
static EventGroupHandle_t ethernet_app_event_group;
const int ETHERNET_APP_ETH_CONNECTED_BIT                = BIT0;
const int ETHERNET_APP_ETH_GOT_IP_BIT                   = BIT1;
const int ETHERNET_APP_ETH_DISCONNECTED_BIT             = BIT2;
const int ETHERNET_APP_ETH_STOP_BIT                     = BIT3;
const int ETHERNET_APP_ETH_USING_STATIC_IP_BIT          = BIT4;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t ethernet_app_queue_handle;

// netif object for the Ethernet
esp_netif_t* esp_netif_eth = NULL;

/**
 * DHCP timeout callback
 * @param xTimer Timer handle that expired
 */
static void dhcp_timeout_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "DHCP timeout - switching to static IP");
    ethernet_app_send_message(ETHERNET_APP_MSG_DHCP_TIMEOUT, NULL);
}

/**
 * SPI bus initialization for W5500
 */
static esp_err_t spi_bus_init(void)
{
    esp_err_t ret = ESP_OK;

    if (spi_bus_initialized) {
        ESP_LOGI(TAG, "SPI bus already initialized");
        return ret;
    }

    // Install GPIO ISR handler to be able to service W5500 interrupts
    if (ETH_SPI_INT_GPIO >= 0 && !gpio_isr_service_installed) {
        ret = gpio_install_isr_service(0);
        if (ret == ESP_OK) {
            gpio_isr_service_installed = true;
            ESP_LOGI(TAG, "GPIO ISR service installed");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
            ret = ESP_OK; // ISR handler already installed, continue
        } else {
            ESP_LOGE(TAG, "GPIO ISR handler install failed");
            return ret;
        }
    }

    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_SPI_MISO_GPIO,
        .mosi_io_num = ETH_SPI_MOSI_GPIO,
        .sclk_io_num = ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    ret = spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialize failed");
        return ret;
    }
    
    spi_bus_initialized = true;
    ESP_LOGI(TAG, "SPI bus initialized");
    
    return ret;
}

/**
 * Initialize W5500 Ethernet hardware
 */
static esp_eth_handle_t eth_init_w5500(void)
{
    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = ETH_SPI_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_SPI_PHY_RST_GPIO;

    // Configure SPI interface for W5500
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = ETH_SPI_CS_GPIO
    };
    
    // Initialize SPI bus if not already initialized
    esp_err_t spi_init_ret = spi_bus_init();
    if (spi_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return NULL;
    }
    
    // W5500 specific configuration
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = ETH_SPI_INT_GPIO;
    w5500_config.poll_period_ms = ETH_SPI_POLLING_MS;
    
    // Create MAC and PHY instances for W5500
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create MAC instance");
        return NULL;
    }
    
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create PHY instance");
        mac->del(mac);
        return NULL;
    }

    // Init Ethernet driver and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    
    esp_err_t eth_install_ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (eth_install_ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed");
        phy->del(phy);
        mac->del(mac);
        return NULL;
    }
    
    // Set MAC address
    uint8_t base_mac_addr[6];
    esp_err_t mac_ret = esp_efuse_mac_get_default(base_mac_addr);
    if (mac_ret != ESP_OK) {
        ESP_LOGE(TAG, "Get EFUSE MAC failed");
        esp_eth_driver_uninstall(eth_handle);
        phy->del(phy);
        mac->del(mac);
        return NULL;
    }
    
    ESP_LOGI(TAG, "MAC address: %02x:%02x:%02x:%02x:%02x:%02x", 
             base_mac_addr[0], base_mac_addr[1], base_mac_addr[2], 
             base_mac_addr[3], base_mac_addr[4], base_mac_addr[5]);
             
    esp_err_t ioctl_ret = esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, base_mac_addr);
    if (ioctl_ret != ESP_OK) {
        ESP_LOGE(TAG, "Set MAC address failed");
        esp_eth_driver_uninstall(eth_handle);
        phy->del(phy);
        mac->del(mac);
        return NULL;
    }
    
    return eth_handle;
}

/**
 * Deinitialize W5500 Ethernet
 */
static esp_err_t eth_deinit_w5500(esp_eth_handle_t eth_handle)
{
    if (eth_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
    
    esp_eth_get_mac_instance(eth_handle, &mac);
    esp_eth_get_phy_instance(eth_handle, &phy);
    
    esp_err_t ret = esp_eth_driver_uninstall(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver uninstall failed");
        return ret;
    }
    
    if (mac != NULL) {
        mac->del(mac);
    }
    
    if (phy != NULL) {
        phy->del(phy);
    }
    
    if (spi_bus_initialized) {
        ret = spi_bus_free(ETH_SPI_HOST);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus free failed");
            return ret;
        }
        spi_bus_initialized = false;
    }
    
    if (gpio_isr_service_installed) {
        gpio_uninstall_isr_service();
        gpio_isr_service_installed = false;
    }
    
    return ESP_OK;
}

/**
 * Configure static IP for the Ethernet interface
 */
static esp_err_t configure_static_ip(void)
{
    esp_netif_dhcpc_stop(esp_netif_eth);
    
    // Set static IP address
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
    
    // Convert string IPs to IP addresses
    inet_pton(AF_INET, s_eth_ip_config.ip, &ip_info.ip);
    inet_pton(AF_INET, s_eth_ip_config.gateway, &ip_info.gw);
    inet_pton(AF_INET, s_eth_ip_config.netmask, &ip_info.netmask);
    
    // Apply network interface IP settings
    esp_err_t ret = esp_netif_set_ip_info(esp_netif_eth, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set DNS servers
    ip_addr_t dns_server;
    inet_pton(AF_INET, s_eth_ip_config.dns, &dns_server.u_addr.ip4.addr);
    dns_server.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dns_server);
    
    ESP_LOGI(TAG, "Configured static IP: %s", s_eth_ip_config.ip);
    ESP_LOGI(TAG, "Configured gateway: %s", s_eth_ip_config.gateway);
    ESP_LOGI(TAG, "Configured netmask: %s", s_eth_ip_config.netmask);
    ESP_LOGI(TAG, "Configured DNS: %s", s_eth_ip_config.dns);
    
    xEventGroupSetBits(ethernet_app_event_group, ETHERNET_APP_ETH_USING_STATIC_IP_BIT);
    ethernet_app_send_message(ETHERNET_APP_MSG_ETH_CONNECTED_GOT_IP, NULL);
    
    return ESP_OK;
}

/**
 * Ethernet application event handler
 * @param arg data, aside from event data, that is passed to the handler when it is called
 * @param event_base the base id of the event to register the handler for
 * @param event_id the id for the event to register the handler for
 * @param event_data event data
 */
static void ethernet_app_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet Link Up");
                xEventGroupSetBits(ethernet_app_event_group, ETHERNET_APP_ETH_CONNECTED_BIT);
                
                // Tambahkan ini: Kirim pesan ke HTTP server
                http_server_monitor_send_message(HTTP_MSG_ETH_CONNECT_INIT);
                
                // Start DHCP timer only if DHCP is enabled
                if (s_eth_ip_config.dhcp_enabled) {
                    // Start timer for DHCP timeout
                    xTimerStart(s_dhcp_timer, 0);
                } else {
                    // Configure static IP immediately
                    configure_static_ip();
                }
                break;
                
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "Ethernet Link Down");
                // Tambahkan ini: Kirim pesan ke HTTP server
                http_server_monitor_send_message(HTTP_MSG_ETH_USER_DISCONNECT);
                xEventGroupSetBits(ethernet_app_event_group, ETHERNET_APP_ETH_DISCONNECTED_BIT);
                xEventGroupClearBits(ethernet_app_event_group, 
                                    ETHERNET_APP_ETH_CONNECTED_BIT | 
                                    ETHERNET_APP_ETH_GOT_IP_BIT | 
                                    ETHERNET_APP_ETH_USING_STATIC_IP_BIT);
                
                // Stop DHCP timer if running
                if (xTimerIsTimerActive(s_dhcp_timer)) {
                    xTimerStop(s_dhcp_timer, 0);
                }
                
                ethernet_app_send_message(ETHERNET_APP_MSG_ETH_DISCONNECTED, NULL);
                break;
                
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet Started");
                break;
                
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet Stopped");
                xEventGroupSetBits(ethernet_app_event_group, ETHERNET_APP_ETH_STOP_BIT);
                xEventGroupClearBits(ethernet_app_event_group, 
                                    ETHERNET_APP_ETH_CONNECTED_BIT | 
                                    ETHERNET_APP_ETH_GOT_IP_BIT | 
                                    ETHERNET_APP_ETH_USING_STATIC_IP_BIT |
                                    ETHERNET_APP_ETH_DISCONNECTED_BIT);
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_ETH_GOT_IP:
                // Stop DHCP timer as we got an IP
                
                // Tambahkan ini: Kirim pesan ke HTTP server
                http_server_monitor_send_message(HTTP_MSG_ETH_CONNECT_SUCCESS);
                
                if (xTimerIsTimerActive(s_dhcp_timer)) {
                    xTimerStop(s_dhcp_timer, 0);
                }
                
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "Ethernet Got IP Address");
                ESP_LOGI(TAG, "~~~~~~~~~~~");
                ESP_LOGI(TAG, "ETHIP: " IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "ETHMASK: " IPSTR, IP2STR(&event->ip_info.netmask));
                ESP_LOGI(TAG, "ETHGW: " IPSTR, IP2STR(&event->ip_info.gw));
                ESP_LOGI(TAG, "~~~~~~~~~~~");
                
                // Update current IP configuration from DHCP result
                if (!(xEventGroupGetBits(ethernet_app_event_group) & ETHERNET_APP_ETH_USING_STATIC_IP_BIT)) {
                    sprintf(s_eth_ip_config.ip, IPSTR, IP2STR(&event->ip_info.ip));
                    sprintf(s_eth_ip_config.gateway, IPSTR, IP2STR(&event->ip_info.gw));
                    sprintf(s_eth_ip_config.netmask, IPSTR, IP2STR(&event->ip_info.netmask));
                    // DNS will remain as previously configured
                }
                
                xEventGroupSetBits(ethernet_app_event_group, ETHERNET_APP_ETH_GOT_IP_BIT);
                ethernet_app_send_message(ETHERNET_APP_MSG_ETH_CONNECTED_GOT_IP, NULL);
                break;
                
            default:
                break;
        }
    }
}

/**
 * Main task for the Ethernet application
 * @param pvParameters parameter which can be passed to the task
 */
static void ethernet_app_task(void *pvParameters)
{
    ethernet_app_queue_message_t msg;
    
    // Create default event loop if not already created
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethernet_app_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ethernet_app_event_handler, NULL));
    
    // Create DHCP timeout timer
    s_dhcp_timer = xTimerCreate(
        "dhcp_timer",
        pdMS_TO_TICKS(ETH_DHCP_TIMEOUT_MS),
        pdFALSE,  // Don't auto reload
        NULL,     // Timer ID
        dhcp_timeout_callback
    );
    
    if (s_dhcp_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create DHCP timer");
        vTaskDelete(NULL);
        return;
    }
    
    // Try to load saved IP configuration
    eth_ip_config_t loaded_config;
    if (app_nvs_load_eth_config(&loaded_config)) {
        memcpy(&s_eth_ip_config, &loaded_config, sizeof(eth_ip_config_t));
        ESP_LOGI(TAG, "Loaded Ethernet configuration from NVS");
    } else {
        ESP_LOGI(TAG, "No saved Ethernet configuration found, using defaults");
    }
    
    // Initialize TCP/IP network interface (should be called only once in application)
    if (esp_netif_eth == NULL) {
        // Create new default instance of esp-netif for Ethernet
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_eth = esp_netif_new(&cfg);
    }
    
    // Initialize W5500 Ethernet
    s_eth_handle = eth_init_w5500();
    if (s_eth_handle == NULL) {
        ESP_LOGE(TAG, "Ethernet initialization failed");
        vTaskDelete(NULL);
        return;
    }
    
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(esp_netif_eth, esp_eth_new_netif_glue(s_eth_handle)));
    
    // If static IP is configured, set it now before starting the Ethernet
    if (!s_eth_ip_config.dhcp_enabled) {
        ESP_LOGI(TAG, "Using static IP configuration");
        configure_static_ip();
    }
    
    // Start Ethernet driver
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
    
    ESP_LOGI(TAG, "Ethernet started successfully");
    
    for(;;)
    {
        if (xQueueReceive(ethernet_app_queue_handle, &msg, portMAX_DELAY))
        {
            switch (msg.msgID)
            {
                case ETHERNET_APP_MSG_START_HTTP_SERVER:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_START_HTTP_SERVER");
                    
                    // HTTP server start logic will be handled by the main application
                    
                    break;
                    
                case ETHERNET_APP_MSG_ETH_CONNECTED_GOT_IP:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_ETH_CONNECTED_GOT_IP");
                    
                    // Check for connection callback
                    if (ethernet_connected_event_cb) {
                        ethernet_app_call_callback();
                    }
                    
                    break;
                    
                case ETHERNET_APP_MSG_ETH_DISCONNECTED:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_ETH_DISCONNECTED");
                    
                    xEventGroupClearBits(ethernet_app_event_group, 
                                        ETHERNET_APP_ETH_CONNECTED_BIT | 
                                        ETHERNET_APP_ETH_GOT_IP_BIT | 
                                        ETHERNET_APP_ETH_USING_STATIC_IP_BIT);
                    
                    break;
                    
                case ETHERNET_APP_MSG_ETH_STOP:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_ETH_STOP");
                    
                    if (s_eth_handle != NULL) {
                        ESP_ERROR_CHECK(esp_eth_stop(s_eth_handle));
                        ESP_ERROR_CHECK(eth_deinit_w5500(s_eth_handle));
                        s_eth_handle = NULL;
                        
                        if (s_dhcp_timer != NULL) {
                            xTimerDelete(s_dhcp_timer, 0);
                            s_dhcp_timer = NULL;
                        }
                    }
                    
                    break;
                    
                case ETHERNET_APP_MSG_DHCP_TIMEOUT:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_DHCP_TIMEOUT - Switching to static IP");
                    
                    // Switch to static IP configuration
                    configure_static_ip();
                    
                    break;
                    
                case ETHERNET_APP_MSG_UPDATE_IP_CONFIG:
                    ESP_LOGI(TAG, "ETHERNET_APP_MSG_UPDATE_IP_CONFIG");
                    
                    if (msg.data != NULL) {
                        eth_ip_config_t* new_config = (eth_ip_config_t*)msg.data;
                        
                        // Check if we're changing from DHCP to static or vice versa
                        bool mode_changing = (s_eth_ip_config.dhcp_enabled != new_config->dhcp_enabled);
                        
                        // Update IP configuration
                        memcpy(&s_eth_ip_config, new_config, sizeof(eth_ip_config_t));
                        free(msg.data);  // Free the allocated memory for the message data
                        
                        // Save configuration to NVS
                        app_nvs_save_eth_config(&s_eth_ip_config);
                        
                        // If we're already connected, apply new settings
                        if (xEventGroupGetBits(ethernet_app_event_group) & ETHERNET_APP_ETH_CONNECTED_BIT) {
                            if (mode_changing) {
                                if (s_eth_ip_config.dhcp_enabled) {
                                    // Switch to DHCP
                                    ESP_LOGI(TAG, "Switching to DHCP");
                                    xEventGroupClearBits(ethernet_app_event_group, ETHERNET_APP_ETH_USING_STATIC_IP_BIT);
                                    esp_netif_dhcpc_start(esp_netif_eth);
                                    
                                    // Start DHCP timeout timer
                                    xTimerStart(s_dhcp_timer, 0);
                                } else {
                                    // Switch to static IP
                                    ESP_LOGI(TAG, "Switching to static IP");
                                    configure_static_ip();
                                }
                            } else if (!s_eth_ip_config.dhcp_enabled) {
                                // We're still using static IP, just update the settings
                                configure_static_ip();
                            }
                        }
                    }
                    
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/**
 * Send message to the Ethernet application task
 */
BaseType_t ethernet_app_send_message(ethernet_app_message_e msgID, void* data)
{
    ethernet_app_queue_message_t msg;
    msg.msgID = msgID;
    msg.data = data;
    return xQueueSend(ethernet_app_queue_handle, &msg, portMAX_DELAY);
}

/**
 * Set callback function for Ethernet connected event
 */
void ethernet_app_set_callback(ethernet_connected_event_callback_t cb)
{
    ethernet_connected_event_cb = cb;
}

/**
 * Call the Ethernet connected event callback
 */
void ethernet_app_call_callback(void)
{
    if (ethernet_connected_event_cb) {
        ethernet_connected_event_cb();
    }
}

/**
 * Get the Ethernet handle
 */
esp_eth_handle_t ethernet_app_get_eth_handle(void)
{
    return s_eth_handle;
}

/**
 * Get the current Ethernet IP configuration
 */
esp_err_t ethernet_app_get_ip_config(eth_ip_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &s_eth_ip_config, sizeof(eth_ip_config_t));
    return ESP_OK;
}

/**
 * Set the Ethernet IP configuration
 */
esp_err_t ethernet_app_set_ip_config(const eth_ip_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate memory for the new configuration (will be freed in the Ethernet task)
    eth_ip_config_t* new_config = malloc(sizeof(eth_ip_config_t));
    if (new_config == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(new_config, config, sizeof(eth_ip_config_t));
    
    // Send message to the ethernet task to update the configuration
    if (ethernet_app_send_message(ETHERNET_APP_MSG_UPDATE_IP_CONFIG, new_config) != pdTRUE) {
        free(new_config);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * Apply IP configuration immediately
 */
esp_err_t ethernet_app_apply_ip_config(void)
{
    // Check if Ethernet is connected
    if (!(xEventGroupGetBits(ethernet_app_event_group) & ETHERNET_APP_ETH_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "Cannot apply IP configuration, Ethernet not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Apply based on DHCP setting
    if (s_eth_ip_config.dhcp_enabled) {
        // Switch to DHCP
        ESP_LOGI(TAG, "Applying DHCP configuration");
        xEventGroupClearBits(ethernet_app_event_group, ETHERNET_APP_ETH_USING_STATIC_IP_BIT);
        esp_netif_dhcpc_start(esp_netif_eth);
        
        // Start DHCP timeout timer
        xTimerStart(s_dhcp_timer, 0);
    } else {
        // Switch to static IP
        ESP_LOGI(TAG, "Applying static IP configuration");
        esp_err_t ret = configure_static_ip();
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    return ESP_OK;
}

/**
 * Start the Ethernet application
 */
void ethernet_app_start(void)
{
    ESP_LOGI(TAG, "STARTING ETHERNET APPLICATION");
    
    // Create message queue
    ethernet_app_queue_handle = xQueueCreate(5, sizeof(ethernet_app_queue_message_t));
    
    // Create Ethernet application event group
    ethernet_app_event_group = xEventGroupCreate();
    
    // Start the Ethernet application task
    xTaskCreatePinnedToCore(&ethernet_app_task, 
                           "ethernet_app_task", 
                           ETH_APP_TASK_STACK_SIZE, 
                           NULL, 
                           ETH_APP_TASK_PRIORITY, 
                           NULL, 
                           ETH_APP_TASK_CORE_ID);
}