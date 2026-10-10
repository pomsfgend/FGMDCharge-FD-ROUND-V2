#include "safe_ota.h"
#include "display.h"
#include "lzss.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SAFE_OTA";
static volatile int s_ota_pct = -1;

/* Spare flash above ota_1 (0x330000..0x400000). Header then LZSS of the factory image. */
#define SPARE_BASE  0x330000u
#define SPARE_SIZE  0xD0000u
#define BAK_MAGIC   0x314C5A46u /* FZL1 */
#define BAK_HDR     32u

int safe_ota_percent(void) { return s_ota_pct; }
void safe_ota_set_percent(int percent) { s_ota_pct = percent; }

static void ota_ui(int percent, const char *msg)
{
    s_ota_pct = percent;
    display_ota_progress(percent, msg);
}

static void ota_hide(void)
{
    s_ota_pct = -1;
    display_ota_hide();
}

static const esp_partition_t *part_sub(esp_partition_subtype_t sub)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, sub, NULL);
}

static void nvs_mig(uint8_t mig, uint8_t tries, uint32_t len, int write_len)
{
    nvs_handle_t h;
    if (nvs_open("cubeota", NVS_READWRITE, &h) != ESP_OK) return;
    (void)nvs_set_u8(h, "mig", mig);
    (void)nvs_set_u8(h, "try", tries);
    if (write_len) (void)nvs_set_u32(h, "len", len);
    (void)nvs_commit(h);
    nvs_close(h);
}

static void nvs_mig_get(uint8_t *mig, uint8_t *tries, uint32_t *len)
{
    nvs_handle_t h;
    *mig = 0;
    *tries = 0;
    *len = 0;
    if (nvs_open("cubeota", NVS_READONLY, &h) != ESP_OK) return;
    (void)nvs_get_u8(h, "mig", mig);
    (void)nvs_get_u8(h, "try", tries);
    (void)nvs_get_u32(h, "len", len);
    nvs_close(h);
}

static uint32_t image_len_of(const esp_partition_t *part)
{
    uint8_t hdr[24];
    uint8_t segs, i;
    uint32_t off;
    if (!part || esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK || hdr[0] != 0xE9) return 0;
    segs = hdr[1];
    if (segs == 0 || segs > 16) return 0;
    off = 24;
    for (i = 0; i < segs; i++) {
        uint8_t sh[8];
        uint32_t ln;
        if (esp_partition_read(part, off, sh, sizeof(sh)) != ESP_OK) return 0;
        ln = (uint32_t)sh[4] | ((uint32_t)sh[5] << 8) | ((uint32_t)sh[6] << 16) | ((uint32_t)sh[7] << 24);
        if (ln > part->size || off + 8u + ln > part->size) return 0;
        off += 8u + ln;
    }
    off += 1u;
    if (off & 15u) off += 16u - (off & 15u);
    if (hdr[23]) off += 32u;
    if (off > part->size || off < 24u) return 0;
    return off;
}

static int rd_part(void *ctx, uint32_t off, uint8_t *dst, uint32_t len)
{
    const esp_partition_t *part = (const esp_partition_t *)ctx;
    if (esp_partition_read(part, off, dst, len) != ESP_OK) return -1;
    return 0;
}

static int rd_spare(void *ctx, uint32_t off, uint8_t *dst, uint32_t len)
{
    uint32_t base = *(uint32_t *)ctx;
    if (esp_flash_read(esp_flash_default_chip, dst, base + off, len) != ESP_OK) return -1;
    return 0;
}

typedef struct {
    uint8_t sec[4096];
    uint32_t sec_base;
    int have;
    uint32_t max_len;
    int overflow;
} spare_wr_t;

static int flush_sec(spare_wr_t *w)
{
    uint32_t addr, n;
    if (!w->have) return 0;
    addr = SPARE_BASE + BAK_HDR + w->sec_base;
    if (w->sec_base >= w->max_len) return -1;
    n = 4096u;
    if (w->sec_base + n > w->max_len) n = w->max_len - w->sec_base;
    if (n & 3u) n = (n + 3u) & ~3u;
    if (n == 0 || n > 4096u || (addr & 3u) || (n & 3u)) return -1;
    if (SPARE_BASE + BAK_HDR + w->sec_base + n > SPARE_BASE + SPARE_SIZE) return -1;
    if (esp_flash_write(esp_flash_default_chip, w->sec, addr, n) != ESP_OK) return -1;
    w->have = 0;
    return 0;
}

static int wr_spare(void *ctx, uint32_t off, const uint8_t *src, uint32_t len)
{
    spare_wr_t *w = (spare_wr_t *)ctx;
    uint32_t i;
    if (off + len > w->max_len) {
        w->overflow = 1;
        return -1;
    }
    for (i = 0; i < len; i++) {
        uint32_t pos = off + i;
        uint32_t base = pos & ~4095u;
        if (!w->have || base != w->sec_base) {
            if (flush_sec(w) != 0) return -1;
            memset(w->sec, 0xFF, sizeof(w->sec));
            w->sec_base = base;
            w->have = 1;
        }
        w->sec[pos - base] = src[i];
    }
    return 0;
}

static int backup_crc_ok(uint32_t comp_len, uint32_t expect)
{
    uint8_t buf[1024];
    uint32_t off = 0, crc = 0;
    while (off < comp_len) {
        uint32_t n = comp_len - off;
        if (n > sizeof(buf)) n = sizeof(buf);
        if (esp_flash_read(esp_flash_default_chip, buf, SPARE_BASE + BAK_HDR + off, n) != ESP_OK) return 0;
        crc = lzss_crc32(buf, n, crc);
        off += n;
    }
    return crc == expect;
}

static int backup_valid(uint32_t *raw_len, uint32_t *comp_len, uint32_t *crc)
{
    uint8_t hdr[BAK_HDR];
    uint32_t magic, raw, comp, sum;
    if (esp_flash_read(esp_flash_default_chip, hdr, SPARE_BASE, sizeof(hdr)) != ESP_OK) return 0;
    memcpy(&magic, hdr + 0, 4);
    memcpy(&raw, hdr + 4, 4);
    memcpy(&comp, hdr + 8, 4);
    memcpy(&sum, hdr + 12, 4);
    if (magic != BAK_MAGIC || raw < 24u || raw > 0x190000u) return 0;
    if (comp < 16u || comp > (SPARE_SIZE - BAK_HDR)) return 0;
    if (!backup_crc_ok(comp, sum)) return 0;
    if (raw_len) *raw_len = raw;
    if (comp_len) *comp_len = comp;
    if (crc) *crc = sum;
    return 1;
}

static esp_err_t factory_backup_ensure(void)
{
    const esp_partition_t *ota0;
    uint32_t raw, comp = 0, crc = 0;
    uint8_t hdr[BAK_HDR];
    static spare_wr_t wr;
    if (backup_valid(NULL, NULL, NULL)) {
        ESP_LOGW(TAG, "factory backup already in spare");
        return ESP_OK;
    }
    ota0 = part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_0);
    if (!ota0) return ESP_ERR_NOT_FOUND;
    raw = image_len_of(ota0);
    if (raw < 24u) return ESP_ERR_INVALID_SIZE;
    ota_ui(1, "КОПИЯ");
    ESP_LOGW(TAG, "compress factory %u bytes", (unsigned)raw);
    if (esp_flash_erase_region(esp_flash_default_chip, SPARE_BASE, SPARE_SIZE) != ESP_OK) return ESP_FAIL;
    memset(&wr, 0, sizeof(wr));
    wr.max_len = SPARE_SIZE - BAK_HDR;
    if (lzss_compress(rd_part, (void *)ota0, raw, wr_spare, &wr, &comp, &crc) != 0 || wr.overflow) {
        ESP_LOGE(TAG, "factory backup does not fit");
        return ESP_ERR_NO_MEM;
    }
    if (flush_sec(&wr) != 0) return ESP_FAIL;
    uint32_t magic = BAK_MAGIC;
    memset(hdr, 0xFF, sizeof(hdr));
    memcpy(hdr + 0, &magic, 4);
    memcpy(hdr + 4, &raw, 4);
    memcpy(hdr + 8, &comp, 4);
    memcpy(hdr + 12, &crc, 4);
    if (esp_flash_write(esp_flash_default_chip, hdr, SPARE_BASE, sizeof(hdr)) != ESP_OK) return ESP_FAIL;
    if (!backup_valid(NULL, NULL, NULL)) return ESP_FAIL;
    ESP_LOGW(TAG, "factory backup %u -> %u", (unsigned)raw, (unsigned)comp);
    return ESP_OK;
}

typedef struct {
    esp_ota_handle_t h;
    uint32_t calls;
} ota_wr_t;

static int wr_ota(void *ctx, uint32_t off, const uint8_t *src, uint32_t len)
{
    ota_wr_t *w = (ota_wr_t *)ctx;
    (void)off;
    if (esp_ota_write(w->h, src, len) != ESP_OK) return -1;
    if ((++w->calls & 31u) == 0u) vTaskDelay(1);
    return 0;
}

/* Decompress the spare copy into a slot we are NOT running. */
static esp_err_t write_factory_to(const esp_partition_t *dst)
{
    uint32_t raw = 0, comp = 0, crc = 0, payload = SPARE_BASE + BAK_HDR;
    ota_wr_t wr;
    esp_ota_handle_t h = 0;
    esp_err_t err;
    if (!backup_valid(&raw, &comp, &crc)) return ESP_ERR_INVALID_CRC;
    if (!dst || raw > dst->size) return ESP_ERR_INVALID_SIZE;
    ESP_LOGW(TAG, "restore factory %u -> 0x%08lx", (unsigned)raw, (unsigned long)dst->address);
    err = esp_ota_begin(dst, raw, &h);
    if (err != ESP_OK) return err;
    wr.h = h;
    wr.calls = 0;
    if (lzss_decompress(rd_spare, &payload, comp, wr_ota, &wr, raw) != 0) {
        (void)esp_ota_abort(h);
        return ESP_FAIL;
    }
    err = esp_ota_end(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "factory verify failed: %s", esp_err_to_name(err));
    return err;
}

static esp_err_t restore_factory(void)
{
    return write_factory_to(part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_0));
}

static esp_err_t copy_image(const esp_partition_t *src, const esp_partition_t *dst, uint32_t len)
{
    esp_ota_handle_t h = 0;
    uint8_t buf[4096];
    uint32_t off = 0;
    esp_err_t err;
    if (len < 24u || len > dst->size) return ESP_ERR_INVALID_SIZE;
    err = esp_ota_begin(dst, len, &h);
    if (err != ESP_OK) return err;
    while (off < len) {
        uint32_t n = len - off;
        if (n > sizeof(buf)) n = sizeof(buf);
        err = esp_partition_read(src, off, buf, n);
        if (err != ESP_OK) { (void)esp_ota_abort(h); return err; }
        err = esp_ota_write(h, buf, n);
        if (err != ESP_OK) { (void)esp_ota_abort(h); return err; }
        off += n;
        if ((off & 0x7FFFu) == 0u) vTaskDelay(1);
    }
    return esp_ota_end(h);
}

void safe_ota_boot_continue(void)
{
    uint8_t mig = 0, tries = 0;
    uint32_t len = 0;
    const esp_partition_t *running, *ota0, *ota1;
    int on0, on1;
    nvs_mig_get(&mig, &tries, &len);
    if (mig == 0) return;
    running = esp_ota_get_running_partition();
    ota0 = part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_0);
    ota1 = part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_1);
    if (!running || !ota0 || !ota1) return;
    on0 = running->address == ota0->address;
    on1 = running->address == ota1->address;
    ESP_LOGW(TAG, "OTA mig=%u try=%u on=%s len=%u", mig, tries, on0 ? "ota_0" : "ota_1", (unsigned)len);

    if (mig == 1 && on0) {
        esp_err_t cerr;
        int attempt;
        if (len < 24u) len = image_len_of(running);
        cerr = ESP_FAIL;
        for (attempt = 0; attempt < 3; attempt++) {
            cerr = copy_image(running, ota1, len);
            if (cerr == ESP_OK) break;
            ESP_LOGE(TAG, "move to ota_1 failed: %s", esp_err_to_name(cerr));
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (cerr == ESP_OK && esp_ota_set_boot_partition(ota1) == ESP_OK) {
            nvs_mig(2, 0, len, 1);
            ESP_LOGW(TAG, "moved to ota_1, reboot to restore factory");
            esp_restart();
        }
        /* ota_1 was erased. Put the factory image there so the next power-on still boots. */
        ESP_LOGE(TAG, "OTA move failed, factory goes to ota_1");
        if (write_factory_to(ota1) == ESP_OK) {
            (void)esp_ota_set_boot_partition(ota1);
            nvs_mig(0, 0, 0, 1);
        }
        return;
    }
    if (mig == 1 && on1) {
        if (tries >= 1) {
            ESP_LOGW(TAG, "OTA retry failed, put factory back");
            (void)restore_factory();
            nvs_mig(0, 0, 0, 1);
            return;
        }
        nvs_mig(1, (uint8_t)(tries + 1), len, 0);
        if (esp_ota_set_boot_partition(ota0) == ESP_OK) esp_restart();
        return;
    }
    if (mig == 2 && on1) {
        if (restore_factory() == ESP_OK) {
            nvs_mig(0, 0, 0, 1);
            ESP_LOGW(TAG, "factory restored in ota_0");
        } else {
            ESP_LOGE(TAG, "factory restore failed");
        }
    }
}

esp_err_t safe_ota_write_stream(const uint8_t *first, size_t first_len,
                                int (*read_cb)(uint8_t *buf, size_t max_len, void *ctx),
                                void *ctx, size_t total_len)
{
    const esp_partition_t *running, *ota0, *ota1, *dest;
    esp_ota_handle_t h = 0;
    esp_err_t err;
    size_t written = 0;
    int shown = -1;
    uint8_t buf[4096];
    int to_factory_slot;

    if (!read_cb || total_len < 24U || first_len == 0U || first_len > total_len || !first) return ESP_ERR_INVALID_ARG;
    if (first[0] != 0xE9) return ESP_ERR_INVALID_ARG;

    running = esp_ota_get_running_partition();
    ota0 = part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_0);
    ota1 = part_sub(ESP_PARTITION_SUBTYPE_APP_OTA_1);
    if (!running || !ota0 || !ota1) return ESP_ERR_NOT_FOUND;
    /* Never erase the factory slot until its LZSS copy is in the spare flash.
       The running app is ota_1, so the new image lands in ota_0 for one reboot,
       then safe_ota_boot_continue moves it back and restores the factory. */
    if (running->address == ota0->address) {
        dest = ota1;
        to_factory_slot = 0;
    } else {
        dest = ota0;
        to_factory_slot = 1;
    }
    if (total_len > dest->size) return ESP_ERR_INVALID_SIZE;
    if (to_factory_slot) {
        err = factory_backup_ensure();
        if (err != ESP_OK) {
            ota_hide();
            return err;
        }
    }

    ESP_LOGI(TAG, "OTA dest 0x%08lx image=%lu", (unsigned long)dest->address, (unsigned long)total_len);
    err = esp_ota_begin(dest, total_len, &h);
    if (err != ESP_OK) {
        if (to_factory_slot) (void)restore_factory();
        ota_hide();
        return err;
    }
    ota_ui(0, "OTA");
    err = esp_ota_write(h, first, first_len);
    if (err != ESP_OK) goto fail;
    written = first_len;

    while (written < total_len) {
        size_t want = total_len - written;
        int n;
        if (want > sizeof(buf)) want = sizeof(buf);
        n = read_cb(buf, want, ctx);
        if (n <= 0 || (size_t)n > want) {
            err = ESP_ERR_INVALID_SIZE;
            goto fail;
        }
        err = esp_ota_write(h, buf, (size_t)n);
        if (err != ESP_OK) goto fail;
        written += (size_t)n;
        {
            int pct = (int)(written * 100 / total_len);
            if (pct != shown) {
                shown = pct;
                ota_ui(pct, "OTA");
            }
        }
    }

    err = esp_ota_end(h);
    h = 0;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA verify failed: %s", esp_err_to_name(err));
        if (to_factory_slot) (void)restore_factory();
        ota_hide();
        return err;
    }
    err = esp_ota_set_boot_partition(dest);
    if (err != ESP_OK) {
        if (to_factory_slot) (void)restore_factory();
        ota_hide();
        return err;
    }
    if (to_factory_slot) nvs_mig(1, 0, (uint32_t)total_len, 1);
    else nvs_mig(0, 0, 0, 1);
    ESP_LOGW(TAG, "OTA stored (%lu), reboot", (unsigned long)written);
    ota_ui(100, "REBOOT");
    return ESP_OK;

fail:
    if (h) (void)esp_ota_abort(h);
    if (to_factory_slot) (void)restore_factory();
    ota_hide();
    return err;
}
