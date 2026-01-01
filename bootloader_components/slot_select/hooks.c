#include <string.h>
#include "esp_rom_gpio.h"
#include "esp_rom_crc.h"
#include "bootloader_flash.h"
#include "soc/gpio_struct.h"
#include "hal/gpio_ll.h"

/* GPIO0 to GND at power-on boots the original slot. Released boots our slot. */
#define SELECT_GPIO 0
#define OTADATA 0xD000

static void write_slot(int slot)
{
    uint32_t seq = (uint32_t)slot + 1;
    uint32_t crc = esp_rom_crc32_le(0xFFFFFFFF, (uint8_t *)&seq, 4);
    uint8_t entry[32];
    memset(entry, 0xFF, sizeof(entry));
    memcpy(entry, &seq, 4);
    memcpy(entry + 28, &crc, 4);
    uint8_t page[64];
    memcpy(page, entry, 32);
    memcpy(page + 32, entry, 32);
    bootloader_flash_erase_range(OTADATA, 0x1000);
    bootloader_flash_write(OTADATA, page, sizeof(page), false);
}

void bootloader_after_init(void)
{
    esp_rom_gpio_pad_select_gpio(SELECT_GPIO);
    gpio_ll_input_enable(&GPIO, SELECT_GPIO);
    gpio_ll_pullup_en(&GPIO, SELECT_GPIO);
    gpio_ll_pulldown_dis(&GPIO, SELECT_GPIO);
    int low = gpio_ll_get_level(&GPIO, SELECT_GPIO) == 0;
    write_slot(low ? 0 : 1);
}
