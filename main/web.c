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

extern const uint8_t web_index_html[];
extern const size_t web_index_html_len;
extern const uint8_t web_style_css[];
extern const size_t web_style_css_len;
extern const uint8_t web_app_js[];
extern const size_t web_app_js_len;

static const char *TAG = "WEB";
static int s_ap_clients;
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
    const long power_w = calc_power_w(&s);
    char json[3200];
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
        "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed\":%s,"
        "\"protocol\":\"%s\",\"voltage_v\":%.1f,\"current_a\":%.1f,\"power_w\":%ld,\"time_min\":%u,"
        "\"permit\":\"%s\",\"ccs\":\"%s\",\"can\":\"%s\",\"limit_v\":\"%.1f\",\"limit_i\":\"%.1f\",\"request\":\"%.1f/%.1f\",\"mode\":\"%s\",\"wifi\":\"%s\",\"sub_state\":%u,\"ap_channel\":%d,\"ap_clients\":%u,\"sta_ip\":\"%s\"}",
        CUBE_FW_NAME, CUBE_FW_VERSION,
        s.can_up ? "true" : "false", s.tx_enabled ? "true" : "false",
        (unsigned long)s.rx_count, (unsigned long)s.tx_count,
        (unsigned long)s.last.id, (unsigned)s.last.dlc, last_hex,
        online ? "true" : "false", cube_charge_stage_name(s.charger_stage), (unsigned long)s.charger_stage_ms,
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
        s.ble_connected ? "true" : "false", s.ble_notify_enabled ? "true" : "false",
        s.ble_indicate_enabled ? "true" : "false", s.display_ready ? "true" : "false",
        s.touch_enabled ? "true" : "false", s.touch_pressed ? "true" : "false",
        cube_protocol_name(s.protocol),
        (s.ccs_output_voltage_raw ? s.ccs_output_voltage_raw : s.battery_voltage_0p1v) / 10.0,
        (s.ccs_output_current_0p1a ? s.ccs_output_current_0p1a : s.battery_current_0p1a) / 10.0,
        power_w, (unsigned)s.ccs_charge_time_min,
        s.ccs_charge_permit ? "YES" : "NO",
        s.ccs_status_raw ? "RUN" : "—",
        s.can_up ? "UP" : "DOWN",
        s.cml_max_voltage_raw / 10.0, s.cml_max_current_0p1a / 10.0,
        s.bcl_requested_voltage_raw / 10.0, s.bcl_requested_current_0p1a / 10.0,
        s.bcl_mode ? "CC" : "CV", CUBE_WIFI_SSID, (unsigned)s.substate,
        web_get_ap_channel(), (unsigned)web_ap_clients(), web_sta_ip());
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
    return httpd_resp_sendstr(req, gbt_can_get_tx_enabled()
        ? "{\"ok\":true,\"tx_enabled\":true,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}"
        : "{\"ok\":true,\"tx_enabled\":false,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}");
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
    set_no_cache(req);
    (void)httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, gbt_can_get_tx_enabled()
        ? "{\"ok\":true,\"tx_enabled\":true}"
        : "{\"ok\":true,\"tx_enabled\":false}");
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
        s_ap_clients++;
        ESP_LOGI(TAG, "STA connected %02X:%02X:%02X:%02X:%02X:%02X AID=%u",
                 e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5], e->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        /* IDF 5.0.9: wifi_event_ap_stadisconnected_t has only mac + aid (no reason). */
        const wifi_event_ap_stadisconnected_t *e = (const wifi_event_ap_stadisconnected_t *)event_data;
        if (s_ap_clients > 0) s_ap_clients--;
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


static int s_ap_channel = CUBE_WIFI_CHANNEL;
static char s_scan_ssid[8][33];
static int s_scan_n;
static char s_sta_ip[20] = "--";
static char s_join_ssid[33];

int web_get_ap_channel(void) { return s_ap_channel; }
int web_ap_clients(void) { return s_ap_clients; }
int web_scan_count(void) { return s_scan_n; }
const char *web_scan_ssid(int index)
{
    if (index < 0 || index >= s_scan_n) return "";
    return s_scan_ssid[index];
}
const char *web_sta_ip(void) { return s_sta_ip; }

static void wifi_ip_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == IP_EVENT_STA_GOT_IP && data) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        uint32_t a = ev->ip_info.ip.addr;
        snprintf(s_sta_ip, sizeof(s_sta_ip), "%u.%u.%u.%u",
                 (unsigned)(a & 255u), (unsigned)((a >> 8) & 255u),
                 (unsigned)((a >> 16) & 255u), (unsigned)((a >> 24) & 255u));
        ESP_LOGW(TAG, "STA got IP %s", s_sta_ip);
    }
}

esp_err_t web_scan_now(void)
{
    wifi_scan_config_t scan = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return err;
    }
    uint16_t n = 8;
    wifi_ap_record_t rec[8];
    memset(rec, 0, sizeof(rec));
    if (esp_wifi_scan_get_ap_records(&n, rec) != ESP_OK) n = 0;
    s_scan_n = (int)n;
    for (int i = 0; i < s_scan_n; ++i) {
        memcpy(s_scan_ssid[i], rec[i].ssid, 32);
        s_scan_ssid[i][32] = 0;
        ESP_LOGW(TAG, "scan[%d] %s rssi=%d ch=%u", i, s_scan_ssid[i], rec[i].rssi, rec[i].primary);
    }
    return ESP_OK;
}

esp_err_t web_sta_join(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    wifi_config_t sta = {0};
    snprintf((char *)sta.sta.ssid, sizeof(sta.sta.ssid), "%s", ssid);
    if (password) snprintf((char *)sta.sta.password, sizeof(sta.sta.password), "%s", password);
    sta.sta.threshold.authmode = (password && strlen(password) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    snprintf(s_join_ssid, sizeof(s_join_ssid), "%s", ssid);
    snprintf(s_sta_ip, sizeof(s_sta_ip), "wait");
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (err != ESP_OK) return err;
    (void)esp_wifi_disconnect();
    err = esp_wifi_connect();
    ESP_LOGW(TAG, "STA join '%s' -> %s", ssid, esp_err_to_name(err));
    return err;
}

esp_err_t web_wifi_restart(void)
{
    ESP_LOGW(TAG, "WiFi restart requested");
    (void)esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        (void)esp_wifi_set_max_tx_power(84);
        ESP_LOGW(TAG, "WiFi restarted SSID=%s", CUBE_WIFI_SSID);
        (void)web_scan_now();
    } else {
        ESP_LOGE(TAG, "WiFi restart failed: %s", esp_err_to_name(err));
    }
    return err;
}


static void wifi_boot_scan_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(400));
    ESP_LOGW(TAG, "background scan start");
    (void)web_scan_now();
    (void)esp_wifi_set_max_tx_power(84);
    ESP_LOGW(TAG, "background scan done, AP stays up");
    vTaskDelete(NULL);
}


static esp_err_t log_get(httpd_req_t *req)
{
    set_security_headers(req);
    char buf[1200];
    app_state_log_text(buf, sizeof(buf));
    set_no_cache(req);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, buf);
}

static esp_err_t setpoints_post(httpd_req_t *req)
{
    set_security_headers(req);
    char body[160] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n < 0) return ESP_FAIL;
    body[n] = 0;
    int v = 0, i = 0, soc = -1;
    char vin[18] = {0};
    sscanf(body, "v=%d&i=%d&soc=%d&vin=%17s", &v, &i, &soc, vin);
    if (v > 0 && v < 1000) v *= 10; /* volts from the page */
    if (i > 0 && i < 400) i *= 10;  /* amps from the page */
    if (v < 500) v = 500;
    if (v > 5000) v = 5000;
    if (i < 10) i = 10;
    if (i > 2500) i = 2500;
    app_state_set_bcl((uint16_t)v, (int16_t)i, 1);
    app_state_set_battery_vi((uint16_t)v, (int16_t)i);
    if (soc >= 0 && soc <= 100) app_state_set_soc((uint8_t)soc);
    if (vin[0]) app_state_set_vin(vin);
    app_state_log("setpoints updated, session not stopped");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}


static esp_err_t wifi_scan_get(httpd_req_t *req)
{
    set_security_headers(req);
    (void)web_scan_now();
    char buf[700];
    size_t used = 0;
    appendf(buf, sizeof(buf), &used, "{\"ok\":true,\"nets\":[");
    int n = web_scan_count();
    for (int i = 0; i < n; i++) {
        appendf(buf, sizeof(buf), &used, "%s\"%s\"", i ? "," : "", web_scan_ssid(i));
    }
    appendf(buf, sizeof(buf), &used, "]}");
    set_no_cache(req);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, buf);
}

esp_err_t web_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wifi_ap_event_handler, NULL);
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &wifi_ap_event_handler, NULL);
    (void)esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_ip_handler, NULL);
    if (!esp_netif_create_default_wifi_ap()) return ESP_ERR_NO_MEM;
    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;
    (void)esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_config_t ap = {0};
    (void)snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", CUBE_WIFI_SSID);
    ap.ap.ssid_len = (uint8_t)strlen(CUBE_WIFI_SSID);
    ap.ap.channel = CUBE_WIFI_CHANNEL;
    s_ap_channel = ap.ap.channel;
    ap.ap.max_connection = CUBE_WIFI_MAX_CONNECTIONS;
    ap.ap.ssid_hidden = 0;          /* always broadcast beacon */
    ap.ap.beacon_interval = 100;
    ap.ap.pmf_cfg.capable = true;
    ap.ap.pmf_cfg.required = false;
    if (strlen(CUBE_WIFI_PASSWORD) >= 8U) {
        /* WPA2-PSK/CCMP is deterministic and avoids mixed-mode roaming quirks. */
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ap.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
        (void)snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", CUBE_WIFI_PASSWORD);
    } else {
        ap.ap.authmode = WIFI_AUTH_OPEN;
        ap.ap.password[0] = 0;
    }
    if ((err = esp_wifi_set_mode(WIFI_MODE_APSTA)) != ESP_OK) return err;
    if ((err = esp_wifi_set_config(WIFI_IF_AP, &ap)) != ESP_OK) return err;
    { wifi_config_t sta0 = {0}; (void)esp_wifi_set_config(WIFI_IF_STA, &sta0); }
    (void)esp_wifi_set_ps(WIFI_PS_NONE);
    (void)esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    (void)esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    (void)esp_wifi_set_country_code("01", true);
    if ((err = esp_wifi_start()) != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    (void)esp_wifi_set_max_tx_power(84);
    (void)esp_wifi_set_max_tx_power(84);
    ESP_LOGW(TAG, "APSTA up now SSID=%s ch=%u HTTP first, scan in background",
             CUBE_WIFI_SSID, (unsigned)CUBE_WIFI_CHANNEL);
    xTaskCreate(wifi_boot_scan_task, "wifi_scan", 4096, NULL, 3, NULL);

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.max_uri_handlers = 36;
    hc.stack_size = 12288;
    hc.recv_wait_timeout = 10;
    hc.send_wait_timeout = 10;
    hc.lru_purge_enable = true;
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.max_open_sockets = 4;
    if ((err = httpd_start(&s_server, &hc)) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Specific routes first. Wildcard catch-all MUST be last or IDF reports
       ESP_ERR_HTTPD_HANDLER_EXISTS / wrong match order. */
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
        {.uri = "/api/wifi/scan", .method = HTTP_GET, .handler = wifi_scan_get},
        {.uri = "/api/can/recent", .method = HTTP_GET, .handler = can_recent_get},
        {.uri = "/api/can", .method = HTTP_GET, .handler = can_recent_get},
        {.uri = "/api/start", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/log", .method = HTTP_GET, .handler = log_get},
        {.uri = "/api/setpoints", .method = HTTP_POST, .handler = setpoints_post},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = session_stop_post},
        {.uri = "/api/tx_enable", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = update_post},
        {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_get},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post},
        {.uri = "/api/can/raw", .method = HTTP_POST, .handler = can_raw_post},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = reboot_post},
        {.uri = "/api/update", .method = HTTP_POST, .handler = update_post},
        {.uri = "/api/session/start", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/session/stop", .method = HTTP_POST, .handler = session_stop_post},
        {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_get},
    };
    for (size_t i = 0; i < sizeof(uri) / sizeof(uri[0]); ++i) {
        err = httpd_register_uri_handler(s_server, &uri[i]);
        if (err != ESP_OK) {
            (void)httpd_stop(s_server);
            s_server = NULL;
            return err;
        }
    }
    ESP_LOGW(TAG, "softAP started SSID:%s password:%s channel:%d", CUBE_WIFI_SSID, CUBE_WIFI_PASSWORD[0]?CUBE_WIFI_PASSWORD:"(open)", s_ap_channel);
    ESP_LOGI(TAG, "Wi-Fi AP=%s UI=http://192.168.4.1/", CUBE_WIFI_SSID);
    return ESP_OK;
}
