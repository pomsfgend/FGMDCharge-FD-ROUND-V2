#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t diag_init(void);
bool diag_get_temperature(float *out_c);
uint8_t diag_get_cpu_load_percent(void);
