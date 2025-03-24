/*
 * wifi_app.h
 *
 *  Created on: Jul 5, 2024
 *      Author: LattePanda
 */

#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

// Callback typedef
typedef void (*wifi_connected_event_callback_t)(void);

// WiFi application settings
#define WIFI_AP_SSID 				"ESP32_AP" 		// AP name
#define WIFI_AP_PASSWORD			"password" 		// AP pass
#define WIFI_AP_CHANNEL				6				// AP channel
#define WIFI_AP_SSID_HIDDEN			0				// AP visibility
#define WIFI_AP_MAX_CONNECTIONS		5				// AP max clients
#define WIFI_AP_BEACON_INTERVAL		100 			// AP beacon: 100 milliseconds recommended
#define WIFI_AP_IP					"192.168.0.1"	// AP default IP
#define WIFI_AP_GATEWAY				"192.168.0.1"	// AP default Gateway (should be the same as the IP)
#define WIFI_AP_NETMASK				"255.255.255.0"	// AP net mask
#define WIFI_AP_BANDWIDTH			WIFI_BW_HT20	// AP bandwidth 10Mhz ( 40 MHz is the other option )
#define WIFI_STA_POWER_SAVE			WIFI_PS_NONE	// Power save is not used
#define MAX_SSID_LENGTH				32				// IEEE standard maximum
#define MAX_PASSWORD_LENGTH			64				// IEEE standard maximum
#define MAX_CONNECTION_RETRIES		5				// Retry number on disconnect

// WiFi max credential lengths
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

// netif object for the Station and Access Point
extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

/**
 * Massage IDs for the WiFi application task
 * @note Expand this based on your application requirements.
 */
typedef enum wifi_app_message
{
	WIFI_APP_MSG_START_HTTP_SERVER = 0,
	WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
	WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
	WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
	WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS,
	WIFI_APP_MSG_STA_DISCONNECTED,
} wifi_app_message_e;

typedef struct wifi_app_queue_message
{
	wifi_app_message_e msgID;
} wifi_app_queue_message_t;

/**
 * Sends a massage to the queue
 * @param msgID message ID from the wifi_app_message_e enum.
 * @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE.
 * @note Expand the parameter list based on your requirements e.g. how you've expanded the wifi_app_queue_message_t.
 */
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);

/**
 * Starts the WiFi RTOS task
 */
void wifi_app_start(void);

/**
 * Gets the WiFi configuration
 */
wifi_config_t* wifi_app_get_wifi_config(void);

/**
 * Sets the callback function.
 */
void wifi_app_set_callback(wifi_connected_event_callback_t cb);

/**
 * Calls the callback function
 */
void wifi_app_call_callback(void);

#endif /* MAIN_WIFI_APP_H_ */
