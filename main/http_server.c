/*
 * http_server.c
 *
 *  Created on: Jul 6, 2024
 *      Author: LattePanda
 */


#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "sys/param.h"

#include "http_server.h"

#include "ethernet_app.h"
#include "sntp_time_sync.h"
#include "tasks_common.h"
#include "wifi_app.h"

// Tag used for ESP serial console message
static const char TAG[] = "http_server";

// WiFi connect status
static int g_wifi_connect_status = NONE;

// Ethernet connect status
static int g_eth_connect_status = NONE; // Gunakan HTTP_ETH_STATUS_NONE pada http_server.h yang diperbarui

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// Local Time status
static bool g_is_local_time_set = false;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &http_server_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[]	asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]		asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]				asm("_binary_index_html_end");
extern const uint8_t app_css_start[]				asm("_binary_app_css_start");
extern const uint8_t app_css_end[]					asm("_binary_app_css_end");
extern const uint8_t app_js_start[]					asm("_binary_app_js_start");
extern const uint8_t app_js_end[]					asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");

/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameters which can be passed tp the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for(;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");

					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;

					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;

					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

					g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAILED;

					break;

				case HTTP_MSG_WIFI_USER_DISCONNECT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_USER_DISCONNECT");

					g_wifi_connect_status = HTTP_WIFI_STATUS_DISCONNECTED;

					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
					http_server_fw_update_reset_timer();

					break;

				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAIL");
					g_fw_update_status = OTA_UPDATE_FAILED;

					break;

				case HTTP_MSG_TIME_SERVICE_INITIALIZED:
					ESP_LOGI(TAG, "HTTP_MSG_TIME_SERVICE_INITIALIZED");
					g_is_local_time_set = true;

					break;
					
				case HTTP_MSG_ETH_CONNECT_INIT:
				    ESP_LOGI(TAG, "HTTP_MSG_ETH_CONNECT_INIT");
				    g_eth_connect_status = HTTP_ETH_STATUS_CONNECTING;
				    break;
				
				case HTTP_MSG_ETH_CONNECT_SUCCESS:
				    ESP_LOGI(TAG, "HTTP_MSG_ETH_CONNECT_SUCCESS");
				    g_eth_connect_status = HTTP_ETH_STATUS_CONNECT_SUCCESS;
				    break;
				
				case HTTP_MSG_ETH_CONNECT_FAIL:
				    ESP_LOGI(TAG, "HTTP_MSG_ETH_CONNECT_FAIL");
				    g_eth_connect_status = HTTP_ETH_STATUS_CONNECT_FAILED;
				    break;
				
				case HTTP_MSG_ETH_USER_DISCONNECT:
				    ESP_LOGI(TAG, "HTTP_MSG_ETH_USER_DISCONNECT");
				    g_eth_connect_status = HTTP_ETH_STATUS_DISCONNECTED;
				    break;

//				case HTTP_MSG_OTA_UPDATE_INITIALIZED:
//					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_INITIALIZED");
//
//					break;

				default:
					break;
			}
		}
	}
}

/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Jquery requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

	return ESP_OK;
}

/**
 * Sends the index.html page
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;
}

/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

	return ESP_OK;
}

/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

	return ESP_OK;
}

/**
 * Receive the .bin file via the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started
 */
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		// Read the data for the request
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
				{
					// Check if timeout occurred
					if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; ////> Retry receiving if timeout occurred
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

		// Is this the first data we are receiving
		// If so, it will have the information in the header that we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);

			}

			// Write this first part of the data
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		else
		{
			//Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}
	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) ==  ESP_OK)
	{
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
			ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
			flash_successful = true;
		}
		else
		{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
		}
	}
	else
	{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	}

	// We won't update the global variables throughout the file, so send the message about the status
	if (flash_successful){ http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); } else { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); }

	return ESP_OK;
}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the url needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}

/**
 * wifiConnect.json handler is invoked after the connect button is pressed
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnect.json requested");

	size_t len_ssid = 0, len_pass = 0;
	char *ssid_str = NULL, *pass_str = NULL;

	// Get SSID header
	len_ssid = httpd_req_get_hdr_value_len(req, "my-connect-ssid") + 1;
	if (len_ssid > 1)
	{
		ssid_str = malloc(len_ssid);
		if (httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid_str, len_ssid) == ESP_OK)
		{
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-ssid: %s", ssid_str);
		}
	}

	// Get Password header
	len_pass = httpd_req_get_hdr_value_len(req, "my-connect-pwd") + 1;
	if (len_pass > 1)
	{
		pass_str = malloc(len_pass);
		if (httpd_req_get_hdr_value_str(req, "my-connect-pwd", pass_str, len_pass) == ESP_OK)
		{
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-pwd: %s", pass_str);
		}
	}

	// Check lengths
	    if (len_ssid > MAX_SSID_LEN || len_pass > MAX_PASS_LEN) {
	        ESP_LOGE(TAG, "SSID or password exceeds maximum length");
	        return ESP_FAIL;
	    }

	    if (len_ssid == 0 || len_pass == 0) {
	        ESP_LOGE(TAG, "Empty credentials");
	        return ESP_FAIL;
	    }

	    if (!ssid_str || !pass_str) {
	        ESP_LOGE(TAG, "Memory allocation failed");
	        return ESP_FAIL;
	    }

	// Update the WiFi networks configuration and let the WiFi application know
	wifi_config_t* wifi_config = wifi_app_get_wifi_config();
	memset(wifi_config, 0x00, sizeof(wifi_config_t));
	memcpy(wifi_config->sta.ssid, ssid_str, len_ssid);
	memcpy(wifi_config->sta.password, pass_str, len_pass);
	wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);

	free(ssid_str);
	free(pass_str);

	return ESP_OK;
}


/**
 * wifiConnectStatus handler updates the connection status for the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectStatus requested");

	char statusJSON[100];

	sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, statusJSON, strlen(statusJSON));

	return ESP_OK;
}

/**
 * wifiConnectInfo.json handler updates the connection status for the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectInfo.json requested");

	char ipInfoJSON[200];
	memset(ipInfoJSON, 0, sizeof(ipInfoJSON));

	char ip[IP4ADDR_STRLEN_MAX];
	char netmask[IP4ADDR_STRLEN_MAX];
	char gw[IP4ADDR_STRLEN_MAX];

	if (g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS)
	{
		wifi_ap_record_t wifi_data;
		ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
		char *ssid = (char*)wifi_data.ssid;

		esp_netif_ip_info_t ip_info;
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
		esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);

		sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gw, ssid);
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));

	return ESP_OK;
}

/**
 * wifiDisconnect.json handler responds by sending a message to the WiFi application to disconnect.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "wifiDisconnect.json requested");

	wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);

	return ESP_OK;
}

/**
 * locaTime.json handler responds by sending by sending the local time.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_local_time_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/localTime.json requested");

	char localTimeJSON[100] = {0};

	if (g_is_local_time_set)
	{
		sprintf(localTimeJSON, "{\"time\":\"%s\"}", sntp_time_sync_get_time());
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, localTimeJSON, strlen(localTimeJSON));

	return ESP_OK;
}

/**
 * apSSID.json handler responds by sending by sending the AP SSID.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_ap_ssid_json_handler(httpd_req_t* req)
{
	ESP_LOGI(TAG, "/apSSID.json requested");

	char ssidJSON[50];

	wifi_config_t *wifi_config = wifi_app_get_wifi_config();
	esp_wifi_get_config(ESP_IF_WIFI_AP, wifi_config);
	char *ssid = (char*)wifi_config->ap.ssid;

	sprintf(ssidJSON, "{\"ssid\":\"%s\"}", ssid);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ssidJSON, strlen(ssidJSON));

	return ESP_OK;
}

/**
 * ethConnect.json handler dijalankan ketika tombol koneksi Ethernet ditekan
 * dan menangani penerimaan konfigurasi IP (DHCP atau statis) dari pengguna
 * @param req HTTP request yang perlu ditangani
 * @return ESP_OK
 */
static esp_err_t http_server_eth_connect_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/ethConnect.json requested");

    size_t len = 0;
    char ip_mode_str[10] = {0};
    char static_ip_str[16] = {0};
    char static_subnet_str[16] = {0};
    char static_gateway_str[16] = {0};
    char static_dns_str[16] = {0};
    bool dhcp_enabled = true; // Default to DHCP

    // Get IP mode header (dhcp or static)
    len = httpd_req_get_hdr_value_len(req, "ip-mode") + 1;
    if (len > 1 && len < sizeof(ip_mode_str))
    {
        if (httpd_req_get_hdr_value_str(req, "ip-mode", ip_mode_str, len) == ESP_OK)
        {
            ESP_LOGI(TAG, "http_server_eth_connect_json_handler: Found header => ip-mode: %s", ip_mode_str);
            if (strcmp(ip_mode_str, "static") == 0)
            {
                dhcp_enabled = false;
            }
        }
    }

    // If static mode, get the IP configuration
    if (!dhcp_enabled)
    {
        // Get static IP header
        len = httpd_req_get_hdr_value_len(req, "static-ip") + 1;
        if (len > 1 && len < sizeof(static_ip_str))
        {
            if (httpd_req_get_hdr_value_str(req, "static-ip", static_ip_str, len) == ESP_OK)
            {
                ESP_LOGI(TAG, "http_server_eth_connect_json_handler: Found header => static-ip: %s", static_ip_str);
            }
        }

        // Get static subnet mask header
        len = httpd_req_get_hdr_value_len(req, "static-subnet") + 1;
        if (len > 1 && len < sizeof(static_subnet_str))
        {
            if (httpd_req_get_hdr_value_str(req, "static-subnet", static_subnet_str, len) == ESP_OK)
            {
                ESP_LOGI(TAG, "http_server_eth_connect_json_handler: Found header => static-subnet: %s", static_subnet_str);
            }
        }

        // Get static gateway header
        len = httpd_req_get_hdr_value_len(req, "static-gateway") + 1;
        if (len > 1 && len < sizeof(static_gateway_str))
        {
            if (httpd_req_get_hdr_value_str(req, "static-gateway", static_gateway_str, len) == ESP_OK)
            {
                ESP_LOGI(TAG, "http_server_eth_connect_json_handler: Found header => static-gateway: %s", static_gateway_str);
            }
        }

        // Get static DNS header
        len = httpd_req_get_hdr_value_len(req, "static-dns") + 1;
        if (len > 1 && len < sizeof(static_dns_str))
        {
            if (httpd_req_get_hdr_value_str(req, "static-dns", static_dns_str, len) == ESP_OK)
            {
                ESP_LOGI(TAG, "http_server_eth_connect_json_handler: Found header => static-dns: %s", static_dns_str);
            }
            else
            {
                // If DNS not provided, use Google DNS as default
                strcpy(static_dns_str, "8.8.8.8");
            }
        }
        else
        {
            // If DNS not provided, use Google DNS as default
            strcpy(static_dns_str, "8.8.8.8");
        }

        // Validate static IP configurations
        if (strlen(static_ip_str) == 0 || strlen(static_subnet_str) == 0 || strlen(static_gateway_str) == 0)
        {
            ESP_LOGE(TAG, "Invalid static IP configuration");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid static IP configuration");
            return ESP_FAIL;
        }
    }

    // Prepare and update Ethernet configuration
    eth_ip_config_t eth_config;
    memset(&eth_config, 0, sizeof(eth_ip_config_t));
    
    // Set DHCP flag
    eth_config.dhcp_enabled = dhcp_enabled;
    
    // If static mode, set the IP configuration
    if (!dhcp_enabled)
    {
        strncpy(eth_config.ip, static_ip_str, sizeof(eth_config.ip) - 1);
        strncpy(eth_config.netmask, static_subnet_str, sizeof(eth_config.netmask) - 1);
        strncpy(eth_config.gateway, static_gateway_str, sizeof(eth_config.gateway) - 1);
        strncpy(eth_config.dns, static_dns_str, sizeof(eth_config.dns) - 1);
    }
    else
    {
        // For DHCP, use default values (will be overwritten by DHCP)
        strncpy(eth_config.ip, ETH_DEFAULT_IP, sizeof(eth_config.ip) - 1);
        strncpy(eth_config.netmask, ETH_DEFAULT_NETMASK, sizeof(eth_config.netmask) - 1);
        strncpy(eth_config.gateway, ETH_DEFAULT_GATEWAY, sizeof(eth_config.gateway) - 1);
        strncpy(eth_config.dns, ETH_DEFAULT_DNS, sizeof(eth_config.dns) - 1);
    }
    
    // Update Ethernet configuration
    esp_err_t ret = ethernet_app_set_ip_config(&eth_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update Ethernet configuration: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update Ethernet configuration");
        return ESP_FAIL;
    }

    // Notify user that configuration was successful
    http_server_monitor_send_message(HTTP_MSG_ETH_CONNECT_INIT);

    // Apply configuration
    ret = ethernet_app_apply_ip_config();
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to apply Ethernet configuration immediately: %s", esp_err_to_name(ret));
        // Not returning error as configuration will be applied on next connection
    }

    return ESP_OK;
}

/**
 * ethConnectStatus handler updates the Ethernet connection status for the web page
 * @param req HTTP request yang perlu ditangani
 * @return ESP_OK
 */
static esp_err_t http_server_eth_connect_status_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/ethConnectStatus requested");

    char statusJSON[100];

    sprintf(statusJSON, "{\"eth_connect_status\":%d}", g_eth_connect_status);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, statusJSON, strlen(statusJSON));

    return ESP_OK;
}

/**
 * ethConnectInfo.json handler menyediakan informasi koneksi Ethernet
 * @param req HTTP request yang perlu ditangani
 * @return ESP_OK
 */
static esp_err_t http_server_get_eth_connect_info_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/ethConnectInfo.json requested");

    char ipInfoJSON[300];
    memset(ipInfoJSON, 0, sizeof(ipInfoJSON));

    if (g_eth_connect_status == HTTP_ETH_STATUS_CONNECT_SUCCESS)
    {
        eth_ip_config_t eth_config;
        esp_err_t ret = ethernet_app_get_ip_config(&eth_config);
        
        // Get MAC address
        uint8_t mac_addr[6];
        char mac_str[18];
        esp_eth_handle_t eth_handle = ethernet_app_get_eth_handle();
        if (eth_handle != NULL)
        {
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
        else
        {
            strcpy(mac_str, "00:00:00:00:00:00");
        }
        
        if (ret == ESP_OK)
        {
            sprintf(ipInfoJSON, 
                "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"mac\":\"%s\",\"mode\":\"%s\"}",
                eth_config.ip, 
                eth_config.netmask, 
                eth_config.gateway, 
                mac_str,
                eth_config.dhcp_enabled ? "DHCP" : "Static");
        }
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));

    return ESP_OK;
}

/**
 * ethDisconnect.json handler responds by memutuskan koneksi Ethernet
 * @param req HTTP request yang perlu ditangani
 * @return ESP_OK
 */
static esp_err_t http_server_eth_disconnect_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ethDisconnect.json requested");

    // Send stop message to Ethernet task
    ethernet_app_send_message(ETHERNET_APP_MSG_ETH_STOP, NULL);

    // Update status
    http_server_monitor_send_message(HTTP_MSG_ETH_USER_DISCONNECT);

    return ESP_OK;
}

/**
 * ethConfig.json handler mendapatkan konfigurasi Ethernet saat ini
 * @param req HTTP request yang perlu ditangani
 * @return ESP_OK
 */
static esp_err_t http_server_get_eth_config_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/ethConfig.json requested");

    char configJSON[300];
    memset(configJSON, 0, sizeof(configJSON));

    eth_ip_config_t eth_config;
    esp_err_t ret = ethernet_app_get_ip_config(&eth_config);
    
    // Get MAC address
    uint8_t mac_addr[6];
    char mac_str[18];
    esp_eth_handle_t eth_handle = ethernet_app_get_eth_handle();
    if (eth_handle != NULL)
    {
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
    else
    {
        strcpy(mac_str, "00:00:00:00:00:00");
    }
    
    if (ret == ESP_OK)
    {
        sprintf(configJSON, 
            "{\"mode\":%d,\"ip\":\"%s\",\"subnet\":\"%s\",\"gateway\":\"%s\",\"mac\":\"%s\",\"dns\":\"%s\"}",
            eth_config.dhcp_enabled ? ETH_MANAGER_IP_DHCP : ETH_MANAGER_IP_STATIC,
            eth_config.ip, 
            eth_config.netmask, 
            eth_config.gateway, 
            mac_str,
            eth_config.dns);
    }
    else
    {
        // Provide default empty JSON if unable to get config
        sprintf(configJSON, "{\"mode\":%d,\"ip\":\"\",\"subnet\":\"\",\"gateway\":\"\",\"mac\":\"\",\"dns\":\"\"}", 
                ETH_MANAGER_IP_DHCP);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, configJSON, strlen(configJSON));

    return ESP_OK;
}


/**
 * Sets up the default httpd server configuration.
 * @return http server instance handle if successful, NULL otherwise.
 */
static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	
	// create the message queue
	http_server_monitor_queue_handle = xQueueCreate(10, sizeof(http_server_queue_message_t));

	// create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor",
			HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY,
			&task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);

	// The core that the HTTP server will run on
	config.core_id =  HTTP_SERVER_TASK_CORE_ID;

	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;

	// Bump up the stack size (default 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

	// Increase uri handlers
	config.lru_purge_enable = true;
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	config.recv_wait_timeout = 30;
	config.send_wait_timeout = 30;
	config.max_resp_headers = 20;
	config.max_open_sockets = 7;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority '%d'",
			config.server_port,
			config.task_priority);

	//Start the httpd server
	if (httpd_start(&http_server_handle, &config) ==  ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

		// register query handler
		httpd_uri_t jquery_js = {
				.uri = "/jquery-3.3.1.min.js",
				.method = HTTP_GET,
				.handler = http_server_jquery_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &jquery_js);

		// register index.html handler
		httpd_uri_t index_html = {
				.uri = "/index.html",
				.method = HTTP_GET,
				.handler = http_server_index_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		// register app.css handler
		httpd_uri_t app_css = {
				.uri = "/app.css",
				.method = HTTP_GET,
				.handler = http_server_app_css_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);

		// register app.js handler
		httpd_uri_t app_js = {
				.uri = "/app.js",
				.method = HTTP_GET,
				.handler = http_server_app_js_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);

		// register favicon.ico handler
		httpd_uri_t favicon_ico = {
				.uri = "/favicon.ico",
				.method = HTTP_GET,
				.handler = http_server_favicon_ico_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		// register OTAupdate handler
		httpd_uri_t OTA_update = {
				.uri = "/OTAupdate",
				.method = HTTP_POST,
				.handler = http_server_OTA_update_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		// register OTAstatus handler
		httpd_uri_t OTA_status = {
				.uri = "/OTAstatus",
				.method = HTTP_POST,
				.handler = http_server_OTA_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);

		// register wifiConnect.json handler
		httpd_uri_t wifi_connect_json = {
				.uri ="/wifiConnect.json",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

		// register wifiConnectStatus.json handler
		httpd_uri_t wifi_connect_status_json = {
				.uri ="/wifiConnectStatus",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_status_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

		// register wifiConnectInfo.json handler
		httpd_uri_t wifi_connect_info_json = {
				.uri ="/wifiConnectInfo.json",
				.method = HTTP_GET,
				.handler = http_server_get_wifi_connect_info_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_info_json);

		// register wifiDisconnect.json handler
		httpd_uri_t wifi_disconnect_json = {
				.uri ="/wifiDisconnect.json",
				.method = HTTP_DELETE,
				.handler = http_server_wifi_disconnect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_disconnect_json);

		// register localTime.json handler
		httpd_uri_t local_time_json = {
				.uri ="/localTime.json",
				.method = HTTP_GET,
				.handler = http_server_get_local_time_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &local_time_json);

		// register apSSID.json handler
		httpd_uri_t ap_ssid_json = {
				.uri ="/apSSID.json",
				.method = HTTP_GET,
				.handler = http_server_get_ap_ssid_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &ap_ssid_json);
		
		// register ethConnect.json handler
		httpd_uri_t eth_connect_json = {
		    .uri = "/ethConnect.json",
		    .method = HTTP_POST,
		    .handler = http_server_eth_connect_json_handler,
		    .user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &eth_connect_json);
		
		// register ethConnectStatus.json handler
		httpd_uri_t eth_connect_status_json = {
		    .uri = "/ethConnectStatus",
		    .method = HTTP_POST,
		    .handler = http_server_eth_connect_status_json_handler,
		    .user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &eth_connect_status_json);
		
		// register ethConnectInfo.json handler
		httpd_uri_t eth_connect_info_json = {
		    .uri = "/ethConnectInfo.json",
		    .method = HTTP_GET,
		    .handler = http_server_get_eth_connect_info_json_handler,
		    .user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &eth_connect_info_json);
		
		// register ethDisconnect.json handler
		httpd_uri_t eth_disconnect_json = {
		    .uri = "/ethDisconnect.json",
		    .method = HTTP_DELETE,
		    .handler = http_server_eth_disconnect_json_handler,
		    .user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &eth_disconnect_json);
		
		// register ethConfig.json handler
		httpd_uri_t eth_config_json = {
		    .uri = "/ethConfig.json",
		    .method = HTTP_GET,
		    .handler = http_server_get_eth_config_json_handler,
		    .user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &eth_config_json);

		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = http_server_configure();
	}
}

void http_server_stop(void)
{
	if (http_server_handle)
	{
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stoping HTTP server");
		http_server_handle = NULL;
	}
	if (task_http_server_monitor)
	{
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
    // Cek apakah queue sudah diinisialisasi
    if (http_server_monitor_queue_handle == NULL) {
        ESP_LOGW(TAG, "http_server_monitor_send_message: Queue not initialized yet");
        return pdFALSE;
    }
    
    http_server_queue_message_t msg;
    msg.msgID = msgID;
    return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}