#pragma once
#include "esp_err.h"
esp_err_t web_init(void);
esp_err_t web_wifi_restart(void);
int web_get_ap_channel(void);
