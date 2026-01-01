#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t cube_ble_init(void);
esp_err_t cube_ble_send_status(const char *text);
bool cube_ble_is_connected(void);
void cube_ble_task(void *arg);
