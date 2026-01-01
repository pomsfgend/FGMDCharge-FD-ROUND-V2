#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define WEB_SCAN_MAX 12

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode; /* wifi_auth_mode_t */
    uint8_t channel;
} web_scan_ap_t;

esp_err_t web_init(void);
esp_err_t web_wifi_restart(void);

/** Blocking scan (call from non-LVGL task or accept brief block). Returns count. */
int web_wifi_scan(web_scan_ap_t *out, int max_out);

/** Connect STA to ssid/password. Async; poll web_wifi_sta_ip. */
esp_err_t web_wifi_sta_connect(const char *ssid, const char *password);

/** SoftAP channel in use (after auto-select). */
uint8_t web_wifi_ap_channel(void);

/** STA connected? */
bool web_wifi_sta_connected(void);

/** Write STA IPv4 string "a.b.c.d" or empty if none. */
void web_wifi_sta_ip(char *buf, size_t buflen);

/** SoftAP password/ssid helpers already in config.h */
