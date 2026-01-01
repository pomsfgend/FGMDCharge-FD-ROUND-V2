/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_ili9341.h"

static const char *TAG = "ili9341";

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const ili9341_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} ili9341_panel_t;

static esp_err_t panel_ili9341_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9341_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data);
static esp_err_t panel_ili9341_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_ili9341_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ili9341_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_ili9341_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_ili9341_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_ili9341(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ili9341_panel_t *ili9341 = NULL;
    gpio_config_t io_conf = {0};

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    ili9341 = calloc(1, sizeof(*ili9341));
    ESP_GOTO_ON_FALSE(ili9341, ESP_ERR_NO_MEM, err, TAG, "no mem for ili9341 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    /* ESP-IDF 5.0.x uses rgb_endian in esp_lcd_panel_dev_config_t. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        ili9341->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        ili9341->madctl_val = LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }
#else
    ili9341->madctl_val = 0;
#endif

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        ili9341->colmod_val = 0x55;
        ili9341->fb_bits_per_pixel = 16;
        break;
    case 18:
        ili9341->colmod_val = 0x66;
        ili9341->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    ili9341->io = io;
    ili9341->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ili9341->reset_level = panel_dev_config->flags.reset_active_high;

    if (panel_dev_config->vendor_config) {
        const ili9341_vendor_config_t *vendor = panel_dev_config->vendor_config;
        ili9341->init_cmds = vendor->init_cmds;
        ili9341->init_cmds_size = vendor->init_cmds_size;
    }

    ili9341->base.del = panel_ili9341_del;
    ili9341->base.reset = panel_ili9341_reset;
    ili9341->base.init = panel_ili9341_init;
    ili9341->base.draw_bitmap = panel_ili9341_draw_bitmap;
    ili9341->base.invert_color = panel_ili9341_invert_color;
    ili9341->base.set_gap = panel_ili9341_set_gap;
    ili9341->base.mirror = panel_ili9341_mirror;
    ili9341->base.swap_xy = panel_ili9341_swap_xy;
    ili9341->base.disp_on_off = panel_ili9341_disp_on_off;
    *ret_panel = &ili9341->base;

    ESP_LOGD(TAG, "new ili9341 panel @%p", ili9341);
    ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d",
             ESP_LCD_ILI9341_VER_MAJOR, ESP_LCD_ILI9341_VER_MINOR, ESP_LCD_ILI9341_VER_PATCH);
    return ESP_OK;

err:
    if (ili9341) {
        if (panel_dev_config && panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ili9341);
    }
    return ret;
}

static esp_err_t panel_ili9341_del(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    if (ili9341->reset_gpio_num >= 0) {
        gpio_reset_pin(ili9341->reset_gpio_num);
    }
    free(ili9341);
    return ESP_OK;
}

static esp_err_t panel_ili9341_reset(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    if (ili9341->reset_gpio_num >= 0) {
        gpio_set_level(ili9341->reset_gpio_num, ili9341->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ili9341->reset_gpio_num, !ili9341->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ili9341->io, LCD_CMD_SWRESET, NULL, 0),
                            TAG, "send software reset failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}

static const ili9341_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xCF, (uint8_t[]){0x00, 0xAA, 0xE0}, 3, 0},
    {0xED, (uint8_t[]){0x67, 0x03, 0x12, 0x81}, 4, 0},
    {0xE8, (uint8_t[]){0x8A, 0x01, 0x78}, 3, 0},
    {0xCB, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    {0xF7, (uint8_t[]){0x20}, 1, 0},
    {0xEA, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xC0, (uint8_t[]){0x23}, 1, 0},
    {0xC1, (uint8_t[]){0x11}, 1, 0},
    {0xC5, (uint8_t[]){0x43, 0x4C}, 2, 0},
    {0xC7, (uint8_t[]){0xA0}, 1, 0},
    {0xB1, (uint8_t[]){0x00, 0x1B}, 2, 0},
    {0xF2, (uint8_t[]){0x00}, 1, 0},
    {0x26, (uint8_t[]){0x01}, 1, 0},
    {0xE0, (uint8_t[]){0x1F, 0x36, 0x36, 0x3A, 0x0C, 0x05, 0x4F, 0x87, 0x3C, 0x08, 0x11, 0x35, 0x19, 0x13, 0x00}, 15, 0},
    {0xE1, (uint8_t[]){0x00, 0x09, 0x09, 0x05, 0x13, 0x0A, 0x30, 0x78, 0x43, 0x07, 0x0E, 0x0A, 0x26, 0x2C, 0x1F}, 15, 0},
    {0xB7, (uint8_t[]){0x07}, 1, 0},
    {0xB6, (uint8_t[]){0x08, 0x82, 0x27}, 3, 0},
};

static esp_err_t panel_ili9341_init(esp_lcd_panel_t *panel)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9341->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send sleep-out failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &ili9341->madctl_val, 1),
                        TAG, "set MADCTL failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, &ili9341->colmod_val, 1),
                        TAG, "set COLMOD failed");

    const ili9341_lcd_init_cmd_t *init_cmds = ili9341->init_cmds;
    uint16_t init_cmds_size = ili9341->init_cmds_size;
    if (!init_cmds) {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = (uint16_t)(sizeof(vendor_specific_init_default) / sizeof(vendor_specific_init_default[0]));
    }

    for (uint16_t i = 0; i < init_cmds_size; ++i) {
        if (init_cmds[i].cmd == LCD_CMD_MADCTL && init_cmds[i].data) {
            ili9341->madctl_val = ((const uint8_t *)init_cmds[i].data)[0];
        } else if (init_cmds[i].cmd == LCD_CMD_COLMOD && init_cmds[i].data) {
            ili9341->colmod_val = ((const uint8_t *)init_cmds[i].data)[0];
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd,
                                                       init_cmds[i].data, init_cmds[i].data_bytes),
                            TAG, "send init command failed");
        if (init_cmds[i].delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }
    return ESP_OK;
}

static esp_err_t panel_ili9341_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                           int x_end, int y_end, const void *color_data)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    assert(x_start < x_end && y_start < y_end);

    x_start += ili9341->x_gap;
    x_end += ili9341->x_gap;
    y_start += ili9341->y_gap;
    y_end += ili9341->y_gap;

    uint8_t col_data[4] = {
        (uint8_t)((x_start >> 8) & 0xFF), (uint8_t)(x_start & 0xFF),
        (uint8_t)(((x_end - 1) >> 8) & 0xFF), (uint8_t)((x_end - 1) & 0xFF)};
    uint8_t row_data[4] = {
        (uint8_t)((y_start >> 8) & 0xFF), (uint8_t)(y_start & 0xFF),
        (uint8_t)(((y_end - 1) >> 8) & 0xFF), (uint8_t)((y_end - 1) & 0xFF)};

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ili9341->io, LCD_CMD_CASET, col_data, sizeof(col_data)),
                        TAG, "set column address failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ili9341->io, LCD_CMD_RASET, row_data, sizeof(row_data)),
                        TAG, "set row address failed");

    size_t len = (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * ili9341->fb_bits_per_pixel / 8U;
    return esp_lcd_panel_io_tx_color(ili9341->io, LCD_CMD_RAMWR, color_data, len);
}

static esp_err_t panel_ili9341_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    return esp_lcd_panel_io_tx_param(ili9341->io, command, NULL, 0);
}

static esp_err_t panel_ili9341_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    if (mirror_x) {
        ili9341->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        ili9341->madctl_val &= (uint8_t)~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        ili9341->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        ili9341->madctl_val &= (uint8_t)~LCD_CMD_MY_BIT;
    }
    return esp_lcd_panel_io_tx_param(ili9341->io, LCD_CMD_MADCTL, &ili9341->madctl_val, 1);
}

static esp_err_t panel_ili9341_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    if (swap_axes) {
        ili9341->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        ili9341->madctl_val &= (uint8_t)~LCD_CMD_MV_BIT;
    }
    return esp_lcd_panel_io_tx_param(ili9341->io, LCD_CMD_MADCTL, &ili9341->madctl_val, 1);
}

static esp_err_t panel_ili9341_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    ili9341->x_gap = x_gap;
    ili9341->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ili9341_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ili9341_panel_t *ili9341 = __containerof(panel, ili9341_panel_t, base);
    return esp_lcd_panel_io_tx_param(ili9341->io, on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
}
