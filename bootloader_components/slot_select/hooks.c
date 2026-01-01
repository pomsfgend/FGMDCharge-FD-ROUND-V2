#include <string.h>
#include "esp_rom_sys.h"
#include "esp_rom_crc.h"
#include "bootloader_flash_priv.h"

void bootloader_reset(void);
int esp_rom_get_reset_reason(int cpu_no);

#define OTADATA 0xD000

static int entry_ok(const uint8_t *e, uint32_t *seq_out)
{
    uint32_t seq, crc, c4, c28;
    memcpy(&seq, e, 4);
    memcpy(&crc, e + 28, 4);
    if (seq == 0 || seq == 0xFFFFFFFFu) return 0;
    c4 = esp_rom_crc32_le(0xFFFFFFFF, (const uint8_t *)&seq, 4);
    c28 = esp_rom_crc32_le(0xFFFFFFFF, e, 28);
    if (crc != c4 && crc != c28) return 0;
    *seq_out = seq;
    return 1;
}

static uint32_t active_seq(int *slot_out)
{
    uint32_t best = 0;
    int slot = 0;
    for (int sec = 0; sec < 2; sec++) {
        uint8_t e[32];
        bootloader_flash_read(OTADATA + sec * 0x1000, e, sizeof(e), false);
        uint32_t seq = 0;
        if (!entry_ok(e, &seq)) continue;
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
    uint32_t crc = esp_rom_crc32_le(0xFFFFFFFF, (uint8_t *)&seq, 4);
    memcpy(entry + 28, &crc, 4);
    for (int sec = 0; sec < 2; sec++) {
        bootloader_flash_erase_range(OTADATA + sec * 0x1000, 0x1000);
        bootloader_flash_write(OTADATA + sec * 0x1000, entry, sizeof(entry), false);
    }
    esp_rom_printf("BOOT wrote seq %u slot %d both sectors\n", (unsigned)seq, slot);
}

void bootloader_after_init(void)
{
    int slot = 0;
    uint32_t seq = active_seq(&slot);
    int reason = esp_rom_get_reset_reason(0);
    esp_rom_printf("BOOT reason %d seq %u slot %d\n", reason, (unsigned)seq, slot);
    /* 1 = power on. Only then force our menu. A software reset keeps the slot
       the button just wrote, so old firmware can actually start. */
    if (reason == 1 && slot != 1) {
        write_slot(1, seq);
        esp_rom_printf("BOOT power -> menu, reset\n");
        bootloader_reset();
    }
}
