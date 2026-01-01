#include <string.h>
#include "esp_rom_sys.h"
#include "esp_rom_crc.h"
#include "bootloader_flash_priv.h"
#include "soc/rtc_cntl_reg.h"

#define OTADATA 0xD000

static int current_slot(void)
{
    uint32_t seq = 0xFFFFFFFF;
    bootloader_flash_read(OTADATA, &seq, 4, false);
    if (seq == 0 || seq == 0xFFFFFFFF) return 0;
    return (int)((seq - 1) & 1);
}

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
    uint32_t reason = REG_READ(RTC_CNTL_RESET_STATE_REG) & 0x3Fu;
    int slot = current_slot();
    esp_rom_printf("BOOT reason %u slot %d\n", (unsigned)reason, slot);
    /* Power-on always lands on our menu (slot 1). A software reset keeps the chosen slot. */
    if (reason == 1u && slot != 1) {
        write_slot(1);
        esp_rom_printf("BOOT power-on -> our menu, reset\n");
        bootloader_reset();
    }
}
