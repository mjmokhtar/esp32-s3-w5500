/*
 * app_nvs.h
 *
 *  Created on: Jul 18, 2024
 *      Author: LattePanda
 */
#ifndef MAIN_APP_NVS_H_
#define MAIN_APP_NVS_H_

#include "esp_err.h"
#include "ethernet_app.h" // For eth_ip_config_t

/**
 * Saves station mode WiFi credentials to NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_save_sta_creds(void);

/**
 * Loads the previously saved credentials from NVS.
 * @return true if previously saved credentials were found.
 */
bool app_nvs_load_sta_creds(void);

/**
 * Clears station mode credentials from NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_clear_sta_creds(void);

/**
 * Saves Ethernet configuration to NVS
 * @param eth_config Pointer to the Ethernet configuration
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_save_eth_config(const eth_ip_config_t* eth_config);

/**
 * Loads the previously saved Ethernet configuration from NVS.
 * @param eth_config Pointer to store the loaded configuration
 * @return true if previously saved configuration was found.
 */
bool app_nvs_load_eth_config(eth_ip_config_t* eth_config);

/**
 * Clears Ethernet configuration from NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_clear_eth_config(void);

#endif /* MAIN_APP_NVS_H_ */