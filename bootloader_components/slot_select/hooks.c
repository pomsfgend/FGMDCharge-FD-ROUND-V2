#include <string.h>
#include "esp_rom_sys.h"
#include "esp_rom_crc.h"
#include "bootloader_flash_priv.h"

void bootloader_reset(void);

#define OTADATA 0xD000
#define CHOICE ((volatile uint32_t *)0x50001FF0)
#define MAGIC_OLD 0x01D0F00Du
#define MAGIC_FORCED 0x0F0C3D01u

static int entry_ok(const uint8_t *e, uint32_t *seq_out)
{
    uint32_t seq, crc, c4, c28;
    memcpy(&seq, e, 4);
    memcpy(&crc, e + 28, 4);
    if (seq == 0 || seq == 0xFFFFFFFFu) return 0;
    c4 = esp_rom_crc32_le(0xFFFFFFFF, e, 4);
    c28 = esp_rom_crc32_le(0xFFFFFFFF, e, 28);
    if (crc != c4 && crc != c28) return 0;
    *seq_out = seq;
    return 1;
}

static uint32_t active_seq(int *slot_out)
{
    uint8_t sec[64];
    uint32_t best = 0;
    int slot = 0;
    bootloader_flash_read(OTADATA, sec, sizeof(sec), false);
    for (int i = 0; i < 2; i++) {
        uint32_t seq = 0;
        if (!entry_ok(sec + i * 32, &seq)) continue;
        if (seq >= best) {
            best = seq;
            slot = (int)((seq - 1) & 1);
        }
    }
    *slot_out = slot;
    return best;
}

static void write_slot(int slot, uint32_t prev)
{
    uint32_t seq = prev + 1;
    if (seq < 1) seq = 1;
    if (((seq - 1) & 1) != (uint32_t)slot) seq++;
    uint8_t entry[32];
    memset(entry, 0xFF, sizeof(entry));
    memcpy(entry, &seq, 4);
    uint32_t crc = esp_rom_crc32_le(0xFFFFFFFF, entry, 28);
    memcpy(entry + 28, &crc, 4);
    uint8_t page[64];
    memcpy(page, entry, 32);
    memcpy(page + 32, entry, 32);
    bootloader_flash_erase_range(OTADATA, 0x1000);
    bootloader_flash_write(OTADATA, page, sizeof(page), false);
    esp_rom_printf("BOOT wrote seq %u slot %d\n", (unsigned)seq, slot);
}

void bootloader_after_init(void)
{
    int slot = 0;
    uint32_t seq = active_seq(&slot);
    esp_rom_printf("BOOT choice %08x seq %u slot %d\n", (unsigned)*CHOICE, (unsigned)seq, slot);
    if (*CHOICE == MAGIC_OLD) {
        esp_rom_printf("BOOT keep old until power off\n");
        return;
    }
    if (*CHOICE == MAGIC_FORCED) {
        esp_rom_printf("BOOT menu already forced\n");
        return;
    }
    if (slot != 1) {
        *CHOICE = MAGIC_FORCED;
        write_slot(1, seq);
        esp_rom_printf("BOOT power -> menu, reset\n");
        bootloader_reset();
    }
}
