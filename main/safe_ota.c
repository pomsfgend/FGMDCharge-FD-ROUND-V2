#include "safe_ota.h"
#include "display.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SAFE_OTA";
static volatile int s_ota_pct = -1;

int safe_ota_percent(void) { return s_ota_pct; }
void safe_ota_set_percent(int percent) { s_ota_pct = percent; }

static void ota_ui(int percent, const char *msg)
{
    s_ota_pct = percent;
    display_ota_progress(percent, msg);
}

esp_err_t safe_ota_write_stream(const uint8_t *first, size_t first_len,
                                int (*read_cb)(uint8_t *buf, size_t max_len, void *ctx),
                                void *ctx, size_t total_len) {
    if (!read_cb || total_len < 24U || first_len == 0U || first_len > total_len || !first) {
        return ESP_ERR_INVALID_ARG;
    }
    if (first[0] != 0xE9) return ESP_ERR_INVALID_ARG;

    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (part && running && part->address == running->address) {
        return ESP_ERR_INVALID_STATE; /* running modified slot; USB flash only */
    }
    if (!part) return ESP_ERR_NOT_FOUND;
    if (part->type != ESP_PARTITION_TYPE_APP || total_len > part->size) return ESP_ERR_INVALID_SIZE;

    ESP_LOGI(TAG, "OTA target: subtype=%d offset=0x%08lx size=0x%08lx image=%lu",
             part->subtype, (unsigned long)part->address,
             (unsigned long)part->size, (unsigned long)total_len);

    esp_ota_handle_t h = 0;
    esp_err_t err = esp_ota_begin(part, total_len, &h);
    if (err != ESP_OK) return err;
    ota_ui(0, "OTA");

    size_t written = 0;
    int shown = -1;
    if (first_len) {
        err = esp_ota_write(h, first, first_len);
        if (err != ESP_OK) goto fail;
        written = first_len;
    }

    uint8_t buf[4096];
    while (written < total_len) {
        size_t want = total_len - written;
        if (want > sizeof(buf)) want = sizeof(buf);
        int n = read_cb(buf, want, ctx);
        if (n <= 0 || (size_t)n > want) {
            err = ESP_ERR_INVALID_SIZE;
            goto fail;
        }
        err = esp_ota_write(h, buf, (size_t)n);
        if (err != ESP_OK) goto fail;
        written += (size_t)n;
        int pct = (int)(written * 100 / total_len);
        if (pct != shown) {
            shown = pct;
            ota_ui(pct, "OTA");
        }
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA verification failed: %s", esp_err_to_name(err));
        ota_ui(0, "OTA FAIL");
        s_ota_pct = -1;
        return err;
    }
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "OTA complete (%lu bytes); boot partition changed", (unsigned long)written);
    ota_ui(100, "REBOOT");
    return ESP_OK;

fail:
    ota_ui(0, "OTA FAIL");
    s_ota_pct = -1;
    esp_ota_abort(h);
    return err;
}
