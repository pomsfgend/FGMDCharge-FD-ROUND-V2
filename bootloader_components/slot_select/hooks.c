#include <string.h>
#include "esp_rom_crc.h"
#include "bootloader_flash.h"

/* Three power-ons toggle the slot. No GPIO. Marks are 4 bytes so the
   bootloader flash writer accepts them. */
#define OTADATA 0xD000
#define COUNT_AT 0xE800

static int current_slot(void)
{
    uint32_t seq = 0xFFFFFFFF;
    bootloader_flash_read(OTADATA, &seq, 4, false);
    if (seq == 0 || seq == 0xFFFFFFFF) return 1;
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
    uint32_t marks[2];
    bootloader_flash_read(COUNT_AT, marks, 8, false);
    int n = (marks[0] == 0) + (marks[0] == 0 && marks[1] == 0);
    if (n >= 2) {
        write_slot(current_slot() ^ 1);
        bootloader_flash_erase_range(0xE000, 0x1000);
        return;
    }
    uint32_t zero = 0;
    bootloader_flash_write(COUNT_AT + (n * 4), &zero, 4, false);
}
