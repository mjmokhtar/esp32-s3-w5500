/*
 * http_server.h
 *
 *  Created on: Jul 6, 2024
 *      Author: LattePanda
 */

#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_

#include "freertos/FreeRTOS.h"

// Default: None
#define NONE 0

// WiFi Status for HTTP Request
#define HTTP_WIFI_STATUS_CONNECTING 1
#define HTTP_WIFI_STATUS_CONNECT_FAILED 2
#define HTTP_WIFI_STATUS_CONNECT_SUCCESS 3
#define HTTP_WIFI_STATUS_DISCONNECTED 4

// Ethernet Status for HTTP Request
#define HTTP_ETH_STATUS_NONE 0
#define HTTP_ETH_STATUS_CONNECTING 1
#define HTTP_ETH_STATUS_CONNECT_FAILED 2
#define HTTP_ETH_STATUS_CONNECT_SUCCESS 3
#define HTTP_ETH_STATUS_DISCONNECTED 4

// Ethernet IP Mode
#define ETH_MANAGER_IP_DHCP 1
#define ETH_MANAGER_IP_STATIC 2

// Firmware update status
#define OTA_UPDATE_PENDING 0
#define OTA_UPDATE_SUCCESSFUL 1
#define OTA_UPDATE_FAILED 2

/**
 * HTTP server message types
 */
typedef enum http_server_message
{
	HTTP_MSG_WIFI_CONNECT_INIT = 0,
	HTTP_MSG_WIFI_CONNECT_SUCCESS,
	HTTP_MSG_WIFI_CONNECT_FAIL,
	HTTP_MSG_WIFI_USER_DISCONNECT,
	HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
	HTTP_MSG_OTA_UPDATE_FAILED,
	HTTP_MSG_TIME_SERVICE_INITIALIZED,
	HTTP_MSG_ETH_CONNECT_INIT,
	HTTP_MSG_ETH_CONNECT_SUCCESS,
	HTTP_MSG_ETH_CONNECT_FAIL,
	HTTP_MSG_ETH_USER_DISCONNECT
} http_server_message_e;

/**
 * Message for the HTTP monitor
 */
typedef struct http_server_queue_message
{
	http_server_message_e msgID;
} http_server_queue_message_t;

/**
 * Sends a message to the queue
 * @param msgID message ID from the http_server_message_e enum.
 * @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE.
 */
BaseType_t http_server_monitor_send_message(http_server_message_e msgID);

/**
 * Starts the HTTP server
 */
void http_server_start(void);

/**
 * Stops the HTTP server
 */
void http_server_stop(void);

/**
 * Firmware update reset callback
 * @param arg argument pointer
 */
void http_server_fw_update_reset_callback(void *arg);

#endif /* MAIN_HTTP_SERVER_H_ */