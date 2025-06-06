/*
 * sntp_time_sync.c
 *
 *  Created on: Jul 18, 2024
 *      Author: LattePanda
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"

#include "tasks_common.h"
#include "http_server.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"
#include "lwip/dns.h"

static const char TAG[] = "sntp_time_sync";

// SNTP operating mode set status
static bool sntp_op_mode_set = false;

static bool sntp_started = false;

bool sntp_time_sync_is_started(void)
{
    return sntp_started;
}


/**
 * Initialize SNTP service using SNTP_OPMODE_POLL mode.
 */
static void sntp_time_sync_init_sntp(void)
{
	ESP_LOGI(TAG, "Initializing the SNTP service");

	if (!sntp_op_mode_set)
	{
		// Set the operating mode
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntp_op_mode_set = true;
	}
	
	 // Tambahkan ini:
    ip_addr_t dnsserver;
    ipaddr_aton("8.8.8.8", &dnsserver);
    dns_setserver(0, &dnsserver);

	sntp_setservername(0, "pool.ntp.org");

	// Initialize the servers
	sntp_init();

	// Let the http_server know service is initialized
	http_server_monitor_send_message(HTTP_MSG_TIME_SERVICE_INITIALIZED);
}

/**
 * Gets the current time and if the current time is not up to date,
 * the sntp_time_synch_init_sntp function is called.
 */
static void sntp_time_sync_obtain_time(void)
{
	time_t now = 0;
	struct tm time_info = {0};

	time(&now);
	localtime_r(&now, &time_info);

	// Check the time, in case we need to initialize/reinitialize
	if (sntp_op_mode_set == false || time_info.tm_year < (2016 - 1900))
	{
		sntp_time_sync_init_sntp();
		// Set the local time zone
		setenv("TZ", "WIB-7", 1);
		tzset();
	}
}

/**
 * The SNTP time synchronization task.
 * @param arg pvParam.
 */
static void sntp_time_sync(void *pvParam)
{
    sntp_time_sync_obtain_time(); // init SNTP

    for (int retry = 0; retry < 20; retry++)  // max 20 detik
    {
        time_t now;
        struct tm time_info;
        time(&now);
        localtime_r(&now, &time_info);

        if (time_info.tm_year >= (2016 - 1900)) {
            ESP_LOGI(TAG, "Time synchronized successfully");
            break;
        }

        ESP_LOGI(TAG, "Waiting for time sync...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);  // agar task berhenti
}



char* sntp_time_sync_get_time(void)
{
	static char time_buffer[100] = {0};

	time_t now = 0;
	struct tm time_info = {0};

	time(&now);
	localtime_r(&now, &time_info);

	if (time_info.tm_year < (2016 - 1900))
	{
		ESP_LOGI(TAG, "Time is not set yet");
	}
	else
	{
		strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &time_info);
		ESP_LOGI(TAG, "Current time info: %s", time_buffer);
	}

	return time_buffer;
}

void sntp_time_sync_task_start(void)
{
    if (!sntp_started) {
        sntp_started = true;
        xTaskCreatePinnedToCore(&sntp_time_sync, "sntp_time_sync", SNTP_TIME_SYNC_TASK_STACK_SIZE, NULL, SNTP_TIME_SYNC_TASK_PRIORITY, NULL, SNTP_TIME_SYNC_TASK_CORE_ID);
    } else {
        ESP_LOGW(TAG, "SNTP time sync task already started");
    }
}
