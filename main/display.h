#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t display_init(void);
void display_task(void *arg);
void display_set_values(void);
bool display_lock(void);
void display_unlock(void);
void display_set_brightness(int percent); /* 5..100 */
int display_get_brightness(void);
