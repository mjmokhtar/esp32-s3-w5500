/*
 * ethernet_app.h
 *
 *  Created on: Jul 25, 2024
 */
 
#ifndef MAIN_ETHERNET_APP_H_
#define MAIN_ETHERNET_APP_H_

#include "esp_netif.h"
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback typedef
typedef void (*ethernet_connected_event_callback_t)(void);

// W5500 SPI Ethernet configuration
#define ETH_SPI_HOST          SPI2_HOST
#define ETH_SPI_CLOCK_MHZ     25      // MHz
#define ETH_SPI_MISO_GPIO     13      // Customize these pins for your setup
#define ETH_SPI_MOSI_GPIO     11
#define ETH_SPI_SCLK_GPIO     12
#define ETH_SPI_CS_GPIO       10
#define ETH_SPI_INT_GPIO      4       // Interrupt pin
#define ETH_SPI_PHY_RST_GPIO  -1      // -1 means not connected
#define ETH_SPI_PHY_ADDR      0       // W5500 doesn't use PHY address
#define ETH_SPI_POLLING_MS    0       // 0 means using interrupt mode

// Default static IP configuration (used if DHCP fails)
#define ETH_DEFAULT_IP        "192.168.0.101"
#define ETH_DEFAULT_GATEWAY   "192.168.0.1"
#define ETH_DEFAULT_NETMASK   "255.255.255.0"
#define ETH_DEFAULT_DNS       "8.8.8.8"

// DHCP timeout in milliseconds
#define ETH_DHCP_TIMEOUT_MS   15000   // 15 seconds

// netif object for the Ethernet
extern esp_netif_t* esp_netif_eth;

// Ethernet IP configuration structure
typedef struct {
    char ip[16];          // IPv4 address: xxx.xxx.xxx.xxx\0
    char gateway[16];     // Gateway IP
    char netmask[16];     // Subnet mask
    char dns[16];         // Primary DNS server
    bool dhcp_enabled;    // Whether to use DHCP or static IP
} eth_ip_config_t;

/**
 * Message IDs for the Ethernet application task
 */
typedef enum ethernet_app_message
{
    ETHERNET_APP_MSG_START_HTTP_SERVER = 0,
    ETHERNET_APP_MSG_ETH_CONNECTED_GOT_IP,
    ETHERNET_APP_MSG_ETH_DISCONNECTED,
    ETHERNET_APP_MSG_ETH_STOP,
    ETHERNET_APP_MSG_DHCP_TIMEOUT,
    ETHERNET_APP_MSG_UPDATE_IP_CONFIG
} ethernet_app_message_e;

typedef struct ethernet_app_queue_message
{
    ethernet_app_message_e msgID;
    void* data;  // Additional data for messages that need it
} ethernet_app_queue_message_t;

/**
 * Sends a message to the queue
 * @param msgID message ID from the ethernet_app_message_e enum
 * @param data Optional data to be passed with the message
 * @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE
 */
BaseType_t ethernet_app_send_message(ethernet_app_message_e msgID, void* data);

/**
 * Starts the Ethernet RTOS task
 */
void ethernet_app_start(void);

/**
 * Gets the Ethernet handle
 */
esp_eth_handle_t ethernet_app_get_eth_handle(void);

/**
 * Sets the callback function
 */
void ethernet_app_set_callback(ethernet_connected_event_callback_t cb);

/**
 * Calls the callback function
 */
void ethernet_app_call_callback(void);

/**
 * Gets the current Ethernet IP configuration
 * @param config Pointer to store the configuration
 * @return ESP_OK on success, or an error code
 */
esp_err_t ethernet_app_get_ip_config(eth_ip_config_t* config);

/**
 * Sets the Ethernet IP configuration
 * @param config Pointer to the new configuration
 * @return ESP_OK on success, or an error code
 */
esp_err_t ethernet_app_set_ip_config(const eth_ip_config_t* config);

/**
 * Apply IP configuration immediately
 * Used after changing IP configuration to apply changes without restarting Ethernet
 * @return ESP_OK on success, or an error code
 */
esp_err_t ethernet_app_apply_ip_config(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ETHERNET_APP_H_ */