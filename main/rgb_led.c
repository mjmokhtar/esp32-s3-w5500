/*
 * rgb_led.c
 *
 *  Created on: Jul 4, 2024
 *      Author: LattePanda
 */


#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "rgb_led.h"

static const char *TAG = "rgb_led";

// Handle untuk LED Strip WS2812
static led_strip_handle_t led_strip;
static bool initialized = false;

/**
 * Inisialisasi LED Strip WS2812
 */
void rgb_led_init(void)
{
    if (initialized) {
        return;
    }

    ESP_LOGI(TAG, "Initializing RGB LED on GPIO %d", RGB_LED_GPIO);

    // Konfigurasi LED Strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_GPIO,
        .max_leds = 1, // Hanya satu LED pada board
    };

    // Konfigurasi backend RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,  // Tidak menggunakan DMA
    };

    // Inisialisasi LED Strip
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // Matikan LED saat pertama kali diinisialisasi
    led_strip_clear(led_strip);
    initialized = true;
}

/**
 * Mengatur warna LED Strip WS2812 menggunakan RGB
 */
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!initialized) {
        rgb_led_init();
    }

    ESP_LOGI(TAG, "Setting LED Color -> R: %d, G: %d, B: %d", red, green, blue);

    // Set warna pada LED
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
}

/**
 * Indikator LED untuk WiFi App Started (Ungu)
 */
void rgb_led_wifi_app_started(void)
{
    if (!initialized) {
        rgb_led_init();
    }
    rgb_led_set_color(255, 0, 255); // Warna ungu
}

/**
 * Indikator LED untuk HTTP Server Started (Kuning)
 */
void rgb_led_http_server_started(void)
{
    if (!initialized) {
        rgb_led_init();
    }
    rgb_led_set_color(255, 255, 0); // Warna kuning
}

/**
 * Indikator LED untuk WiFi Connected (Hijau)
 */
void rgb_led_wifi_connected(void)
{
    if (!initialized) {
        rgb_led_init();
    }
    rgb_led_set_color(0, 255, 0); // Warna hijau
}