#include <string.h>
#include "esp_rom_crc.h"
#include "esp_rom_spiflash.h"

/* Three power-ons toggle ota_0/ota_1. ROM flash API, no GPIO. */
#define OTADATA 0xD000
#define COUNT_AT 0xE800

static int current_slot(void)
{
    uint32_t seq = 0xFFFFFFFF;
    esp_rom_spiflash_read(OTADATA, &seq, 4);
    if (seq == 0 || seq == 0xFFFFFFFF) return 1;
    return (int)((seq - 1) & 1);
}

static void write_slot(int slot)
{
    uint32_t seq = (uint32_t)slot + 1;
    uint32_t crc = esp_rom_crc32_le(0xFFFFFFFF, (uint8_t *)&seq, 4);
    uint32_t page[16];
    memset(page, 0xFF, sizeof(page));
    page[0] = seq;
    page[7] = crc;
    page[8] = seq;
    page[15] = crc;
    esp_rom_spiflash_erase_sector(OTADATA / 0x1000);
    esp_rom_spiflash_write(OTADATA, page, sizeof(page));
}

void bootloader_after_init(void)
{
    uint32_t marks[2] = {0xFFFFFFFF, 0xFFFFFFFF};
    esp_rom_spiflash_read(COUNT_AT, marks, 8);
    int n = (marks[0] == 0) + (marks[0] == 0 && marks[1] == 0);
    if (n >= 2) {
        write_slot(current_slot() ^ 1);
        esp_rom_spiflash_erase_sector(0xE000 / 0x1000);
        return;
    }
    uint32_t zero = 0;
    esp_rom_spiflash_write(COUNT_AT + (n * 4), &zero, 4);
}
