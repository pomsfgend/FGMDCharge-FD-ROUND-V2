#pragma once
#include "esp_err.h"
esp_err_t web_init(void);
esp_err_t web_wifi_restart(void);
int web_get_ap_channel(void);
esp_err_t web_scan_now(void);
int web_scan_count(void);
const char *web_scan_ssid(int index);
esp_err_t web_sta_join(const char *ssid, const char *password);
const char *web_sta_ip(void);
