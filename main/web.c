#include "soc/rtc_cntl_reg.h"
#include "web.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "config.h"
#include "app_state.h"
#include "sm_relay.h"
#include "can_gbt.h"
#include "safe_ota.h"
#include "display.h"
#include "session_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

extern const uint8_t web_index_html[];
extern const size_t web_index_html_len;
extern const uint8_t web_style_css[];
extern const size_t web_style_css_len;
extern const uint8_t web_app_js[];
extern const size_t web_app_js_len;

static const char *TAG = "WEB";
static httpd_handle_t s_server;
static int s_ap_clients;
static int s_ap_channel = CUBE_WIFI_CHANNEL;
static char s_scan_ssid[16][33];
static int s_scan_n;
static char s_sta_ip[20] = "--";
static char s_join_ssid[33];
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;
static char s_dl_url[320];
static char s_dl_user[40];
static char s_dl_pass[40];

int web_get_ap_channel(void) { return s_ap_channel; }
int web_ap_clients(void) { return s_ap_clients; }
int web_scan_count(void) { return s_scan_n; }
const char *web_scan_ssid(int index)
{
    if (index < 0 || index >= s_scan_n) return "";
    return s_scan_ssid[index];
}
const char *web_sta_ip(void) { return s_sta_ip; }

static const char *json_bool(int v) { return v ? "true" : "false"; }

static void set_security_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(req, "Content-Security-Policy",
        "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
}

static void set_no_cache(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
}

static int auth(httpd_req_t *req)
{
    char buf[48];
    if (!CUBE_WEB_KEY[0]) return 1;
    if (httpd_req_get_hdr_value_str(req, "X-Cube-Key", buf, sizeof(buf)) == ESP_OK &&
        strcmp(buf, CUBE_WEB_KEY) == 0) return 1;
    return 0;
}

static esp_err_t forbidden(httpd_req_t *req)
{
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"forbidden\"}");
}

static esp_err_t send_asset(httpd_req_t *req, const uint8_t *body, size_t len, const char *type)
{
    set_security_headers(req);
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, (const char *)body, len);
}

static esp_err_t root_get(httpd_req_t *req)
{
    return send_asset(req, web_index_html, web_index_html_len, "text/html; charset=utf-8");
}
static esp_err_t css_get(httpd_req_t *req)
{
    return send_asset(req, web_style_css, web_style_css_len, "text/css; charset=utf-8");
}
static esp_err_t js_get(httpd_req_t *req)
{
    return send_asset(req, web_app_js, web_app_js_len, "application/javascript; charset=utf-8");
}
static esp_err_t favicon_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t captive_html_get(httpd_req_t *req)
{
    set_security_headers(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, "<!DOCTYPE html><html><head><title>Success</title></head><body>Success</body></html>");
}

static esp_err_t captive_text_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    if (strstr(req->uri, "ncsi")) return httpd_resp_sendstr(req, "Microsoft NCSI");
    return httpd_resp_sendstr(req, "Microsoft Connect Test");
}

static esp_err_t portal_catchall_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t probe_discard(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static int read_body(httpd_req_t *req, char *buf, size_t cap)
{
    int left = req->content_len;
    int got = 0;
    if (left < 0) left = 0;
    if ((size_t)left >= cap) left = (int)cap - 1;
    while (got < left) {
        int n = httpd_req_recv(req, buf + got, (size_t)(left - got));
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) break;
        got += n;
    }
    buf[got] = 0;
    return got;
}

static const char *find_json_key(const char *body, const char *key)
{
    char pat[48];
    const char *p;
    if (!body || !key) return NULL;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(body, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == '"') p++;
    return p;
}

static void field_copy(const char *body, const char *key, char *out, size_t out_len)
{
    const char *p = body;
    size_t klen = strlen(key);
    out[0] = 0;
    while (p && *p) {
        if ((p == body || p[-1] == '&' || p[-1] == '?') && strncmp(p, key, klen) == 0) break;
        p = strchr(p + 1, '&');
        if (p) ++p;
    }
    if (!p || strncmp(p, key, klen) != 0) return;
    p += klen;
    size_t i = 0;
    while (*p && *p != '&' && i + 1 < out_len) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = {p[1], p[2], 0};
            out[i++] = (char)strtol(hex, NULL, 16);
            p += 3;
        } else if (*p == '+') {
            out[i++] = ' ';
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = 0;
}

static void hex8(const uint8_t *d, int n, char *out, size_t cap)
{
    size_t i = 0;
    out[0] = 0;
    for (int k = 0; k < n && i + 2 < cap; ++k) {
        snprintf(out + i, cap - i, "%02X", d[k]);
        i += 2;
    }
}

static esp_err_t status_get(httpd_req_t *req)
{
    cube_state_snapshot_t st;
    char data[20];
    char cem[12];
    char body[2048];
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    int online;
    long power;
    uint32_t stage_ms;
    app_state_snapshot(&st);
    hex8(st.last.data, st.last.dlc > 8 ? 8 : st.last.dlc, data, sizeof(data));
    hex8(st.cem_data, 4, cem, sizeof(cem));
    online = st.last_charger_rx_ms != 0 && (now - st.last_charger_rx_ms) < 3000u;
    power = (long)st.ccs_output_voltage_raw * (long)st.ccs_output_current_0p1a / 100L;
    if (power < 0) power = -power;
    stage_ms = (st.charger_stage_ms && now > st.charger_stage_ms) ? (now - st.charger_stage_ms) : 0;
    snprintf(body, sizeof(body),
        "{\"ok\":true,\"fw\":\"%s\",\"version\":\"%s\",\"can\":%s,\"tx_enabled\":%s,"
        "\"rx_count\":%lu,\"tx_count\":%lu,\"last_id\":\"0x%08lX\",\"last_dlc\":%u,\"last_data\":\"%s\","
        "\"charger_online\":%s,\"stage\":\"%s\",\"stage_ms\":%lu,\"main_state\":%u,\"main_state_name\":\"%s\","
        "\"substate\":%u,\"relay_a\":%u,\"relay_b\":%u,\"handoff_done\":%s,\"session_active\":%s,"
        "\"battery_voltage_0p1v\":%u,\"battery_current_0p1a\":%d,\"soc\":%u,"
        "\"cml_max_voltage_0p1v\":%u,\"cml_min_voltage_0p1v\":%u,\"cml_max_current_0p1a\":%d,\"cml_min_current_0p1a\":%d,"
        "\"ccs_voltage_0p1v\":%u,\"ccs_current_0p1a\":%d,\"ccs_time_min\":%u,\"ccs_charge_permit\":%u,\"ccs_status_raw\":%u,"
        "\"power_w\":%ld,\"cro_status_raw\":%u,\"cst_status_raw\":%u,\"csd_time_min\":%u,\"csd_energy_0p1kwh\":%u,"
        "\"bcl_voltage_0p1v\":%u,\"bcl_current_0p1a\":%d,\"bcl_mode\":%u,"
        "\"ble\":%s,\"notify\":%s,\"indicate\":%s,\"display\":%s,\"touch\":%s,\"touch_pressed\":%s,"
        "\"protocol\":\"%s\",\"voltage_v\":%.1f,\"current_a\":%.1f,\"power_w\":%ld,\"time_min\":%u,"
        "\"proto_n\":%u,\"proto_lock\":%s,\"crm_status\":%u,"
        "\"last_crm_ms\":%lu,\"last_cml_ms\":%lu,\"last_cro_ms\":%lu,\"last_ccs_ms\":%lu,"
        "\"last_cst_ms\":%lu,\"last_csd_ms\":%lu,\"last_cem_ms\":%lu,\"cem\":\"%s\","
        "\"energy_kwh\":%.3f,\"vin\":\"%s\",\"cap_kwh\":%.2f,\"ota_pct\":%d,\"ap_channel\":%d,\"ap_clients\":%u,\"sta_ip\":\"%s\"}",
        CUBE_FW_NAME, CUBE_FW_VERSION, json_bool(st.can_up), json_bool(st.tx_enabled),
        (unsigned long)st.rx_count, (unsigned long)st.tx_count, (unsigned long)st.last.id,
        (unsigned)st.last.dlc, data,
        json_bool(online), cube_charge_stage_name(st.charger_stage), (unsigned long)stage_ms,
        (unsigned)st.main_state, cube_main_state_name(st.main_state),
        (unsigned)st.substate, (unsigned)st.relay_a, (unsigned)st.relay_b,
        json_bool(st.handoff_done), json_bool(st.session_active),
        (unsigned)st.battery_voltage_0p1v, (int)st.battery_current_0p1a, (unsigned)st.soc_percent,
        (unsigned)st.cml_max_voltage_raw, (unsigned)st.cml_min_voltage_raw,
        (int)st.cml_max_current_0p1a, (int)st.cml_min_current_0p1a,
        (unsigned)st.ccs_output_voltage_raw, (int)st.ccs_output_current_0p1a,
        (unsigned)st.ccs_charge_time_min, (unsigned)st.ccs_charge_permit, (unsigned)st.ccs_status_raw,
        power, (unsigned)st.cro_status_raw, (unsigned)st.cst_status_raw,
        (unsigned)st.csd_time_min, (unsigned)st.csd_energy_0p1kwh,
        (unsigned)st.bcl_requested_voltage_raw, (int)st.bcl_requested_current_0p1a, (unsigned)st.bcl_mode,
        json_bool(st.ble_connected), json_bool(st.ble_notify_enabled), json_bool(st.ble_indicate_enabled),
        json_bool(st.display_ready), json_bool(st.touch_enabled), json_bool(st.touch_pressed),
        cube_protocol_name(st.protocol),
        (double)st.ccs_output_voltage_raw / 10.0, (double)st.ccs_output_current_0p1a / 10.0,
        power, (unsigned)st.ccs_charge_time_min,
        (unsigned)st.protocol, json_bool(app_state_protocol_locked()), (unsigned)st.crm_status,
        (unsigned long)st.last_crm_ms, (unsigned long)st.last_cml_ms, (unsigned long)st.last_cro_ms,
        (unsigned long)st.last_ccs_ms, (unsigned long)st.last_cst_ms, (unsigned long)st.last_csd_ms,
        (unsigned long)st.last_cem_ms, cem,
        (double)app_state_energy_kwh(), app_state_vin(), (double)app_state_cap_kwh(), safe_ota_percent(),
        s_ap_channel, (unsigned)s_ap_clients, s_sta_ip);
    set_security_headers(req);
    set_no_cache(req);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t log_get(httpd_req_t *req)
{
    char buf[1200];
    int n = app_state_log_text(buf, (int)sizeof(buf));
    if (n < 0) n = 0;
    set_security_headers(req);
    set_no_cache(req);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, buf, n);
}

static esp_err_t can_text_get(httpd_req_t *req)
{
    cube_can_frame_t hist[12];
    size_t n = 0;
    char body[700];
    size_t used = 0;
    gbt_can_get_history(hist, 12, &n);
    body[0] = 0;
    for (size_t i = 0; i < n && used + 40 < sizeof(body); ++i) {
        char hex[20];
        hex8(hist[i].data, hist[i].dlc > 8 ? 8 : hist[i].dlc, hex, sizeof(hex));
        used += (size_t)snprintf(body + used, sizeof(body) - used, "%lu %08lX %u %s\n",
            (unsigned long)hist[i].timestamp_ms, (unsigned long)hist[i].id,
            (unsigned)hist[i].dlc, hex);
    }
    set_security_headers(req);
    set_no_cache(req);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t can_recent_get(httpd_req_t *req)
{
    cube_can_frame_t hist[12];
    size_t n = 0;
    char body[900];
    size_t used;
    gbt_can_get_history(hist, 12, &n);
    used = (size_t)snprintf(body, sizeof(body), "{\"ok\":true,\"count\":%u,\"frames\":[", (unsigned)n);
    for (size_t i = 0; i < n && used + 80 < sizeof(body); ++i) {
        char hex[20];
        hex8(hist[i].data, hist[i].dlc > 8 ? 8 : hist[i].dlc, hex, sizeof(hex));
        used += (size_t)snprintf(body + used, sizeof(body) - used,
            "%s{\"id\":\"0x%08lX\",\"dlc\":%u,\"flags\":%lu,\"ms\":%lu,\"data\":\"%s\"}",
            i ? "," : "", (unsigned long)hist[i].id, (unsigned)hist[i].dlc,
            (unsigned long)hist[i].flags, (unsigned long)hist[i].timestamp_ms, hex);
    }
    if (used > sizeof(body) - 3) used = sizeof(body) - 3;   /* snprintf returns the untruncated length */
    snprintf(body + used, sizeof(body) - used, "]}");
    set_security_headers(req);
    set_no_cache(req);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t session_start_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    sm_start_session();
    gbt_can_set_tx_enabled(true);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    if (gbt_can_get_tx_enabled()) {
        return httpd_resp_sendstr(req,
            "{\"ok\":true,\"tx_enabled\":true,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}");
    }
    return httpd_resp_sendstr(req,
        "{\"ok\":true,\"tx_enabled\":false,\"tx_compiled\":true,\"web_key_required\":true,\"wifi_auth\":\"WPA2-PSK\"}");
}

static esp_err_t session_stop_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    sm_stop_session();
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"action\":\"stop\"}");
}

static esp_err_t setpoints_post(httpd_req_t *req)
{
    char body[240];
    char tmp[24];
    int v = 240, i = 50, soc = 20, proto = -1;
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    read_body(req, body, sizeof(body));
    field_copy(body, "v=", tmp, sizeof(tmp)); if (tmp[0]) v = atoi(tmp);
    field_copy(body, "i=", tmp, sizeof(tmp)); if (tmp[0]) i = atoi(tmp);
    field_copy(body, "soc=", tmp, sizeof(tmp)); if (tmp[0]) soc = atoi(tmp);
    field_copy(body, "cap=", tmp, sizeof(tmp));
    if (tmp[0]) app_state_set_cap_kwh((float)atof(tmp));
    field_copy(body, "proto=", tmp, sizeof(tmp)); if (tmp[0]) proto = atoi(tmp);
    field_copy(body, "vin=", tmp, sizeof(tmp)); if (tmp[0]) app_state_set_vin(tmp);
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    if (i < 0) i = 0;
    if (i > 400) i = 400;
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    app_state_set_bcl((uint16_t)(v * 10), (int16_t)(-(i * 10)), 1);
    app_state_set_soc((uint8_t)soc);
    if (proto >= 0 && proto <= 3) app_state_set_protocol((uint8_t)proto);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"action\":\"start\"}");
}

static esp_err_t settings_get(httpd_req_t *req)
{
    set_security_headers(req);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t settings_post(httpd_req_t *req)
{
    char body[180];
    const char *p;
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    read_body(req, body, sizeof(body));
    p = find_json_key(body, "relay_test");
    if (p && (p[0] == 't' || p[0] == '1')) sm_request_relay_test();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t can_raw_post(httpd_req_t *req)
{
    char body[160];
    char idb[16] = {0};
    char data[40] = {0};
    unsigned long id = 0;
    uint8_t raw[8] = {0};
    int n = 0;
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    read_body(req, body, sizeof(body));
    field_copy(body, "id=", idb, sizeof(idb));
    field_copy(body, "data=", data, sizeof(data));
    if (!idb[0]) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_can_frame\"}");
    }
    id = strtoul(idb, NULL, 16);
    for (size_t k = 0; data[k] && data[k + 1] && n < 8; k += 2) {
        char h[3] = {data[k], data[k + 1], 0};
        raw[n++] = (uint8_t)strtoul(h, NULL, 16);
    }
    esp_err_t err = gbt_can_send((uint32_t)id, raw, (uint8_t)n);
    char out[80];
    snprintf(out, sizeof(out), "{\"ok\":%s,\"error\":\"%s\",\"id\":\"0x%08lX\",\"dlc\":%lu}",
             err == ESP_OK ? "true" : "false", err == ESP_OK ? "" : esp_err_to_name(err),
             id, (unsigned long)n);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, out);
}

static int req_reader(uint8_t *buf, size_t max_len, void *ctx)
{
    int n = httpd_req_recv((httpd_req_t *)ctx, (char *)buf, max_len);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) return -1;
    return n;
}

static esp_err_t update_post(httpd_req_t *req)
{
    uint8_t first[1024];
    int got;
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    if (req->content_len < 24 || req->content_len > (int)CUBE_HTTP_MAX_UPLOAD) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_size\"}");
    }
    got = httpd_req_recv(req, (char *)first, sizeof(first));
    if (got == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"upload_timeout\"}");
    }
    if (got <= 0 || first[0] != 0xE9) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"not_esp_image\"}");
    }
    display_ota_progress(2, "OTA");
    esp_err_t err = safe_ota_write_stream(first, (size_t)got, req_reader, req, (size_t)req->content_len);
    httpd_resp_set_type(req, "application/json");
    if (err != ESP_OK) {
        display_ota_hide();
        char out[64];
        snprintf(out, sizeof(out), "{\"ok\":false,\"error\":\"%s\"}", esp_err_to_name(err));
        return httpd_resp_sendstr(req, out);
    }
    httpd_resp_sendstr(req, "{\"ok\":true}");
    display_ota_progress(100, "REBOOT");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
    return ESP_OK;
}

static int http_read_cb(uint8_t *buf, size_t max_len, void *ctx)
{
    int n = esp_http_client_read((esp_http_client_handle_t)ctx, (char *)buf, (int)max_len);
    return n;
}

static void ota_url_task(void *arg)
{
    (void)arg;
    display_ota_progress(2, "NET");
    esp_http_client_config_t cfg = {
        .url = s_dl_url,
        .username = s_dl_user[0] ? s_dl_user : NULL,
        .password = s_dl_pass[0] ? s_dl_pass : NULL,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .timeout_ms = 20000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client || esp_http_client_open(client, 0) != ESP_OK) {
        display_ota_hide();
        if (client) esp_http_client_cleanup(client);
        vTaskDelete(NULL);
        return;
    }
    (void)esp_http_client_fetch_headers(client);
    int total = esp_http_client_get_content_length(client);
    uint8_t first[1024];
    int got = esp_http_client_read(client, (char *)first, sizeof(first));
    if (got <= 0 || first[0] != 0xE9 || total < got) {
        display_ota_hide();
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
        return;
    }
    esp_err_t err = safe_ota_write_stream(first, (size_t)got, http_read_cb, client, (size_t)total);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err == ESP_OK) {
        display_ota_progress(100, "REBOOT");
        vTaskDelay(pdMS_TO_TICKS(600));
        esp_restart();
    }
    display_ota_hide();
    vTaskDelete(NULL);
}

static esp_err_t ota_url_post(httpd_req_t *req)
{
    char body[400];
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    read_body(req, body, sizeof(body));
    field_copy(body, "url=", s_dl_url, sizeof(s_dl_url));
    field_copy(body, "user=", s_dl_user, sizeof(s_dl_user));
    field_copy(body, "pass=", s_dl_pass, sizeof(s_dl_pass));
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    if (strncmp(s_dl_url, "http://", 7) != 0 && strncmp(s_dl_url, "https://", 8) != 0) {
        return httpd_resp_sendstr(req, "need http://host/share/file.bin or \\\\host\\share\\file.bin");
    }
    if (xTaskCreate(ota_url_task, "otaurl", 6144, NULL, 4, NULL) != pdPASS) {
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid_body\"}");
    }
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t reboot_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(400));
    esp_restart();
    return ESP_OK;
}

static esp_err_t logs_list_get(httpd_req_t *req)
{
    char body[900];
    int n = session_log_count();
    size_t used;
    static const char *proto[] = {"AUTO", "2015", "2011", "2023"};
    static const char *why[] = {"update", "proto", "stop"};
    set_security_headers(req);
    set_no_cache(req);
    used = (size_t)snprintf(body, sizeof(body), "{\"ok\":true,\"count\":%d,\"logs\":[", n);
    for (int i = 0; i < n && used + 80 < sizeof(body); ++i) {
        int seq = 0, bytes = 0, protocol = 0, reason = 0;
        session_log_info(i, &seq, &bytes, &protocol, &reason);
        used += (size_t)snprintf(body + used, sizeof(body) - used,
            "%s{\"n\":%d,\"seq\":%d,\"bytes\":%d,\"protocol\":\"%s\",\"reason\":\"%s\"}",
            i ? "," : "", i + 1, seq, bytes,
            proto[protocol > 3 ? 0 : protocol], why[reason > 2 ? 0 : reason]);
    }
    snprintf(body + used, sizeof(body) - used, "]}");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t logs_last_get(httpd_req_t *req)
{
    set_security_headers(req);
    set_no_cache(req);
    return session_log_send_last(req);
}

static esp_err_t logs_zip_get(httpd_req_t *req)
{
    set_security_headers(req);
    set_no_cache(req);
    return session_log_send_zip(req);
}

static esp_err_t logs_file_get(httpd_req_t *req)
{
    char q[40] = {0};
    char nbuf[8] = {0};
    int n = 0;
    set_security_headers(req);
    set_no_cache(req);
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "n", nbuf, sizeof(nbuf)) == ESP_OK) {
        n = atoi(nbuf);
        if (n < 0) n = 0;
    }
    return session_log_send_one(req, n);
}

static void wifi_ap_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == WIFI_EVENT_AP_STACONNECTED && data) {
        wifi_event_ap_staconnected_t *ev = data;
        s_ap_clients++;
        ESP_LOGI(TAG, "STA connected %02X:%02X:%02X:%02X:%02X:%02X AID=%u heap=%u",
                 ev->mac[0], ev->mac[1], ev->mac[2], ev->mac[3], ev->mac[4], ev->mac[5],
                 (unsigned)ev->aid, (unsigned)esp_get_free_heap_size());
    } else if (id == WIFI_EVENT_AP_START) {
        ESP_LOGW(TAG, "AP_START event: radio is beaconing, heap=%u min=%u",
                 (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
    } else if (id == WIFI_EVENT_AP_STOP) {
        ESP_LOGW(TAG, "AP_STOP event");
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED && data) {
        wifi_event_ap_stadisconnected_t *ev = data;
        if (s_ap_clients > 0) s_ap_clients--;
        ESP_LOGW(TAG, "STA disconnected %02X:%02X:%02X:%02X:%02X:%02X AID=%u",
                 ev->mac[0], ev->mac[1], ev->mac[2], ev->mac[3], ev->mac[4], ev->mac[5],
                 (unsigned)ev->aid);
    }
}

static void wifi_ip_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == IP_EVENT_STA_GOT_IP && data) {
        ip_event_got_ip_t *ev = data;
        uint32_t a = ev->ip_info.ip.addr;
        snprintf(s_sta_ip, sizeof(s_sta_ip), "%u.%u.%u.%u",
                 (unsigned)(a & 255u), (unsigned)((a >> 8) & 255u),
                 (unsigned)((a >> 16) & 255u), (unsigned)((a >> 24) & 255u));
        ESP_LOGW(TAG, "STA got IP %s", s_sta_ip);
    }
}

static void fill_ap(wifi_config_t *ap)
{
    memset(ap, 0, sizeof(*ap));
    snprintf((char *)ap->ap.ssid, sizeof(ap->ap.ssid), "%s", CUBE_WIFI_SSID);
    ap->ap.ssid_len = (uint8_t)strlen(CUBE_WIFI_SSID);
    ap->ap.channel = CUBE_WIFI_CHANNEL;
    s_ap_channel = ap->ap.channel;
    ap->ap.max_connection = CUBE_WIFI_MAX_CONNECTIONS;
    ap->ap.ssid_hidden = 0;
    ap->ap.beacon_interval = 100;
    /* PMF off. With capable=1 the C3 beacon is dropped by many phones,
       so Cube is up in the log and invisible in the scan list. */
    ap->ap.pmf_cfg.capable = false;
    ap->ap.pmf_cfg.required = false;
    ap->ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap->ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    snprintf((char *)ap->ap.password, sizeof(ap->ap.password), "%s", CUBE_WIFI_PASSWORD);
}

/* Country and power-save only. set_protocol / set_bandwidth / set_country
   after esp_wifi_start stop beacons on this C3 while the log still says started. */
static void ap_radio_before_start(void)
{
    wifi_country_t country = {
        .cc = "CN", .schan = 1, .nchan = 13, .policy = WIFI_COUNTRY_POLICY_MANUAL
    };
    esp_err_t e1 = esp_wifi_set_ps(WIFI_PS_NONE);
    esp_err_t e2 = esp_wifi_set_country(&country);
    /* Before esp_wifi_start only. Overrides whatever esp_wifi_init loaded. */
    esp_err_t e3 = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_err_t e4 = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    ESP_LOGW(TAG, "radio pre-start ps=%s country=%s proto=%s bw=%s",
             esp_err_to_name(e1), esp_err_to_name(e2), esp_err_to_name(e3), esp_err_to_name(e4));
}

static esp_err_t ap_apply_config(void)
{
    wifi_config_t ap;
    fill_ap(&ap);
    return esp_wifi_set_config(WIFI_IF_AP, &ap);
}

esp_err_t web_scan_now(void)
{
    wifi_scan_config_t scan = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err;
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (!s_sta_netif) return ESP_ERR_NO_MEM;
        if (esp_wifi_set_mode(WIFI_MODE_APSTA) == ESP_OK) (void)ap_apply_config();
    }
    err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return err;
    }
    uint16_t n = 16;
    wifi_ap_record_t rec[16];
    memset(rec, 0, sizeof(rec));
    if (esp_wifi_scan_get_ap_records(&n, rec) != ESP_OK) n = 0;
    s_scan_n = (int)n;
    for (int i = 0; i < s_scan_n; ++i) {
        memcpy(s_scan_ssid[i], rec[i].ssid, 32);
        s_scan_ssid[i][32] = 0;
    }
    ESP_LOGI(TAG, "scan done n=%d", s_scan_n);
    return ESP_OK;
}

esp_err_t web_sta_join(const char *ssid, const char *password)
{
    wifi_config_t sta = {0};
    esp_err_t err;
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (!s_sta_netif) return ESP_ERR_NO_MEM;
    }
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = ap_apply_config();
    if (err != ESP_OK) return err;
    snprintf((char *)sta.sta.ssid, sizeof(sta.sta.ssid), "%s", ssid);
    if (password) snprintf((char *)sta.sta.password, sizeof(sta.sta.password), "%s", password);
    sta.sta.threshold.authmode = (password && strlen(password) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    snprintf(s_join_ssid, sizeof(s_join_ssid), "%s", ssid);
    snprintf(s_sta_ip, sizeof(s_sta_ip), "wait");
    err = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (err != ESP_OK) return err;
    (void)esp_wifi_disconnect();
    err = esp_wifi_connect();
    ESP_LOGW(TAG, "STA join '%s' -> %s", ssid, esp_err_to_name(err));
    return err;
}

esp_err_t web_wifi_restart(void)
{
    esp_err_t err;
    ESP_LOGW(TAG, "WiFi restart requested");
    (void)esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    (void)esp_wifi_set_mode(WIFI_MODE_AP);
    ap_radio_before_start();   /* after set_mode: set_protocol(WIFI_IF_AP) needs the AP interface */
    err = ap_apply_config();
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        (void)esp_wifi_set_max_tx_power(CUBE_WIFI_TX_POWER_QDBM);
        err = ESP_OK;
        ESP_LOGW(TAG, "WiFi restarted SSID=%s", CUBE_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "WiFi restart failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t wifi_scan_get(httpd_req_t *req)
{
    char body[600];
    size_t used;
    set_security_headers(req);
    set_no_cache(req);
    (void)web_scan_now();
    used = (size_t)snprintf(body, sizeof(body), "{\"ok\":true,\"nets\":[");
    for (int i = 0; i < s_scan_n && used + 40 < sizeof(body); ++i) {
        used += (size_t)snprintf(body + used, sizeof(body) - used, "%s\"%s\"", i ? "," : "", s_scan_ssid[i]);
    }
    snprintf(body + used, sizeof(body) - used, "]}");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t wifi_join_post(httpd_req_t *req)
{
    char body[160];
    char ssid[33];
    char pass[65];
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    read_body(req, body, sizeof(body));
    field_copy(body, "ssid=", ssid, sizeof(ssid));
    field_copy(body, "pass=", pass, sizeof(pass));
    httpd_resp_set_type(req, "application/json");
    if (web_sta_join(ssid, pass) != ESP_OK) return httpd_resp_sendstr(req, "{\"ok\":false}");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static void dns_task(void *arg)
{
    int sock;
    struct sockaddr_in local;
    (void)arg;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGW(TAG, "dns socket fail");
        vTaskDelete(NULL);
        return;
    }
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(53);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) != 0) {
        ESP_LOGW(TAG, "dns bind fail");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGW(TAG, "dns up");
    for (;;) {
        uint8_t pkt[512];
        struct sockaddr_in from;
        socklen_t flen = sizeof(from);
        int n = recvfrom(sock, pkt, sizeof(pkt), 0, (struct sockaddr *)&from, &flen);
        int i, qtype;
        if (n < 12 || n > (int)sizeof(pkt) - 16) continue;
        if ((pkt[2] & 0x80) != 0) continue;
        if (((pkt[2] >> 3) & 0x0F) != 0) continue;
        if (pkt[4] == 0 && pkt[5] == 0) continue;
        i = 12;
        while (i < n && pkt[i] != 0) {
            int lab = pkt[i];
            if ((lab & 0xC0) == 0xC0) { i += 2; goto named; }
            if (lab > 63 || i + 1 + lab >= n) goto skip;
            i += 1 + lab;
        }
        if (i >= n || pkt[i] != 0) continue;
        i++;
named:
        if (i + 4 > n) continue;
        qtype = ((int)pkt[i] << 8) | pkt[i + 1];
        i += 4;
        pkt[2] = (uint8_t)((pkt[2] & 0x01) | 0x84);
        pkt[3] = 0;
        pkt[8] = 0; pkt[9] = 0; pkt[10] = 0; pkt[11] = 0;
        if (qtype != 1) {
            pkt[6] = 0; pkt[7] = 0;
            (void)sendto(sock, pkt, n, 0, (struct sockaddr *)&from, flen);
            continue;
        }
        if (i + 16 > (int)sizeof(pkt)) continue;
        pkt[6] = 0; pkt[7] = 1;
        pkt[i++] = 0xC0; pkt[i++] = 0x0C;
        pkt[i++] = 0x00; pkt[i++] = 0x01;
        pkt[i++] = 0x00; pkt[i++] = 0x01;
        pkt[i++] = 0x00; pkt[i++] = 0x00; pkt[i++] = 0x00; pkt[i++] = 0x3C;
        pkt[i++] = 0x00; pkt[i++] = 0x04;
        pkt[i++] = 192; pkt[i++] = 168; pkt[i++] = 4; pkt[i++] = 1;
        (void)sendto(sock, pkt, i, 0, (struct sockaddr *)&from, flen);
skip:
        ;
    }
}

static esp_err_t http_open(httpd_handle_t hd, int sockfd)
{
    int nodelay = 1;
    int snd = 16384;
    (void)hd;
    (void)setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    (void)setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    return ESP_OK;
}

void web_boot_original(void)
{
    const esp_partition_t *orig = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!orig) return;
    REG_WRITE(RTC_CNTL_STORE0_REG, 0x01D0F00Du);
    esp_ota_set_boot_partition(orig);
    esp_restart();
}

static esp_err_t boot_original_post(httpd_req_t *req)
{
    set_security_headers(req);
    if (!auth(req)) return forbidden(req);
    httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(400));
    web_boot_original();
    return ESP_OK;
}

esp_err_t web_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wifi_ap_event_handler, NULL);
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &wifi_ap_event_handler, NULL);
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &wifi_ap_event_handler, NULL);
    (void)esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &wifi_ap_event_handler, NULL);
    ESP_LOGW(TAG, "wifi init: heap=%u largest=%u", (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    (void)esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_ip_handler, NULL);
    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 6;
    cfg.dynamic_rx_buf_num = 8;
    cfg.dynamic_tx_buf_num = 8;
    cfg.cache_tx_buf_num = 0;   /* PSRAM-only feature; the cube has no PSRAM */
    cfg.ampdu_rx_enable = 0;
    cfg.ampdu_tx_enable = 0;
    cfg.amsdu_tx_enable = 0;
    cfg.rx_ba_win = 6;
    cfg.mgmt_sbuf_num = 8;
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        wifi_init_config_t fallback = WIFI_INIT_CONFIG_DEFAULT();
        ESP_LOGW(TAG, "slim wifi init failed: %s", esp_err_to_name(err));
        cfg = fallback;
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    }
    ESP_LOGW(TAG, "wifi buffers static_rx=%d dyn_rx=%d dyn_tx=%d ampdu_rx=%d",
             cfg.static_rx_buf_num, cfg.dynamic_rx_buf_num, cfg.dynamic_tx_buf_num,
             cfg.ampdu_rx_enable);
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    ESP_LOGW(TAG, "wifi storage RAM -> %s", esp_err_to_name(err));
    if ((err = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK) {
        ESP_LOGE(TAG, "set_mode AP failed: %s", esp_err_to_name(err));
        return err;
    }
    ap_radio_before_start();   /* after set_mode: set_protocol(WIFI_IF_AP) needs the AP interface */
    if ((err = ap_apply_config()) != ESP_OK) {
        ESP_LOGE(TAG, "set_config AP failed: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = esp_wifi_start()) != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }
    {
        esp_err_t pe = esp_wifi_set_max_tx_power(CUBE_WIFI_TX_POWER_QDBM);
        int8_t pw = 0;
        (void)esp_wifi_get_max_tx_power(&pw);
        ESP_LOGW(TAG, "tx power req=%d got=%d (0.25 dBm) -> %s heap=%u",
                 CUBE_WIFI_TX_POWER_QDBM, (int)pw, esp_err_to_name(pe), (unsigned)esp_get_free_heap_size());
    }
    {
        wifi_config_t chk = {0};
        if (esp_wifi_get_config(WIFI_IF_AP, &chk) == ESP_OK) {
            ESP_LOGW(TAG, "AP check auth=%d pmf=%d key_len=%u ch=%u hidden=%u",
                     (int)chk.ap.authmode, (int)chk.ap.pmf_cfg.capable,
                     (unsigned)strlen((const char *)chk.ap.password),
                     (unsigned)chk.ap.channel, (unsigned)chk.ap.ssid_hidden);
        }
    }
    ESP_LOGW(TAG, "AP only SSID=%s ch=%u", CUBE_WIFI_SSID, (unsigned)CUBE_WIFI_CHANNEL);

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.max_uri_handlers = 48;
    hc.stack_size = 12288;
    hc.recv_wait_timeout = 10;
    hc.send_wait_timeout = 4;
    hc.lru_purge_enable = true;
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.max_open_sockets = 4;
    hc.open_fn = http_open;
    if ((err = httpd_start(&s_server, &hc)) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }
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
        {.uri = "/api/wifi/join", .method = HTTP_POST, .handler = wifi_join_post},
        {.uri = "/api/boot/original", .method = HTTP_POST, .handler = boot_original_post},
        {.uri = "/api/can/recent", .method = HTTP_GET, .handler = can_recent_get},
        {.uri = "/api/can", .method = HTTP_GET, .handler = can_text_get},
        {.uri = "/api/start", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/log", .method = HTTP_GET, .handler = log_get},
        {.uri = "/api/logs/last", .method = HTTP_GET, .handler = logs_last_get},
        {.uri = "/api/logs/file", .method = HTTP_GET, .handler = logs_file_get},
        {.uri = "/api/logs.zip", .method = HTTP_GET, .handler = logs_zip_get},
        {.uri = "/api/logs", .method = HTTP_GET, .handler = logs_list_get},
        {.uri = "/api/setpoints", .method = HTTP_POST, .handler = setpoints_post},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = session_stop_post},
        {.uri = "/api/tx_enable", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/ota", .method = HTTP_POST, .handler = update_post},
        {.uri = "/api/ota/url", .method = HTTP_POST, .handler = ota_url_post},
        {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_get},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post},
        {.uri = "/api/can/raw", .method = HTTP_POST, .handler = can_raw_post},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = reboot_post},
        {.uri = "/api/update", .method = HTTP_POST, .handler = update_post},
        {.uri = "/api/session/start", .method = HTTP_POST, .handler = session_start_post},
        {.uri = "/api/session/stop", .method = HTTP_POST, .handler = session_stop_post},
        {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_get},
        {.uri = "/*", .method = HTTP_POST, .handler = probe_discard},
        {.uri = "/*", .method = HTTP_PUT, .handler = probe_discard},
        {.uri = "/*", .method = HTTP_HEAD, .handler = probe_discard},
    };
    for (size_t i = 0; i < sizeof(uri) / sizeof(uri[0]); ++i) {
        err = httpd_register_uri_handler(s_server, &uri[i]);
        if (err != ESP_OK) {
            (void)httpd_stop(s_server);
            s_server = NULL;
            return err;
        }
    }
    ESP_LOGW(TAG, "softAP started SSID:%s channel:%d", CUBE_WIFI_SSID, s_ap_channel);
    ESP_LOGI(TAG, "Wi-Fi AP=%s UI=http://192.168.4.1/", CUBE_WIFI_SSID);
    if (xTaskCreate(dns_task, "dns", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "dns task fail");
    }
    return ESP_OK;
}
