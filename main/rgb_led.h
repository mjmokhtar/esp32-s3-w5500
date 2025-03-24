/*
 * rgb_led.h
 *
 *  Created on: Jul 4, 2024
 *      Author: LattePanda
 */
#include <stdint.h>
#include <stdbool.h>

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

// ESP32-S3 memiliki LED RGB terintegrasi di GPIO 48
#define RGB_LED_GPIO 48

/**
 * Inisialisasi LED RGB Strip
 */
void rgb_led_init(void);

/**
 * Mengatur warna LED RGB menggunakan nilai RGB (0-255)
 */
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * Indikator warna saat aplikasi WiFi telah dimulai (Ungu)
 */
void rgb_led_wifi_app_started(void);

/**
 * Indikator warna saat server HTTP telah dimulai (Kuning)
 */
void rgb_led_http_server_started(void);

/**
 * Indikator warna saat ESP32 terhubung ke WiFi (Hijau)
 */
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */