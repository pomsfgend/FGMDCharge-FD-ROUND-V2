#include "session_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_flash.h"
#include "esp_log.h"

#include "app_state.h"
#include "can_gbt.h"
#include "config.h"

static const char *TAG = "LOG";
static SemaphoreHandle_t s_mu;
static int s_pending;
static int s_pending_seal;
static int s_pending_reason;
static int s_open = -1;
static uint16_t s_open_seq;
static uint8_t s_page[CUBE_LOG_SLOT_SIZE] __attribute__((aligned(4)));
static char s_text[3072];

#define LOG_MAGIC 0x474F4C43u
#define LOG_HDR   16

static void put_u16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put_u32(uint8_t *p, unsigned v)
{
    put_u16(p, v);
    put_u16(p + 2, (unsigned)(v >> 16));
}

static unsigned get_u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned get_u32(const uint8_t *p)
{
    return get_u16(p) | (get_u16(p + 2) << 16);
}

static uint32_t slot_addr(int index)
{
    return (uint32_t)CUBE_LOG_FLASH_BASE + (uint32_t)index * (uint32_t)CUBE_LOG_SLOT_SIZE;
}

static const char *proto_tag(int protocol)
{
    if (protocol == 1) return "2015";
    if (protocol == 2) return "2011";
    if (protocol == 3) return "2023";
    return "auto";
}

static const char *reason_ru(int reason)
{
    if (reason == SESSION_LOG_PROTO) return "станция опознана";
    if (reason == SESSION_LOG_STOP) return "сессия закончилась";
    return "обновление";
}

static const char *reason_en(int reason)
{
    if (reason == SESSION_LOG_PROTO) return "proto";
    if (reason == SESSION_LOG_STOP) return "stop";
    return "upd";
}

typedef struct {
    int index;
    unsigned seq;
    unsigned len;
    int protocol;
    int reason;
    int sealed;
    int ok;
} slot_rec_t;

static int read_slot_hdr(int index, slot_rec_t *out)
{
    uint8_t hdr[LOG_HDR];
    memset(out, 0, sizeof(*out));
    out->index = index;
    if (esp_flash_read(NULL, hdr, slot_addr(index), sizeof(hdr)) != ESP_OK) return 0;
    if (get_u32(hdr) != LOG_MAGIC) return 0;
    out->seq = get_u16(hdr + 4);
    out->len = get_u16(hdr + 6);
    out->protocol = hdr[8];
    out->reason = hdr[9];
    out->sealed = hdr[10] ? 1 : 0;
    if (out->len == 0 || out->len > (unsigned)(CUBE_LOG_SLOT_SIZE - LOG_HDR)) return 0;
    out->ok = 1;
    return 1;
}

static int collect(slot_rec_t *slots)
{
    int n = 0;
    for (int i = 0; i < CUBE_LOG_SLOTS; ++i) {
        slot_rec_t rec;
        if (!read_slot_hdr(i, &rec)) continue;
        slots[n++] = rec;
    }
    for (int i = 1; i < n; ++i) {
        slot_rec_t key = slots[i];
        int j = i - 1;
        while (j >= 0 && slots[j].seq < key.seq) {
            slots[j + 1] = slots[j];
            --j;
        }
        slots[j + 1] = key;
    }
    return n;
}

static int add_fmt(int o, const char *fmt, ...)
{
    va_list ap;
    int room;
    int w;
    if (o < 0) o = 0;
    if (o >= (int)sizeof(s_text) - 1) return (int)sizeof(s_text) - 1;
    room = (int)sizeof(s_text) - o;
    va_start(ap, fmt);
    w = vsnprintf(s_text + o, (size_t)room, fmt, ap);
    va_end(ap);
    if (w < 0) return o;
    if (w >= room) return (int)sizeof(s_text) - 1;
    return o + w;
}
static int compose(int reason)
{
    cube_state_snapshot_t st;
    cube_can_frame_t frames[16];
    size_t nframes = 0;
    int o = 0;
    int ev;
    app_state_snapshot(&st);
    if (gbt_can_get_history(frames, 16, &nframes) != ESP_OK) nframes = 0;
    o = add_fmt(0,
                "FD_ROUND_V2 %s\nпричина: %s\nпротокол: %s\nрежим: %s\nпередача CAN: %s\n"
                "VIN: %s\nсостояние: %s\nприём: %lu\nотправка: %lu\n--- события ---\n",
                CUBE_FW_VERSION, reason_ru(reason), cube_protocol_name(st.protocol),
                app_state_protocol_locked() ? "выбран вручную" : "авто",
                st.tx_enabled ? "включена" : "выключена",
                app_state_vin(), cube_main_state_name(st.main_state),
                (unsigned long)st.rx_count, (unsigned long)st.tx_count);
    ev = app_state_log_text(s_text + o, (int)sizeof(s_text) - o);
    if (ev > 0) o += ev;
    if (o > (int)sizeof(s_text) - 1) o = (int)sizeof(s_text) - 1;
    o = add_fmt(o, "--- CAN ---\n");
    if (nframes == 0) {
        o = add_fmt(o, "кадров нет\n");
    }
    for (size_t i = 0; i < nframes; ++i) {
        o = add_fmt(o, "%lu %08lX %u",
                    (unsigned long)frames[i].timestamp_ms, (unsigned long)frames[i].id,
                    (unsigned)frames[i].dlc);
        for (uint8_t b = 0; b < frames[i].dlc; ++b) o = add_fmt(o, " %02X", frames[i].data[b]);
        o = add_fmt(o, "\n");
    }
    if (o < 0) return 0;
    if (o >= (int)sizeof(s_text)) o = (int)sizeof(s_text) - 1;
    s_text[o] = 0;
    return o;
}

static int alloc_slot(void)
{
    slot_rec_t slots[CUBE_LOG_SLOTS];
    int n = collect(slots);
    unsigned best = 0;
    int best_i = -1;
    for (int i = 0; i < n; ++i) {
        if (slots[i].seq >= best) {
            best = slots[i].seq;
            best_i = slots[i].index;
        }
    }
    s_open_seq = (uint16_t)(best + 1u);
    if (s_open_seq == 0) s_open_seq = 1;
    s_open = (best_i < 0) ? 0 : (best_i + 1) % CUBE_LOG_SLOTS;
    return s_open;
}

static int write_open(int reason, int seal, int protocol)
{
    int len = compose(reason);
    int slot;
    if (len <= 0) return 0;
    if (len > CUBE_LOG_SLOT_SIZE - LOG_HDR) len = CUBE_LOG_SLOT_SIZE - LOG_HDR;
    if (s_open < 0) alloc_slot();
    slot = s_open;
    if (slot < 0 || slot >= CUBE_LOG_SLOTS) return 0;
    memset(s_page, 0xFF, sizeof(s_page));
    put_u32(s_page, LOG_MAGIC);
    put_u16(s_page + 4, s_open_seq);
    put_u16(s_page + 6, (unsigned)len);
    s_page[8] = (uint8_t)(protocol & 0xFF);
    s_page[9] = (uint8_t)(reason & 0xFF);
    s_page[10] = seal ? 1u : 0u;
    s_page[11] = app_state_protocol_locked() ? 1u : 0u;
    memcpy(s_page + LOG_HDR, s_text, (size_t)len);
    if (esp_flash_erase_region(NULL, slot_addr(slot), CUBE_LOG_SLOT_SIZE) != ESP_OK) {
        ESP_LOGW(TAG, "erase slot %d failed", slot);
        return 0;
    }
    if (esp_flash_write(NULL, s_page, slot_addr(slot), CUBE_LOG_SLOT_SIZE) != ESP_OK) {
        ESP_LOGW(TAG, "write slot %d failed", slot);
        return 0;
    }
    if (seal) s_open = -1;
    return 1;
}

void session_log_init(void)
{
    if (!s_mu) s_mu = xSemaphoreCreateMutex();
    s_open = -1;
    s_pending = 0;
    s_pending_seal = 0;
    s_pending_reason = 0;
}

void session_log_request(int seal, int reason)
{
    if (!s_mu) return;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    s_pending = 1;
    if (seal) s_pending_seal = 1;
    if (reason > s_pending_reason) s_pending_reason = reason;
    (void)xSemaphoreGive(s_mu);
}

void session_log_service(void)
{
    int seal;
    int reason;
    cube_state_snapshot_t st;
    if (!s_mu) return;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    if (!s_pending) {
        (void)xSemaphoreGive(s_mu);
        return;
    }
    seal = s_pending_seal;
    reason = s_pending_reason;
    s_pending = 0;
    s_pending_seal = 0;
    s_pending_reason = 0;
    app_state_snapshot(&st);
    if (st.rx_count == 0 && s_open < 0) {
        (void)xSemaphoreGive(s_mu);
        return;
    }
    (void)write_open(reason, seal, (int)st.protocol);
    (void)xSemaphoreGive(s_mu);
}

void session_log_on_second(void)
{
    static unsigned tick;
    cube_state_snapshot_t st;
    tick++;
    app_state_snapshot(&st);
    if ((tick % 30u) == 0u && st.rx_count > 0 && (st.session_active || st.protocol != 0)) {
        session_log_request(0, SESSION_LOG_UPDATE);
    }
    session_log_service();
}

int session_log_count(void)
{
    slot_rec_t slots[CUBE_LOG_SLOTS];
    int n;
    if (!s_mu) return 0;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    n = collect(slots);
    (void)xSemaphoreGive(s_mu);
    return n;
}

int session_log_info(int newest_index, int *seq, int *nbytes, int *protocol, int *reason)
{
    slot_rec_t slots[CUBE_LOG_SLOTS];
    int n;
    if (!s_mu) return 0;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    n = collect(slots);
    if (newest_index < 0 || newest_index >= n) {
        (void)xSemaphoreGive(s_mu);
        return 0;
    }
    if (seq) *seq = (int)slots[newest_index].seq;
    if (nbytes) *nbytes = (int)slots[newest_index].len;
    if (protocol) *protocol = slots[newest_index].protocol;
    if (reason) *reason = slots[newest_index].reason;
    (void)xSemaphoreGive(s_mu);
    return 1;
}

static int read_payload(int newest_index)
{
    slot_rec_t slots[CUBE_LOG_SLOTS];
    int n = collect(slots);
    if (newest_index < 0 || newest_index >= n) return 0;
    if (esp_flash_read(NULL, s_page, slot_addr(slots[newest_index].index), CUBE_LOG_SLOT_SIZE) != ESP_OK) return 0;
    if (get_u32(s_page) != LOG_MAGIC) return 0;
    if (slots[newest_index].len >= sizeof(s_text)) return 0;
    memcpy(s_text, s_page + LOG_HDR, slots[newest_index].len);
    s_text[slots[newest_index].len] = 0;
    return (int)slots[newest_index].len;
}

esp_err_t session_log_send_last(httpd_req_t *req)
{
    int len;
    char *body = NULL;
    if (!s_mu) return ESP_FAIL;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    len = read_payload(0);
    if (len <= 0) len = compose(SESSION_LOG_UPDATE);
    if (len > 0) {
        body = malloc((size_t)len);
        if (body) memcpy(body, s_text, (size_t)len);
    }
    (void)xSemaphoreGive(s_mu);
    (void)httpd_resp_set_type(req, "text/plain; charset=utf-8");
    (void)httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"cube-last.txt\"");
    if (!body || len <= 0) {
        free(body);
        return httpd_resp_sendstr(req, "логов нет\n");
    }
    {
        esp_err_t err = httpd_resp_send(req, body, len);
        free(body);
        return err;
    }
}

static uint32_t crc32_zip(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

esp_err_t session_log_send_zip(httpd_req_t *req)
{
    slot_rec_t slots[CUBE_LOG_SLOTS];
    uint8_t cd[700];
    size_t cd_used = 0;
    uint32_t offset = 0;
    int files = 0;
    int n;
    esp_err_t err = ESP_OK;
    if (!s_mu) return ESP_FAIL;
    (void)xSemaphoreTake(s_mu, portMAX_DELAY);
    n = collect(slots);
    (void)xSemaphoreGive(s_mu);
    if (n <= 0) {
        (void)httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(req, "логов нет\n");
    }
    (void)httpd_resp_set_type(req, "application/zip");
    (void)httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"cube-logs.zip\"");
    for (int i = n - 1; i >= 0 && err == ESP_OK; --i) {
        char name[40];
        uint8_t local[30];
        uint8_t *copy = NULL;
        unsigned len = 0;
        int name_len;
        uint32_t crc;
        int slot = slots[i].index;
        (void)xSemaphoreTake(s_mu, portMAX_DELAY);
        if (esp_flash_read(NULL, s_page, slot_addr(slot), CUBE_LOG_SLOT_SIZE) == ESP_OK &&
            get_u32(s_page) == LOG_MAGIC) {
            len = get_u16(s_page + 6);
            if (len > 0 && len <= (unsigned)(CUBE_LOG_SLOT_SIZE - LOG_HDR)) {
                copy = malloc(len);
                if (copy) memcpy(copy, s_page + LOG_HDR, len);
            }
        }
        (void)xSemaphoreGive(s_mu);
        if (!copy) continue;
        name_len = snprintf(name, sizeof(name), "cube-%u-%s-%s.txt", slots[i].seq,
                            proto_tag(slots[i].protocol), reason_en(slots[i].reason));
        if (name_len <= 0 || name_len >= (int)sizeof(name) || cd_used + 46u + (size_t)name_len > sizeof(cd)) {
            free(copy);
            continue;
        }
        crc = crc32_zip(copy, len);
        memset(local, 0, sizeof(local));
        put_u32(local, 0x04034b50u);
        put_u16(local + 4, 20);
        put_u32(local + 14, crc);
        put_u32(local + 18, len);
        put_u32(local + 22, len);
        put_u16(local + 26, (unsigned)name_len);
        memset(cd + cd_used, 0, 46);
        put_u32(cd + cd_used, 0x02014b50u);
        put_u16(cd + cd_used + 4, 20);
        put_u16(cd + cd_used + 6, 20);
        put_u32(cd + cd_used + 16, crc);
        put_u32(cd + cd_used + 20, len);
        put_u32(cd + cd_used + 24, len);
        put_u16(cd + cd_used + 28, (unsigned)name_len);
        put_u32(cd + cd_used + 42, offset);
        memcpy(cd + cd_used + 46, name, (size_t)name_len);
        cd_used += 46u + (size_t)name_len;
        err = httpd_resp_send_chunk(req, (const char *)local, 30);
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, name, name_len);
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, (const char *)copy, (ssize_t)len);
        offset += 30u + (uint32_t)name_len + len;
        files++;
        free(copy);
    }
    if (err != ESP_OK) {
        (void)httpd_resp_send_chunk(req, NULL, 0);
        return err;
    }
    if (files == 0) {
        (void)httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(req, "логов нет\n");
    }
    if (err == ESP_OK) {
        uint8_t end[22];
        memset(end, 0, sizeof(end));
        put_u32(end, 0x06054b50u);
        put_u16(end + 8, (unsigned)files);
        put_u16(end + 10, (unsigned)files);
        put_u32(end + 12, (unsigned)cd_used);
        put_u32(end + 16, offset);
        err = httpd_resp_send_chunk(req, (const char *)cd, (ssize_t)cd_used);
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, (const char *)end, sizeof(end));
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, NULL, 0);
    }
    return err;
}
