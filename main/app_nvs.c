/*
 * app_nvs.c
 *
 *  Created on: Jul 18, 2024
 *      Author: LattePanda
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "app_nvs.h"
#include "wifi_app.h"
#include "ethernet_app.h"

// Tag for logging to the monitor
static const char TAG[] = "nvs";

// NVS namespace used for station mode credentials
const char app_nvs_sta_creds_namespace[] = "stacreds";

// NVS namespace used for Ethernet configuration
const char app_nvs_eth_config_namespace[] = "ethconfig";

esp_err_t app_nvs_save_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_save_sta_creds: Saving station mode credentials to flash");

	wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

	if (wifi_sta_config)
	{
		esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set SSID
		esp_err = nvs_set_blob(handle, "ssid", wifi_sta_config->sta.ssid, MAX_SSID_LENGTH);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set Password
		esp_err = nvs_set_blob(handle, "password", wifi_sta_config->sta.password, MAX_PASSWORD_LENGTH);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) setting Password to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Commit credentials to NVS
		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) committing credentials to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}
		nvs_close(handle);
		ESP_LOGI(TAG, "app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s Password %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
	}

	printf("app_nvs_save_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

bool app_nvs_load_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;

	ESP_LOGI(TAG, "app_nvs_load_sta_creds: Loading WiFi credentials from flash");

	if (nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle) == ESP_OK)
	{
		wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

//		if (wifi_sta_config == NULL)
//		{
//			wifi_sta_config = (wifi_config_t*)malloc(sizeof(wifi_config_t));
//		}
		memset(wifi_sta_config, 0x00, sizeof(wifi_config_t));

		// Allocate buffer
		size_t wifi_config_size = sizeof(wifi_config_t);
		uint8_t *wifi_config_buff = (uint8_t*)malloc(sizeof(uint8_t) * wifi_config_size);
		memset(wifi_config_buff, 0x00, sizeof(wifi_config_size));

		// Load SSID
		wifi_config_size = sizeof(wifi_sta_config->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", wifi_config_buff, &wifi_config_size);
		if(esp_err != ESP_OK)
		{
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) no station SSID found in NVS\n", esp_err_to_name(esp_err));
			return false;
		}
		memcpy(wifi_sta_config->sta.ssid, wifi_config_buff, wifi_config_size);

		// Load Password
		wifi_config_size = sizeof(wifi_sta_config->sta.password);
		esp_err = nvs_get_blob(handle, "password", wifi_config_buff, &wifi_config_size);
		if(esp_err != ESP_OK)
		{
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) retrieving password!\n", esp_err_to_name(esp_err));
			return false;
		}
		memcpy(wifi_sta_config->sta.password, wifi_config_buff, wifi_config_size);

		free(wifi_config_buff);
		nvs_close(handle);

		printf("app_nvs_load_sta_creds: SSID: %s Password: %s\n", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
		return wifi_sta_config->sta.ssid[0] != '\0';

	}
	else
	{
		return false;
	}
}

esp_err_t app_nvs_clear_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing WiFi station mode credentials from flash");

	esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) opening NVS\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Erase credentials
	esp_err = nvs_erase_all(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) erasing station mode credentials!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Commit clearing credentials from NVS
	esp_err = nvs_commit(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) NVS\n", esp_err_to_name(esp_err));
		return esp_err;
	}
	nvs_close(handle);

	printf("app_nvs_clear_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

/**
 * Save Ethernet configuration to NVS
 */
esp_err_t app_nvs_save_eth_config(const eth_ip_config_t* eth_config)
{
    nvs_handle handle;
    esp_err_t esp_err;
    
    ESP_LOGI(TAG, "app_nvs_save_eth_config: Saving Ethernet configuration to flash");

    if (eth_config == NULL)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Null configuration pointer");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err = nvs_open(app_nvs_eth_config_namespace, NVS_READWRITE, &handle);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) opening NVS handle!", esp_err_to_name(esp_err));
        return esp_err;
    }

    // Save IP address (using blob for consistency)
    esp_err = nvs_set_blob(handle, "ip", eth_config->ip, sizeof(eth_config->ip));
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) setting IP to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Save gateway
    esp_err = nvs_set_blob(handle, "gateway", eth_config->gateway, sizeof(eth_config->gateway));
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) setting Gateway to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Save netmask
    esp_err = nvs_set_blob(handle, "netmask", eth_config->netmask, sizeof(eth_config->netmask));
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) setting Netmask to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Save DNS
    esp_err = nvs_set_blob(handle, "dns", eth_config->dns, sizeof(eth_config->dns));
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) setting DNS to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Save DHCP enabled flag
    esp_err = nvs_set_u8(handle, "dhcp", eth_config->dhcp_enabled ? 1 : 0);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) setting DHCP flag to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Commit configuration to NVS
    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_save_eth_config: Error (%s) committing configuration to NVS!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }
    
    nvs_close(handle);
    ESP_LOGI(TAG, "app_nvs_save_eth_config: Saved Ethernet configuration - IP: %s, GW: %s, Mask: %s, DNS: %s, DHCP: %s", 
             eth_config->ip, eth_config->gateway, eth_config->netmask, 
             eth_config->dns, eth_config->dhcp_enabled ? "Enabled" : "Disabled");

    return ESP_OK;
}

/**
 * Load Ethernet configuration from NVS
 */
bool app_nvs_load_eth_config(eth_ip_config_t* eth_config)
{
    nvs_handle handle;
    esp_err_t esp_err;
    size_t required_size;
    bool success = true;
    
    ESP_LOGI(TAG, "app_nvs_load_eth_config: Loading Ethernet configuration from flash");

    if (eth_config == NULL)
    {
        ESP_LOGE(TAG, "app_nvs_load_eth_config: Null configuration pointer");
        return false;
    }

    if (nvs_open(app_nvs_eth_config_namespace, NVS_READWRITE, &handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_load_eth_config: Error opening NVS handle");
        return false;
    }

    // Load IP address
    required_size = sizeof(eth_config->ip);
    esp_err = nvs_get_blob(handle, "ip", eth_config->ip, &required_size);
    if (esp_err != ESP_OK)
    {
        ESP_LOGW(TAG, "app_nvs_load_eth_config: (%s) no IP address found in NVS", esp_err_to_name(esp_err));
        success = false;
    }

    // Load gateway
    required_size = sizeof(eth_config->gateway);
    esp_err = nvs_get_blob(handle, "gateway", eth_config->gateway, &required_size);
    if (esp_err != ESP_OK)
    {
        ESP_LOGW(TAG, "app_nvs_load_eth_config: (%s) no gateway found in NVS", esp_err_to_name(esp_err));
        success = false;
    }

    // Load netmask
    required_size = sizeof(eth_config->netmask);
    esp_err = nvs_get_blob(handle, "netmask", eth_config->netmask, &required_size);
    if (esp_err != ESP_OK)
    {
        ESP_LOGW(TAG, "app_nvs_load_eth_config: (%s) no netmask found in NVS", esp_err_to_name(esp_err));
        success = false;
    }

    // Load DNS
    required_size = sizeof(eth_config->dns);
    esp_err = nvs_get_blob(handle, "dns", eth_config->dns, &required_size);
    if (esp_err != ESP_OK)
    {
        ESP_LOGW(TAG, "app_nvs_load_eth_config: (%s) no DNS found in NVS", esp_err_to_name(esp_err));
        success = false;
    }

    // Load DHCP enabled flag
    uint8_t dhcp_value;
    esp_err = nvs_get_u8(handle, "dhcp", &dhcp_value);
    if (esp_err != ESP_OK)
    {
        ESP_LOGW(TAG, "app_nvs_load_eth_config: (%s) no DHCP flag found in NVS", esp_err_to_name(esp_err));
        success = false;
    }
    else
    {
        eth_config->dhcp_enabled = (dhcp_value == 1);
    }

    nvs_close(handle);
    
    if (success)
    {
        ESP_LOGI(TAG, "app_nvs_load_eth_config: Loaded Ethernet configuration - IP: %s, GW: %s, Mask: %s, DNS: %s, DHCP: %s", 
                eth_config->ip, eth_config->gateway, eth_config->netmask, 
                eth_config->dns, eth_config->dhcp_enabled ? "Enabled" : "Disabled");
    }
    
    return success;
}

/**
 * Clear Ethernet configuration from NVS
 */
esp_err_t app_nvs_clear_eth_config(void)
{
    nvs_handle handle;
    esp_err_t esp_err;
    
    ESP_LOGI(TAG, "app_nvs_clear_eth_config: Clearing Ethernet configuration from flash");

    esp_err = nvs_open(app_nvs_eth_config_namespace, NVS_READWRITE, &handle);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_clear_eth_config: Error (%s) opening NVS handle!", esp_err_to_name(esp_err));
        return esp_err;
    }

    // Erase all keys for this namespace
    esp_err = nvs_erase_all(handle);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_clear_eth_config: Error (%s) erasing NVS namespace!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    // Commit the erase operation
    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK)
    {
        ESP_LOGE(TAG, "app_nvs_clear_eth_config: Error (%s) committing NVS clear operation!", esp_err_to_name(esp_err));
        nvs_close(handle);
        return esp_err;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "app_nvs_clear_eth_config: Ethernet configuration cleared successfully");
    
    return ESP_OK;
}