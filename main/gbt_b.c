/*
 * GB/T 27930-2023 category B, vehicle side.
 * Wire format follows the OpenChaoJi reference (CEC implementation of the standard):
 *   version nego  P=3 PF=0x38
 *   long info     P=6 PF=0x34
 *   reliable short P=4 PF=0x35
 *   unreliable short P=6 PF=0x36
 *   reliable ctrl P=3 PF=0x37  (0=SM_ACK, 1=LM_ACK, 2=LM_NACK, 3=LM_EndACK)
 * Current and voltage in B messages have resolution 0.1 and offset 0
 * (unlike category A, where current is offset by -400 A).
 */
#include "gbt_b.h"

#include <string.h>

#include "app_state.h"
#include "can_gbt.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "GBT_B";

#define B_SA_SECC 0x5Fu
#define B_SA_EVCC 0x25u
#define B_PF_LM   0x34u
#define B_PF_RSM  0x35u
#define B_PF_URM  0x36u
#define B_PF_CTL  0x37u
#define B_PF_VER  0x38u

#define B_PGI_PHASE_SECC 0x01u
#define B_PGI_PHASE_EVCC 0x02u
#define B_PGI_STOP_SECC  0x03u
#define B_PGI_STOP_EVCC  0x04u
#define B_PGI_RELAY_SECC 0x05u
#define B_PGI_RELAY_EVCC 0x06u
#define B_PGI_LATCH_EVCC 0x07u
#define B_PGI_FUNC_SECC  0x11u
#define B_PGI_FUNC_EVCC  0x12u
#define B_PGI_PAR_SECC   0x21u
#define B_PGI_PAR_EVCC   0x22u
#define B_PGI_READY_SECC 0x71u
#define B_PGI_READY_EVCC 0x72u
#define B_PGI_REQ_EVCC   0x73u
#define B_PGI_INFO_EVCC  0x74u
#define B_PGI_BATT_EVCC  0x75u
#define B_PGI_STAT_EVCC  0x82u

#define B_FC_PARAM  0x20u
#define B_FC_ENERGY 0x70u
#define B_FC_STATS  0x80u

#define Q_MAX 4
#define LM_MAX 240

enum { Q_RSM = 1, Q_URM = 2, Q_LM = 3 };
enum { TX_IDLE = 0, TX_WAIT_ACK, TX_SEND, TX_WAIT_END };

typedef struct {
    uint8_t kind;
    uint8_t len;
    uint8_t data[64];
} qitem_t;

static qitem_t s_q[Q_MAX];
static uint8_t s_q_n;

static uint8_t s_lm_rx[LM_MAX];
static uint16_t s_lm_rx_len;
static uint16_t s_lm_rx_got;
static uint8_t s_lm_rx_frames;
static uint8_t s_lm_rx_next;
static bool s_lm_rx_on;

static uint8_t s_lm_tx[64];
static uint16_t s_lm_tx_len;
static uint8_t s_lm_tx_frames;
static uint8_t s_lm_tx_next;
static uint8_t s_lm_tx_end;
static uint8_t s_lm_tx_state;
static uint8_t s_lm_try;
static uint32_t s_lm_tick;

static bool s_nego;
static bool s_energy;
static bool s_stop;
static bool s_secc_ready;
static bool s_contact;
static bool s_pause;
static uint8_t s_want_energy;
static uint8_t s_want_stats;
static uint32_t s_ready_ms;
static uint32_t s_last_ready;
static uint32_t s_last_req;
static uint32_t s_last_info;
static uint32_t s_last_batt;
static uint32_t s_last_relay;
static uint32_t s_last_latch;
static uint8_t s_rsm_try;
static uint32_t s_rsm_tick;

static uint32_t b_id(uint8_t p, uint8_t pf, uint8_t ps, uint8_t sa)
{
    return ((uint32_t)(p & 7u) << 26) | ((uint32_t)pf << 16) | ((uint32_t)ps << 8) | sa;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool enqueue(uint8_t kind, const uint8_t *data, uint8_t len)
{
    if (!data || len == 0 || len > sizeof(s_q[0].data) || s_q_n >= Q_MAX) return false;
    s_q[s_q_n].kind = kind;
    s_q[s_q_n].len = len;
    memcpy(s_q[s_q_n].data, data, len);
    s_q_n++;
    return true;
}

static void q_pop(void)
{
    if (s_q_n == 0) return;
    memmove(&s_q[0], &s_q[1], (size_t)(s_q_n - 1u) * sizeof(s_q[0]));
    s_q_n--;
    s_rsm_try = 0;
    s_rsm_tick = 0;
    s_lm_try = 0;
    s_lm_tx_state = TX_IDLE;
}

static void send_ctl(uint8_t kind, uint8_t a, uint8_t b)
{
    uint8_t d[8];
    memset(d, 0xFF, sizeof(d));
    d[0] = kind;
    d[1] = a;
    d[2] = b;
    (void)gbt_can_send(b_id(3, B_PF_CTL, B_SA_SECC, B_SA_EVCC), d, 8);
}

static void send_padded(uint8_t p, uint8_t pf, const uint8_t *data, uint8_t len)
{
    uint8_t d[8];
    memset(d, 0xFF, sizeof(d));
    if (len > 8) len = 8;
    memcpy(d, data, len);
    (void)gbt_can_send(b_id(p, pf, B_SA_SECC, B_SA_EVCC), d, 8);
}

static void tx_version(uint8_t result, uint8_t major, uint8_t minor, uint8_t patch)
{
    uint8_t d[8];
    memset(d, 0xFF, sizeof(d));
    d[0] = 0x00; /* CAN 2.0 */
    d[1] = result; /* 0 continue, 1 success, 2 failed */
    d[2] = major;
    d[3] = minor;
    d[4] = patch;
    (void)gbt_can_send(b_id(3, B_PF_VER, B_SA_SECC, B_SA_EVCC), d, 8);
}

static uint16_t demand_v(void)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    return st.bcl_requested_voltage_raw ? st.bcl_requested_voltage_raw : 2400;
}

static uint16_t demand_i(void)
{
    cube_state_snapshot_t st;
    int32_t i;
    app_state_snapshot(&st);
    i = st.bcl_requested_current_0p1a;
    if (i < 0) i = -i;
    if (i > 65535) i = 65535;
    return (uint16_t)i;
}

static uint8_t fdc_has(const uint8_t *p, uint16_t len, uint16_t base, uint8_t code)
{
    uint16_t i;
    if (!p || len <= (uint16_t)(1u + base)) return 0;
    for (i = 0; i < 8 && (uint16_t)(1u + base + i) < len; ++i) {
        if (p[1u + base + i] == code) return code;
    }
    return 0;
}

static void reply_func(const uint8_t *p, uint16_t len)
{
    uint8_t out[8];
    memset(out, 0, sizeof(out));
    out[0] = B_PGI_FUNC_EVCC;
    /* Slot value is the FDC code. 1 is charge. 2 is discharge, which this cube does not offer. */
    s_want_energy = fdc_has(p, len, 32, 1);
    s_want_stats = fdc_has(p, len, 40, 1);
    out[5] = s_want_energy;
    out[6] = s_want_stats;
    if (!enqueue(Q_RSM, out, 8)) app_state_log("B func reply queue full");
    app_state_log(s_want_energy ? "B func: charge FDC1" : "B func: no charge FDC");
}

static void reply_param(void)
{
    cube_state_snapshot_t st;
    uint8_t b[20];
    uint16_t vmax;
    uint16_t soc;
    uint16_t energy;
    app_state_snapshot(&st);
    memset(b, 0, sizeof(b));
    vmax = CUBE_DEFAULT_MAX_VOLTAGE_0P1V;
    if (demand_v() > vmax) vmax = demand_v();
    energy = (uint16_t)(app_state_cap_kwh() * 10.0f);
    soc = (uint16_t)st.soc_percent * 10u;
    b[0] = B_PGI_PAR_EVCC;
    put16(&b[1], demand_i());
    put16(&b[3], vmax);
    put16(&b[5], energy);
    put16(&b[7], soc);
    put16(&b[9], 4200); /* 4.200 V cell, 0.001 V */
    b[11] = 90;        /* 50 C, offset -40 */
    if (!enqueue(Q_LM, b, 20)) app_state_log("B param queue full");
    else app_state_log("B param 20B");
}

static void on_phase(const uint8_t *p, uint8_t len)
{
    uint8_t fc = len > 1 ? p[1] : 0;
    uint8_t fdc = len > 2 ? p[2] : 0;
    uint8_t ack[2];
    bool ok = false;
    ack[0] = B_PGI_PHASE_EVCC;
    if (fc == B_FC_PARAM) ok = true;
    else if (fc == B_FC_ENERGY && fdc == s_want_energy && s_want_energy != 0) ok = true;
    else if (fc == B_FC_STATS && s_want_stats) ok = true;
    else if (fc == 0x10) ok = true; /* function nego phase confirm */
    ack[1] = ok ? 0xAA : 0x00;
    (void)enqueue(Q_RSM, ack, 2);
    if (fc == B_FC_ENERGY && ok) {
        s_energy = true;
        s_stop = false;
        s_ready_ms = 0;
        app_state_set_charger_stage(CUBE_STAGE_CHARGING);
        app_state_log("B energy phase");
    }
    if (fc == B_FC_STATS && ok) {
        cube_state_snapshot_t st;
        uint8_t s[3];
        app_state_snapshot(&st);
        s[0] = B_PGI_STAT_EVCC;
        put16(&s[1], (uint16_t)st.soc_percent * 10u);
        (void)enqueue(Q_RSM, s, 3);
    }
}

static void on_payload(const uint8_t *p, uint16_t len)
{
    uint8_t pgi;
    if (!p || len == 0) return;
    pgi = p[0];
    switch (pgi) {
        case B_PGI_FUNC_SECC:
            reply_func(p, len);
            break;
        case B_PGI_PAR_SECC:
            if (len >= 9) {
                uint16_t max_v = get16(&p[1]);
                uint16_t min_v = get16(&p[3]);
                uint16_t max_i = get16(&p[5]);
                uint16_t min_i = get16(&p[7]);
                app_state_set_cml(max_v, min_v, (int16_t)max_i, (int16_t)min_i);
            }
            reply_param();
            app_state_set_charger_stage(CUBE_STAGE_PARAMETER);
            break;
        case B_PGI_PHASE_SECC:
            on_phase(p, (uint8_t)(len > 8 ? 8 : len));
            break;
        case B_PGI_STOP_SECC:
            s_stop = true;
            s_energy = false;
            app_state_set_charger_stage(CUBE_STAGE_SUSPENDED);
            app_state_log("B stop from charger");
            break;
        case B_PGI_RELAY_SECC:
            s_contact = len >= 2 && (p[1] == 0xAA || (len >= 3 && p[2] == 0xAA));
            break;
        case B_PGI_READY_SECC:
            s_secc_ready = len >= 2 && p[1] == 0xAA;
            if (s_secc_ready) app_state_set_cro(0xAA);
            break;
        case 0x77: /* pause */
            s_pause = len >= 2 && p[1] == 0xAA;
            break;
        case 0x76: /* ability is a limit, not measured output */
            break;
        default:
            break;
    }
}

static void lm_rx_reset(void)
{
    s_lm_rx_on = false;
    s_lm_rx_len = 0;
    s_lm_rx_got = 0;
    s_lm_rx_frames = 0;
    s_lm_rx_next = 1;
}

static void on_lm(const uint8_t *d, uint8_t dlc)
{
    if (dlc < 1) return;
    if (d[0] == 0) {
        uint16_t total;
        uint8_t frames;
        if (dlc < 4) return;
        frames = d[1];
        total = get16(&d[2]);
        if (frames == 0 || total == 0 || total > LM_MAX) {
            send_ctl(2, 0xFF, 0xFF);
            lm_rx_reset();
            return;
        }
        s_lm_rx_on = true;
        s_lm_rx_len = total;
        s_lm_rx_frames = frames;
        s_lm_rx_got = 0;
        s_lm_rx_next = 1;
        memset(s_lm_rx, 0xFF, sizeof(s_lm_rx));
        send_ctl(1, 1, frames);
        return;
    }
    if (!s_lm_rx_on || d[0] != s_lm_rx_next || dlc < 2) return;
    {
        uint16_t off = (uint16_t)(d[0] - 1u) * 7u;
        uint8_t n = (uint8_t)(dlc - 1u);
        if (off >= s_lm_rx_len) return;
        if ((uint16_t)n > (uint16_t)(s_lm_rx_len - off)) n = (uint8_t)(s_lm_rx_len - off);
        memcpy(&s_lm_rx[off], &d[1], n);
        s_lm_rx_got = (uint16_t)(off + n);
        s_lm_rx_next++;
        if (s_lm_rx_got >= s_lm_rx_len || d[0] >= s_lm_rx_frames) {
            uint8_t c[8];
            memset(c, 0xFF, sizeof(c));
            c[0] = 3;
            c[1] = s_lm_rx_frames;
            c[2] = (uint8_t)(s_lm_rx_len & 0xFF);
            c[3] = (uint8_t)(s_lm_rx_len >> 8);
            (void)gbt_can_send(b_id(3, B_PF_CTL, B_SA_SECC, B_SA_EVCC), c, 8);
            on_payload(s_lm_rx, s_lm_rx_len);
            lm_rx_reset();
        }
    }
}

static void on_version(const uint8_t *d, uint8_t dlc)
{
    uint8_t can_type = dlc > 0 ? d[0] : 0xFF;
    uint8_t major = dlc > 1 ? d[1] : 0;
    uint8_t minor = dlc > 2 ? d[2] : 0;
    uint8_t patch = dlc > 3 ? d[3] : 0;
    if (can_type != 0x00) {
        tx_version(2, 0xFF, 0xFF, 0xFF);
        return;
    }
    if (major == 2 && minor == 0 && patch == 0) {
        tx_version(1, 2, 0, 0);
        if (!s_nego) {
            s_nego = true;
            app_state_log("B version 2.0.0 ok");
            ESP_LOGI(TAG, "version negotiation success");
        }
        return;
    }
    if (major > 2 || (major == 2 && minor > 0)) {
        tx_version(0, 2, 0, 0); /* continue, we only speak 2.0.0 */
        return;
    }
    tx_version(2, 0xFF, 0xFF, 0xFF);
}

static void on_ctl(const uint8_t *d, uint8_t dlc)
{
    if (dlc < 1 || s_q_n == 0) return;
    if (s_q[0].kind == Q_RSM && d[0] == 0) {
        q_pop();
        return;
    }
    if (s_q[0].kind != Q_LM) return;
    if (d[0] == 1 && s_lm_tx_state == TX_WAIT_ACK && dlc >= 3) {
        s_lm_tx_next = d[1] ? d[1] : 1;
        s_lm_tx_end = (uint8_t)(s_lm_tx_next + (d[2] ? d[2] : 1u));
        if (s_lm_tx_end > (uint8_t)(s_lm_tx_frames + 1u)) s_lm_tx_end = (uint8_t)(s_lm_tx_frames + 1u);
        s_lm_tx_state = TX_SEND;
        return;
    }
    if (d[0] == 3 && s_lm_tx_state == TX_WAIT_END) {
        s_lm_tx_state = TX_IDLE;
        q_pop();
        return;
    }
    if (d[0] == 2) {
        s_lm_tx_state = TX_IDLE;
        s_lm_try++;
    }
}

void gbt_b_reset(void)
{
    memset(s_q, 0, sizeof(s_q));
    s_q_n = 0;
    lm_rx_reset();
    s_lm_tx_state = TX_IDLE;
    s_lm_try = 0;
    s_lm_tick = 0;
    s_rsm_try = 0;
    s_rsm_tick = 0;
    s_nego = false;
    s_energy = false;
    s_stop = false;
    s_secc_ready = false;
    s_contact = false;
    s_pause = false;
    s_want_energy = 0;
    s_want_stats = 0;
    s_ready_ms = 0;
}

void gbt_b_on_rx(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    uint32_t body;
    uint8_t sa, ps, pf;
    if (!data) return;
    body = id & 0x00FFFFFFu;
    sa = (uint8_t)(body & 0xFFu);
    ps = (uint8_t)((body >> 8) & 0xFFu);
    pf = (uint8_t)((body >> 16) & 0xFFu);
    if (sa != B_SA_SECC || ps != B_SA_EVCC) return;
    if (pf == B_PF_VER) {
        on_version(data, dlc);
        return;
    }
    if (pf == B_PF_LM) on_lm(data, dlc);
    else if (pf == B_PF_RSM) {
        send_ctl(0, 1, 0xFF);
        on_payload(data, dlc);
    } else if (pf == B_PF_URM) {
        on_payload(data, dlc);
    } else if (pf == B_PF_CTL) {
        on_ctl(data, dlc);
    }
}

static void pump_tx(uint32_t now)
{
    if (s_q_n == 0) return;
    if (s_q[0].kind == Q_URM) {
        send_padded(6, B_PF_URM, s_q[0].data, s_q[0].len);
        q_pop();
        return;
    }
    if (s_q[0].kind == Q_RSM) {
        if (s_rsm_tick != 0 && (uint32_t)(now - s_rsm_tick) < 50u) return;
        send_padded(4, B_PF_RSM, s_q[0].data, s_q[0].len);
        s_rsm_tick = now;
        s_rsm_try++;
        if (s_rsm_try > 20) q_pop();
        return;
    }
    /* long message */
    if (s_lm_tx_state == TX_IDLE) {
        uint8_t c[8];
        s_lm_tx_len = s_q[0].len;
        memcpy(s_lm_tx, s_q[0].data, s_lm_tx_len);
        s_lm_tx_frames = (uint8_t)((s_lm_tx_len + 6u) / 7u);
        memset(c, 0xFF, sizeof(c));
        c[0] = 0;
        c[1] = s_lm_tx_frames;
        c[2] = (uint8_t)(s_lm_tx_len & 0xFF);
        c[3] = (uint8_t)(s_lm_tx_len >> 8);
        (void)gbt_can_send(b_id(6, B_PF_LM, B_SA_SECC, B_SA_EVCC), c, 8);
        s_lm_tx_state = TX_WAIT_ACK;
        s_lm_tick = now;
        return;
    }
    if (s_lm_tx_state == TX_WAIT_ACK || s_lm_tx_state == TX_WAIT_END) {
        if ((uint32_t)(now - s_lm_tick) > 1000u) {
            s_lm_tx_state = TX_IDLE;
            s_lm_try++;
            if (s_lm_try > 3) {
                s_lm_try = 0;
                q_pop();
                app_state_log("B long message dropped");
            }
        }
        return;
    }
    if (s_lm_tx_state == TX_SEND) {
        uint8_t frame[8];
        uint16_t off;
        uint8_t i;
        if (s_lm_tx_next >= s_lm_tx_end || s_lm_tx_next == 0) {
            s_lm_tx_state = (s_lm_tx_next > s_lm_tx_frames) ? TX_WAIT_END : TX_WAIT_ACK;
            s_lm_tick = now;
            return;
        }
        memset(frame, 0xFF, sizeof(frame));
        frame[0] = s_lm_tx_next;
        off = (uint16_t)(s_lm_tx_next - 1u) * 7u;
        for (i = 0; i < 7 && (uint16_t)(off + i) < s_lm_tx_len; ++i) frame[1u + i] = s_lm_tx[off + i];
        (void)gbt_can_send(b_id(6, B_PF_LM, B_SA_SECC, B_SA_EVCC), frame, 8);
        s_lm_tx_next++;
        if (s_lm_tx_next > s_lm_tx_frames) {
            s_lm_tx_state = TX_WAIT_END;
            s_lm_tick = now;
        }
    }
}

static void periodic(uint32_t now)
{
    uint8_t d[8];
    cube_state_snapshot_t st;
    if (!s_energy) return;
    app_state_snapshot(&st);
    if ((uint32_t)(now - s_last_ready) >= 250u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_READY_EVCC;
        put16(&d[1], demand_v());
        d[3] = 0xAA;
        send_padded(6, B_PF_URM, d, 4);
        s_last_ready = now;
    }
    if ((uint32_t)(now - s_last_req) >= 50u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_REQ_EVCC;
        put16(&d[1], demand_v());
        put16(&d[3], demand_i());
        d[5] = (st.bcl_mode == 2) ? 0x01 : 0x02; /* A mode 1 is voltage */
        send_padded(6, B_PF_URM, d, 6);
        s_last_req = now;
    }
    if ((uint32_t)(now - s_last_info) >= 250u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_INFO_EVCC;
        put16(&d[1], (uint16_t)st.soc_percent * 10u);
        put16(&d[3], 60);
        send_padded(6, B_PF_URM, d, 5);
        s_last_info = now;
    }
    if ((uint32_t)(now - s_last_batt) >= 1000u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_BATT_EVCC;
        put16(&d[1], 4200);
        put16(&d[3], 3300);
        d[5] = 75; /* 35 C, offset -40 */
        d[6] = 65; /* 25 C */
        send_padded(6, B_PF_URM, d, 7);
        s_last_batt = now;
    }
    if ((uint32_t)(now - s_last_relay) >= 250u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_RELAY_EVCC;
        d[1] = 0xAA;
        d[2] = 0xAA;
        send_padded(6, B_PF_URM, d, 3);
        s_last_relay = now;
    }
    if ((uint32_t)(now - s_last_latch) >= 250u) {
        memset(d, 0xFF, sizeof(d));
        d[0] = B_PGI_LATCH_EVCC;
        d[1] = 0xAA;
        send_padded(6, B_PF_URM, d, 2);
        s_last_latch = now;
    }
    if ((s_contact || s_secc_ready) && !s_pause) {
        if (s_ready_ms == 0) s_ready_ms = now ? now : 1u;
        if ((uint32_t)(now - s_ready_ms) >= 3000u) {
            app_state_set_ccs(demand_v(), (int16_t)demand_i(), 0, 0x01);
        }
    } else {
        if (s_ready_ms != 0 && s_pause) app_state_set_ccs(0, 0, 0, 0x00);
        s_ready_ms = 0;
    }
    (void)now;
}

void gbt_b_tick(uint32_t now_ms)
{
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    if (st.protocol != 3) return;
    pump_tx(now_ms);
    periodic(now_ms);
}

void gbt_b_request_stop(void)
{
    uint8_t d[7];
    memset(d, 0, sizeof(d));
    d[0] = B_PGI_STOP_EVCC;
    d[1] = 0x01;
    (void)enqueue(Q_RSM, d, 7);
    s_energy = false;
    s_stop = true;
}

bool gbt_b_energy(void) { return s_energy && !s_stop; }
bool gbt_b_stop(void) { return s_stop; }
