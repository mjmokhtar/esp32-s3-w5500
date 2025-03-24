/**
 * Application entry point.
 */
/*
 * main.c
 *  Created on: Mar 17, 2025
 *      Author: LattePanda
 */
#include "esp_log.h"
#include "esp_event.h"
#include <string.h>

#include "nvs_flash.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"
#include "wifi_reset_button.h"
#include "http_server.h"
#include "ethernet_app.h"


static const char TAG[] = "main";

void wifi_application_connected_events(void)
{
    ESP_LOGI(TAG, "WiFi Application Connected!!");
    sntp_time_sync_task_start();
}

void eth_application_connected_events(void)
{
    ESP_LOGI(TAG, "Ethernet Application Connected!!");
    sntp_time_sync_task_start();
}

// Di main.c
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // Inisialisasi TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    esp_err_t err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Default event loop already created, skipping...");
    } else {
        ESP_ERROR_CHECK(err);
    }
    
    // LANGKAH 1: Inisialisasi HTTP server
    http_server_start();
    
    // LANGKAH 2: Set Ethernet connected callback & start Ethernet
    ethernet_app_set_callback(&eth_application_connected_events);
    ethernet_app_start();
    
    // LANGKAH 3: Start WiFi (setelah Ethernet, untuk menghindari konflik)
    wifi_app_start();
    
    // LANGKAH 4: Configure WiFi reset button
    wifi_reset_button_config();
    
    // LANGKAH 5: Set WiFi connected event callbacks
    wifi_app_set_callback(&wifi_application_connected_events);
    
    
    
    
}