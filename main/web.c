#include "web.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "app_state.h"
#include "sm_relay.h"
#include "can_gbt.h"
#include "safe_ota.h"
#include "display.h"
#include "settings.h"
#include "diag.h"

extern const uint8_t web_index_html[];
extern const size_t web_index_html_len;
extern const uint8_t web_style_css[];
extern const size_t web_style_css_len;
extern const uint8_t web_app_js[];
extern const size_t web_app_js_len;

static const char *TAG = "WEB";
static uint8_t s_ap_channel = 6;
static bool s_sta_got_ip;
static char s_sta_ip[16];
static volatile bool s_wifi_restart_busy;

static httpd_handle_t s_server;

static void set_no_cache(httpd_req_t *req)
{
    /* Do not force Connection: close. The UI polls status frequently and the ESP HTTP
       server is happier when the client can reuse a keep-alive socket. */
    (void)httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    (void)httpd_resp_set_hdr(req, "Pragma", "no-cache");
}

static void set_security_headers(httpd_req_t *req)
{
    (void)httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    (void)httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    (void)httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
    (void)httpd_resp_set_hdr(req, "Content-Security-Policy",
                             "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; "
                             "connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
}

static bool auth(httpd_req_t *req)
{
    /* Empty key is a deliberate local-development mode; production build ships
       with a non-empty write-control key. */
    if (CUBE_WEB_KEY[0] == '\0') return true;
    size_t n = httpd_req_get_hdr_value_len(req, "X-Cube-Key");
    if (n == 0U || n >= 96U) return false;
    char key[96] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Cube-Key", key, sizeof(key)) != ESP_OK) return false;
    return strcmp(key, CUBE_WEB_KEY) == 0;
}

static esp_err_t forbidden(httpd_req_t *req)
{
    set_security_headers(req);
    set_no_cache(req);
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"unauthorized\"}");
}

static esp_err_t send_asset(httpd_req_t *req, const char *type, const uint8_t *data, size_t len, bool cache)
{
    set_security_headers(req);
    (void)httpd_resp_set_type(req, type);
    (void)httpd_resp_set_hdr(req, "Cache-Control", cache ? "public, max-age=3600" : "no-cache");
    return httpd_resp_send(req, (const char *)data, len);
}

static esp_err_t root_get(httpd_req_t *req)
{
    return send_asset(req, "text/html; charset=utf-8", web_index_html, web_index_html_len, false);
}

static esp_err_t css_get(httpd_req_t *req)
{
    return send_asset(req, "text/css; charset=utf-8", web_style_css, web_style_css_len, true);
}

static esp_err_t js_get(httpd_req_t *req)
{
    return send_asset(req, "application/javascript; charset=utf-8", web_app_js, web_app_js_len, true);
}

static esp_err_t favicon_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static void hex8(const uint8_t *data, uint8_t n, char *out, size_t out_sz)
{
    if (!data || !out || out_sz < ((size_t)n * 2U + 1U)) {
        if (out && out_sz) out[0] = 0;
        return;
    }
    for (uint8_t i = 0; i < n; ++i) {
        (void)snprintf(&out[i * 2U], 3U, "%02X", data[i]);
    }
    out[n * 2U] = 0;
}

static bool charger_online(uint32_t last_rx_ms)
{
    if (!last_rx_ms) return false;
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    return (uint32_t)(now - last_rx_ms) <= 3000U;
}

static long calc_power_w(const cube_state_snapshot_t *s)
{
    if (!s || s->ccs_output_voltage_raw == 0U || s->ccs_output_current_0p1a <= 0) return 0L;
    return ((long)s->ccs_output_voltage_raw * (long)s->ccs_output_current_0p1a) / 100L;
}

static esp_err_t status_get(httpd_req_t *req)
{
    set_security_headers(req);
    cube_state_snapshot_t s;
    app_state_snapshot(&s);
    char last_hex[17] = {0};
    hex8(s.last.data, s.last.dlc, last_hex, sizeof(last_hex));
    const bool online = charger_online(s.last_charger_rx_ms);
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    const uint32_t stage_age_ms = s.charger_stage_ms ? (uint32_t)(now_ms - s.charger_stage_ms) : 0U;
    const long power_w = calc_power_w(&s);
    float cpu_temp = 0.0f;
    const bool cpu_temp_valid = diag_get_temperature(&cpu_temp);
    const uint8_t cpu_load = diag_get_cpu_load_percent();
    const uint8_t brightness = display_get_brightness();
    char json[3400];
    int written = snprintf(json, sizeof(json),
        "{\"ok\":true,\"fw\":\"%s\",\"version\":\"%s\","
        "\"can\":%s,\"tx_enabled\":%s,\"rx_count\":%lu,\"tx_count\":%lu,"
        "\"last_id\":\"0x%08lX\",\"last_dlc\":%u,\"last_data\":\"%s\","
        "\"charger_online\":%s,\"stage\":\"%s\",\"stage_ms\":%lu,"
        "\"main_state\":%u,\"main_state_name\":\"%s\",\"substate\":%u,"
        "\"relay_a\":%u,\"relay_b\":%u,\"handoff_done\":%s,\"session_active\":%s,"
        "\"battery_voltage_0p1v\":%u,\"battery_current_0p1a\":%d,\"soc\":%u,"
        "\"cml_max_voltage_0p1v\":%u,\"cml_min_voltage_0p1v\":%u,"
        "\"cml_max_current_0p1a\":%d,\"cml_min_current_0p1a\":%d,"
        "\"ccs_voltage_0p1v\":%u,\"ccs_current_0p1a\":%d,\"ccs_time_min\":%u,"
        "\"ccs_charge_permit\":%u,\"ccs_status_raw\":%u,\"power_w\":%ld,"
        "\"cro_status_raw\":%u,\"cst_status_raw\":%u,\"csd_time_min\":%u,\"csd_energy_0p1kwh\":%u,"
        "\"bcl_voltage_0p1v\":%u,\"bcl_current_0p1a\":%d,\"bcl_mode\":%u,"
        "\"cpu_temp_c\":%.1f,\"cpu_temp_valid\":%s,\"cpu_load_percent\":%u,\"brightness_percent\":%u,\"heap_free_bytes\":%u,"
        "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed\":%s}",
        CUBE_FW_NAME, CUBE_FW_VERSION,
        s.can_up ? "true" : "false", s.tx_enabled ? "true" : "false",
        (unsigned long)s.rx_count, (unsigned long)s.tx_count,
        (unsigned long)s.last.id, (unsigned)s.last.dlc, last_hex,
        online ? "true" : "false", cube_charge_stage_name(s.charger_stage), (unsigned long)s.charger_stage_ms, (unsigned long)stage_age_ms,
        (unsigned)s.main_state, cube_main_state_name(s.main_state), (unsigned)s.substate,
        (unsigned)s.relay_a, (unsigned)s.relay_b, s.handoff_done ? "true" : "false",
        s.session_active ? "true" : "false",
        (unsigned)s.battery_voltage_0p1v, (int)s.battery_current_0p1a, (unsigned)s.soc_percent,
        (unsigned)s.cml_max_voltage_raw, (unsigned)s.cml_min_voltage_raw,
        (int)s.cml_max_current_0p1a, (int)s.cml_min_current_0p1a,
        (unsigned)s.ccs_output_voltage_raw, (int)s.ccs_output_current_0p1a, (unsigned)s.ccs_charge_time_min,
        (unsigned)s.ccs_charge_permit, (unsigned)s.ccs_status_raw, power_w,
        (unsigned)s.cro_status_raw, (unsigned)s.cst_status_raw, (unsigned)s.csd_time_min,
        (unsigned)s.csd_energy_0p1kwh, (unsigned)s.bcl_requested_voltage_raw,
        (int)s.bcl_requested_current_0p1a, (unsigned)s.bcl_mode,
        (double)cpu_temp, cpu_temp_valid ? "true" : "false", (unsigned)cpu_load,
        (unsigned)brightness, (unsigned)esp_get_free_heap_size(),
        s.ble_connected ? "true" : "false", s.ble_notify_enabled ? "true" : "false",
        s.ble_indicate_enabled ? "true" : "false", s.display_ready ? "true" : "false",
        s.touch_enabled ? "true" : "false", s.touch_pressed ? "true" : "false");
    if (written < 0 || (size_t)written >= sizeof(json)) return ESP_ERR_NO_MEM;
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, json);
}

static int req_reader(uint8_t *buf, size_t max_len, void *ctx)
{
    httpd_req_t *req = (httpd_req_t *)ctx;
    if (!req || !buf || max_len == 0U) return -1;
    for (int attempt = 0; attempt < 3; ++attempt) {
        int n = httpd_req_recv(req, (char *)buf, max_len);
        if (n != HTTPD_SOCK_ERR_TIMEOUT) return n;
    }
    return HTTPD_SOCK_ERR_TIMEOUT;
}

static int read_body_exact(httpd_req_t *req, char *body, size_t capacity)
{
    if (!req || !body || capacity == 0U || req->content_len == 0U || req->content_len >= capacity) return -1;
    size_t got = 0;
    while (got < req->content_len) {
        int n = req_reader((uint8_t *)body + got, req->content_len - got, req);
        if (n <= 0) return -1;
        got += (size_t)n;
    }
    body[got] = 0;
    return (int)got;
}

static const char *find_json_key(const char *body, const char *key)
{
    if (!body || !key) return NULL;
    char needle[48];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) return NULL;
    return strstr(body, needle);
}

static bool json_bool(const char *body, const char *key, bool *value)
{
    const char *p = find_json_key(body, key);
    if (!p || !value) return false;
    p = strchr(p, ':');
    if (!p) return false;
    do { ++p; } while (isspace((unsigned char)*p));
    if (strncmp(p, "true", 4) == 0 && !isalnum((unsigned char)p[4])) { *value = true; return true; }
    if (strncmp(p, "false", 5) == 0 && !isalnum((unsigned char)p[5])) { *value = false; return true; }
    if (*p == '1' && !isdigit((unsigned char)p[1])) { *value = true; return true; }
    if (*p == '0' && !isdigit((unsigned char)p[1])) { *value = false; return true; }
    return false;
}

static bool json_u32(const char *body, const char *key, unsigned long *value)
{
    const char *p = find_json_key(body, key);
    if (!p || !value) return false;
    p = strchr(p, ':');
    if (!p) return false;
    do { ++p; } while (isspace((unsigned char)*p));
    char *end = NULL;
    unsigned long v = strtoul(p, &end, 0);
    if (end == p) return false;
    while (isspace((unsigned char)*end)) ++end;
    if (*end != ',' && *end != '}' && *end != 0) return false;
    *value = v;
    return true;
}

static bool json_data8(const char *body, uint8_t *out, uint8_t *count)
{
    const char *p = find_json_key(body, "data");
    if (!p || !out || !count) return false;
    p = strchr(p, '[');
    if (!p) return false;
    ++p;
    uint8_t n = 0;
    while (*p && *p != ']') {
        while (isspace((unsigned char)*p) || *p == ',') ++p;
        if (*p == ']') break;
        if (n >= 8U) return false;
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 0);
        if (end == p || v > 255UL) return false;
        out[n++] = (uint8_t)v;
        p = end;
    }
    if (*p != ']') return false;
    ++p;
    while (isspace((unsigned char)*p)) ++p;
    if (*p != ',' && *p != '}') return false;
    *count = n;
    return true;
}

static bool appendf(char *buf, size_t cap, size_t *used, const char *fmt, ...)
{
    if (!buf || !used || *used >= cap) return false;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *used, cap - *used, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *used) return false;
    *used += (size_t)n;
    return true;
}

static esp_err_t can_recent_get(httpd_req_t *req)
{
    cube_can_frame_t frames[24];
    size_t n = 0;
    esp_err_t err = gbt_can_get_history(frames, 24U, &n);
    if (err != ESP_OK) return err;
    char json[4096];
    size_t used = 0;
    if (!appendf(json, sizeof(json), &used, "{\"ok\":true,\"count\":%u,\"frames\":[", (unsigned)n)) return ESP_ERR_NO_MEM;
    for (size_t i = 0; i < n; ++i) {
        char hex[17];
        hex8(frames[i].data, frames[i].dlc, hex, sizeof(hex));
        if (!appendf(json, sizeof(json), &used,
                     "%s{\"id\":\"0x%08lX\",\"dlc\":%u,\"flags\":%lu,\"ms\":%lu,\"data\":\"%s\"}",
                     i ? "," : "", (unsigned long)frames[i].id, (unsigned)frames[i].dlc,
                     (unsigned long)frames[i].flags, (unsigned long)frames[i].timestamp_ms, hex)) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (!appendf(json, sizeof(json), &used, "]}")) return ESP_ERR_NO_MEM;
    set_security_headers(req);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t settings_get(httpd_req_t *req)
{
    set_security_headers(req);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    char out[256];
    (void)snprintf(out, sizeof(out),
                   "{\"ok\":true,\"tx_enabled\":%s,\"tx_compiled\":true,"
                   "\"web_key_required\":%s,\"wifi_auth\":\"WPA2-PSK\","
                   "\"brightness_percent\":%u}",
                   gbt_can_get_tx_enabled() ? "true" : "false",
                   CUBE_WEB_KEY[0] ? "true" : "false",
                   (unsigned)display_get_brightness());
    return httpd_resp_sendstr(req, out);
}

static esp_err_t settings_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    char body[256];
    if (read_body_exact(req, body, sizeof(body)) < 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_body\"}");
    }
    bool value = false;
    if (json_bool(body, "toggle", &value) && value) {
        gbt_can_set_tx_enabled(!gbt_can_get_tx_enabled());
    }
    if (json_bool(body, "tx_enabled", &value)) {
        gbt_can_set_tx_enabled(value);
    }

    unsigned long brightness = 0;
    if (json_u32(body, "brightness", &brightness)) {
        if (brightness < 10UL || brightness > 100UL) {
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_brightness\"}");
        }
        esp_err_t berr = settings_set_brightness((uint8_t)brightness);
        if (berr != ESP_OK) {
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"brightness_save_failed\"}");
        }
        (void)display_set_brightness((uint8_t)brightness);
    }

    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    char out[160];
    (void)snprintf(out, sizeof(out), "{\"ok\":true,\"tx_enabled\":%s,\"brightness_percent\":%u}",
                   gbt_can_get_tx_enabled() ? "true" : "false",
                   (unsigned)display_get_brightness());
    return httpd_resp_sendstr(req, out);
}

static esp_err_t can_raw_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    char body[512];
    if (read_body_exact(req, body, sizeof(body)) < 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_body\"}");
    }
    unsigned long id = 0, dlc = 0;
    uint8_t data[8] = {0};
    uint8_t data_count = 0;
    if (!json_u32(body, "id", &id) || !json_u32(body, "dlc", &dlc) ||
        dlc > 8UL || !json_data8(body, data, &data_count) || data_count != (uint8_t)dlc ||
        id > 0x1FFFFFFFUL) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_can_frame\"}");
    }
    esp_err_t err = gbt_can_send((uint32_t)id, data, (uint8_t)dlc);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, err == ESP_ERR_INVALID_STATE ? "409 Conflict" : "500 Internal Server Error");
    }
    char out[192];
    (void)snprintf(out, sizeof(out), "{\"ok\":%s,\"error\":\"%s\",\"id\":\"0x%08lX\",\"dlc\":%lu}",
                   err == ESP_OK ? "true" : "false", esp_err_to_name(err), id, dlc);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, out);
}

static esp_err_t reboot_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    esp_err_t err = httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return err;
}

static esp_err_t update_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    if (req->content_len < 24U || req->content_len > CUBE_HTTP_MAX_UPLOAD) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_size\"}");
    }
    uint8_t first[4096];
    size_t first_want = req->content_len < sizeof(first) ? req->content_len : sizeof(first);
    int got = req_reader(first, first_want, req);
    if (got <= 0) {
        httpd_resp_set_status(req, "408 Request Timeout");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"upload_timeout\"}");
    }
    if (first[0] != 0xE9) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"not_esp_image\"}");
    }
    esp_err_t err = safe_ota_write_stream(first, (size_t)got, req_reader, req, req->content_len);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    if (err != ESP_OK) {
        char msg[160];
        (void)snprintf(msg, sizeof(msg), "{\"ok\":false,\"error\":\"%s\"}", esp_err_to_name(err));
        return httpd_resp_sendstr(req, msg);
    }
    esp_err_t send_err = httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(700));
    esp_restart();
    return send_err;
}

static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base != WIFI_EVENT) return;
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *e = (const wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "STA connected %02X:%02X:%02X:%02X:%02X:%02X AID=%u",
                 e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5], e->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        /* IDF 5.0.9: wifi_event_ap_stadisconnected_t has only mac + aid (no reason). */
        const wifi_event_ap_stadisconnected_t *e = (const wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u",
                 e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5], e->aid);
    }
}

static esp_err_t captive_text_get(httpd_req_t *req)
{
    set_security_headers(req);
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "text/plain; charset=utf-8");
    if (strcmp(req->uri, "/connecttest.txt") == 0) {
        return httpd_resp_sendstr(req, "Microsoft Connect Test");
    }
    if (strcmp(req->uri, "/ncsi.txt") == 0) {
        return httpd_resp_sendstr(req, "Microsoft NCSI");
    }
    return httpd_resp_sendstr(req, "FGMDCharge local portal");
}

static esp_err_t captive_html_get(httpd_req_t *req)
{
    return root_get(req);
}

static esp_err_t portal_catchall_get(httpd_req_t *req)
{
    /* Captive-portal clients frequently probe vendor-specific paths. Serve the
       local UI instead of returning a 404 that makes some clients abandon the AP. */
    return root_get(req);
}


static esp_err_t session_start_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    sm_start_session();
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"action\":\"start\"}");
}

static esp_err_t session_stop_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    sm_stop_session();
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"action\":\"stop\"}");
}


static uint8_t wifi_pick_channel(void)
{
    /* Prefer 1/6/11 with least APs visible. */
    int cnt[14] = {0};
    wifi_scan_config_t sc = {0};
    sc.show_hidden = false;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return 6;
    }
    uint16_t n = 0;
    (void)esp_wifi_scan_get_ap_num(&n);
    if (n == 0) return 6;
    if (n > 40) n = 40;
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs) return 6;
    uint16_t got = n;
    if (esp_wifi_scan_get_ap_records(&got, recs) != ESP_OK) {
        free(recs);
        return 6;
    }
    for (uint16_t i = 0; i < got; ++i) {
        uint8_t ch = recs[i].primary;
        if (ch >= 1 && ch <= 13) cnt[ch]++;
    }
    free(recs);
    const uint8_t cand[] = {1, 6, 11};
    uint8_t best = 6;
    int best_n = 999;
    for (int i = 0; i < 3; ++i) {
        uint8_t ch = cand[i];
        if (cnt[ch] < best_n) {
            best_n = cnt[ch];
            best = ch;
        }
    }
    ESP_LOGW(TAG, "auto channel -> %u (load 1=%d 6=%d 11=%d)", best, cnt[1], cnt[6], cnt[11]);
    return best;
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_sta_got_ip = true;
        ESP_LOGW(TAG, "STA got IP %s", s_sta_ip);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_got_ip = false;
        s_sta_ip[0] = 0;
        ESP_LOGW(TAG, "STA disconnected");
    }
}

esp_err_t web_init(void)
{
    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    /* Official ESP-IDF SoftAP path (examples/wifi/getting_started/softAP). */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) return ESP_ERR_NO_MEM;
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;
    (void)esp_wifi_set_storage(WIFI_STORAGE_RAM);

    (void)esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
                                              &wifi_ap_event_handler, NULL, NULL);
    (void)esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED,
                                              &wifi_ap_event_handler, NULL, NULL);
    (void)esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &ip_event_handler, NULL, NULL);
    (void)esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              &ip_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    (void)snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", CUBE_WIFI_SSID);
    wifi_config.ap.ssid_len = (uint8_t)strlen(CUBE_WIFI_SSID);
    wifi_config.ap.channel = CUBE_WIFI_CHANNEL;
    wifi_config.ap.max_connection = CUBE_WIFI_MAX_CONNECTIONS;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.pmf_cfg.capable = true;
    wifi_config.ap.pmf_cfg.required = false;
    if (strlen(CUBE_WIFI_PASSWORD) >= 8U) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        (void)snprintf((char *)wifi_config.ap.password, sizeof(wifi_config.ap.password),
                       "%s", CUBE_WIFI_PASSWORD);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.password[0] = 0;
    }

    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) return err;
    /* Start radio briefly to scan for auto channel */
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    s_ap_channel = (CUBE_WIFI_CHANNEL >= 1 && CUBE_WIFI_CHANNEL <= 13)
                       ? (uint8_t)CUBE_WIFI_CHANNEL : wifi_pick_channel();
    wifi_config.ap.channel = s_ap_channel;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) return err;
    /* Re-apply after channel pick (already started) */
    (void)esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) return err;
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    (void)esp_wifi_set_ps(WIFI_PS_NONE);
    (void)esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    (void)esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    /* 80 = 20 dBm units of 0.25 dBm */
    (void)esp_wifi_set_max_tx_power(80);


    {
        uint8_t mac[6] = {0};
        (void)esp_wifi_get_mac(WIFI_IF_AP, mac);
        ESP_LOGW(TAG, "softAP started SSID:%s password:%s channel:%u",
                 CUBE_WIFI_SSID,
                 (strlen(CUBE_WIFI_PASSWORD) >= 8U) ? CUBE_WIFI_PASSWORD : "(open)",
                 (unsigned)s_ap_channel);
        ESP_LOGW(TAG, "AP MAC %02X:%02X:%02X:%02X:%02X:%02X UI http://192.168.4.1/",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.max_uri_handlers = 28;
    hc.stack_size = 12288;
    hc.recv_wait_timeout = 10;
    hc.send_wait_timeout = 10;
    hc.lru_purge_enable = true;
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.max_open_sockets = 7;
    if ((err = httpd_start(&s_server, &hc)) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Specific routes first; wildcard last. */
    const httpd_uri_t uri[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get},
        {.uri = "/style.css", .method = HTTP_GET, .handler = css_get},
        {.uri = "/app.js", .method = HTTP_GET, .handler = js_get},
        {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get},
        {.uri = "/generate_204", .method = HTTP_GET, .handler = captive_html_get},
        {.uri = "/gen_204", .method = HTTP_GET, .handler = captive_html_get},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_html_get},
        {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = captive_text_get},
        {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = captive_text_get},
        {.uri = "/success.txt", .method = HTTP_GET, .handler = captive_text_get},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/api/can/recent", .method = HTTP_GET, .handler = can_recent_get},
        {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_get},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post},
        {.uri = "/api/can/raw", .method = HTTP_POST, .handler = can_raw_post},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = reboot_post},
        {.uri = "/api/update", .method = HTTP_POST, .handler = update_post},
        {.uri = "/api/session/start", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/session/stop", .method = HTTP_POST, .handler = session_stop_post},
        {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_get},
    };
    for (size_t u = 0; u < sizeof(uri) / sizeof(uri[0]); ++u) {
        if ((err = httpd_register_uri_handler(s_server, &uri[u])) != ESP_OK) {
            ESP_LOGE(TAG, "uri %s failed: %s", uri[u].uri, esp_err_to_name(err));
            (void)httpd_stop(s_server);
            s_server = NULL;
            return err;
        }
    }

    ESP_LOGI(TAG, "Wi-Fi AP=%s UI=http://192.168.4.1/", CUBE_WIFI_SSID);
    return ESP_OK;
}


static volatile bool s_wifi_restart_busy;

static void wifi_restart_task(void *arg)
{
    (void)arg;
    ESP_LOGW(TAG, "WiFi restart task");
    esp_err_t err;

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "wifi_stop: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    wifi_config_t wifi_config = {0};
    (void)snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "%s", CUBE_WIFI_SSID);
    wifi_config.ap.ssid_len = (uint8_t)strlen(CUBE_WIFI_SSID);
    wifi_config.ap.channel = CUBE_WIFI_CHANNEL;
    wifi_config.ap.max_connection = CUBE_WIFI_MAX_CONNECTIONS;
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.pmf_cfg.capable = true;
    wifi_config.ap.pmf_cfg.required = false;
    if (strlen(CUBE_WIFI_PASSWORD) >= 8U) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        (void)snprintf((char *)wifi_config.ap.password, sizeof(wifi_config.ap.password),
                       "%s", CUBE_WIFI_PASSWORD);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.password[0] = 0;
    }

    (void)esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_config.ap.channel = s_ap_channel ? s_ap_channel : 6;
    (void)esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "wifi_start: %s", esp_err_to_name(err));
    } else {
        (void)esp_wifi_set_ps(WIFI_PS_NONE);
        (void)esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
        (void)esp_wifi_set_protocol(WIFI_IF_AP,
            WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        (void)esp_wifi_set_max_tx_power(80);
        ESP_LOGW(TAG, "WiFi restarted SSID=%s ch=%u TX=max",
                 CUBE_WIFI_SSID, (unsigned)s_ap_channel);
    }
    s_wifi_restart_busy = false;
    vTaskDelete(NULL);
}

esp_err_t web_wifi_restart(void)
{
    if (s_wifi_restart_busy) {
        ESP_LOGW(TAG, "WiFi restart already running");
        return ESP_ERR_INVALID_STATE;
    }
    s_wifi_restart_busy = true;
    /* Never call esp_wifi_stop/start from LVGL task — use worker. */
    if (xTaskCreate(wifi_restart_task, "wifi_rst", 4096, NULL, 5, NULL) != pdPASS) {
        s_wifi_restart_busy = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

uint8_t web_wifi_ap_channel(void) { return s_ap_channel; }
bool web_wifi_sta_connected(void) { return s_sta_got_ip; }
void web_wifi_sta_ip(char *buf, size_t buflen)
{
    if (!buf || buflen == 0) return;
    if (s_sta_got_ip) {
        snprintf(buf, buflen, "%s", s_sta_ip);
    } else {
        buf[0] = 0;
    }
}

int web_wifi_scan(web_scan_ap_t *out, int max_out)
{
    if (!out || max_out <= 0) return 0;
    wifi_scan_config_t sc = {0};
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    sc.show_hidden = false;
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return 0;
    uint16_t n = 0;
    (void)esp_wifi_scan_get_ap_num(&n);
    if (n == 0) return 0;
    if (n > 32) n = 32;
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs) return 0;
    uint16_t got = n;
    if (esp_wifi_scan_get_ap_records(&got, recs) != ESP_OK) {
        free(recs);
        return 0;
    }
    int m = 0;
    for (uint16_t i = 0; i < got && m < max_out; ++i) {
        if (recs[i].ssid[0] == 0) continue;
        memset(&out[m], 0, sizeof(out[m]));
        memcpy(out[m].ssid, recs[i].ssid, 32);
        out[m].rssi = recs[i].rssi;
        out[m].authmode = (uint8_t)recs[i].authmode;
        out[m].channel = recs[i].primary;
        m++;
    }
    free(recs);
    return m;
}

esp_err_t web_wifi_sta_connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    s_sta_got_ip = false;
    s_sta_ip[0] = 0;
    wifi_config_t sta = {0};
    (void)snprintf((char *)sta.sta.ssid, sizeof(sta.sta.ssid), "%s", ssid);
    if (password) {
        (void)snprintf((char *)sta.sta.password, sizeof(sta.sta.password), "%s", password);
    }
    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    if (!password || !password[0]) {
        sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (err != ESP_OK) return err;
    (void)esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    err = esp_wifi_connect();
    ESP_LOGW(TAG, "STA connect to '%s': %s", ssid, esp_err_to_name(err));
    return err;
}

