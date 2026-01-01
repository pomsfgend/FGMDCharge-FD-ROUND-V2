#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t safe_ota_write_stream(const uint8_t *first, size_t first_len,
                                int (*read_cb)(uint8_t *buf, size_t max_len, void *ctx),
                                void *ctx, size_t total_len);
