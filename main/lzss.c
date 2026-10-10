#include "lzss.h"
#include <stdlib.h>
#include <string.h>

#define LZ_WIN 4096u
#define LZ_MIN 3u
#define LZ_MAX 18u

uint32_t lzss_crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int b;
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

uint32_t lzss_crc32(const uint8_t *p, uint32_t n, uint32_t crc)
{
    return ~lzss_crc32_update(~crc, p, n);
}

#ifndef LZSS_HOST_TEST
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static void lz_yield(uint32_t n)
{
    if ((n & 0x3FFu) == 0u) vTaskDelay(1);
}
#else
static void lz_yield(uint32_t n) { (void)n; }
#endif

static uint8_t s_cache[2048];
static uint32_t s_cache_off;
static uint32_t s_cache_len;
static uint32_t s_raw_len;
static lzss_read_fn s_rd;
static void *s_rctx;

static int at_byte(uint32_t pos, uint8_t *out)
{
    if (pos >= s_raw_len) return -1;
    if (pos < s_cache_off || pos >= s_cache_off + s_cache_len) {
        uint32_t base = pos & ~1023u;
        uint32_t n = 2048u;
        if (base >= s_raw_len) return -1;
        if (base + n > s_raw_len) n = s_raw_len - base;
        if (s_rd(s_rctx, base, s_cache, n) != 0) return -1;
        s_cache_off = base;
        s_cache_len = n;
    }
    *out = s_cache[pos - s_cache_off];
    return 0;
}

static int hash3(uint32_t p, uint32_t *h)
{
    uint8_t a, b, c;
    if (at_byte(p, &a) || at_byte(p + 1u, &b) || at_byte(p + 2u, &c)) return -1;
    *h = (uint32_t)(a ^ ((uint32_t)b << 4) ^ ((uint32_t)c << 8)) & 0xFFFu;
    return 0;
}

int lzss_compress(lzss_read_fn rd, void *rctx, uint32_t raw_len,
                  lzss_write_fn wr, void *wctx, uint32_t *comp_len, uint32_t *crc_out)
{
    static uint32_t *head = NULL;
    static uint32_t *prev = NULL;
    uint32_t i, out_off, crc;
    if (!rd || !wr || raw_len < 3u) return -1;
    if (!head) head = malloc(4096u * sizeof(uint32_t));
    if (!prev) prev = malloc(LZ_WIN * sizeof(uint32_t));
    if (!head || !prev) return -1;
    s_rd = rd;
    s_rctx = rctx;
    s_raw_len = raw_len;
    s_cache_off = 0;
    s_cache_len = 0;
    memset(head, 0xFF, 4096u * sizeof(uint32_t));
    memset(prev, 0xFF, LZ_WIN * sizeof(uint32_t));
    i = 0;
    out_off = 0;
    crc = 0xFFFFFFFFu;
    while (i < raw_len) {
        uint8_t group[1 + 16];
        uint32_t glen = 1;
        uint8_t flag = 0;
        int bit;
        lz_yield(i);
        for (bit = 0; bit < 8; bit++) {
            uint32_t best = 0, bestd = 0, h = 0;
            if (i >= raw_len) break;
            if (i + LZ_MIN <= raw_len && i + 2u < raw_len && hash3(i, &h) == 0) {
                uint32_t p = head[h];
                int steps = 0;
                while (p != 0xFFFFFFFFu && steps < 32 && i > p && (i - p) <= LZ_WIN) {
                    uint32_t m = 0;
                    uint32_t lim = (i + LZ_MAX <= raw_len) ? LZ_MAX : (raw_len - i);
                    while (m < lim) {
                        uint32_t sp = p + m;
                        uint8_t sb, nb;
                        if (sp < i) {
                            if (at_byte(sp, &sb)) return -1;
                        } else {
                            if (at_byte(i + (sp - i), &sb)) return -1;
                        }
                        if (at_byte(i + m, &nb) || sb != nb) break;
                        m++;
                    }
                    if (m > best && m >= LZ_MIN) {
                        best = m;
                        bestd = i - p;
                        if (m == lim) break;
                    }
                    {
                        uint32_t np = prev[p % LZ_WIN];
                        if (np == p) break;
                        p = np;
                    }
                    steps++;
                }
                prev[i % LZ_WIN] = head[h];
                head[h] = i;
            }
            if (best >= LZ_MIN) {
                uint32_t offb = bestd - 1u;
                uint32_t ln = best - LZ_MIN;
                uint32_t k;
                group[glen++] = (uint8_t)((offb >> 4) & 0xFFu);
                group[glen++] = (uint8_t)(((offb & 0xFu) << 4) | (ln & 0xFu));
                flag = (uint8_t)(flag | (1u << bit));
                for (k = 1; k < best; k++) {
                    uint32_t hk;
                    if (i + k + 2u < raw_len && hash3(i + k, &hk) == 0) {
                        prev[(i + k) % LZ_WIN] = head[hk];
                        head[hk] = i + k;
                    }
                }
                i += best;
            } else {
                uint8_t lit;
                if (i + 2u < raw_len && !(i + LZ_MIN <= raw_len)) {
                    uint32_t hk;
                    if (hash3(i, &hk) == 0) {
                        prev[i % LZ_WIN] = head[hk];
                        head[hk] = i;
                    }
                }
                if (at_byte(i, &lit)) return -1;
                group[glen++] = lit;
                i++;
            }
        }
        group[0] = flag;
        if (wr(wctx, out_off, group, glen) != 0) return -1;
        crc = lzss_crc32_update(crc, group, glen);
        out_off += glen;
    }
    if (comp_len) *comp_len = out_off;
    if (crc_out) *crc_out = ~crc;
    return 0;
}

int lzss_decompress(lzss_read_fn rd, void *rctx, uint32_t comp_len,
                    lzss_write_fn wr, void *wctx, uint32_t raw_len)
{
    uint8_t *hist = malloc(LZ_WIN);
    uint8_t chunk[256];
    uint32_t produced = 0, ip = 0, cn = 0;
    if (!rd || !wr || raw_len == 0u || !hist) {
        free(hist);
        return -1;
    }
    while (produced < raw_len) {
        uint8_t flag;
        int bit;
        lz_yield(produced);
        if (ip >= comp_len || rd(rctx, ip, &flag, 1) != 0) { free(hist); return -1; }
        ip++;
        for (bit = 0; bit < 8 && produced < raw_len; bit++) {
            uint8_t b;
            if (flag & (1u << bit)) {
                uint8_t ab[2];
                uint32_t dist, ln, k;
                if (ip + 2u > comp_len || rd(rctx, ip, ab, 2) != 0) { free(hist); return -1; }
                ip += 2u;
                dist = ((((uint32_t)ab[0] << 4) | (ab[1] >> 4)) + 1u);
                ln = (uint32_t)(ab[1] & 0xFu) + LZ_MIN;
                if (dist == 0u || dist > produced || dist > LZ_WIN) { free(hist); return -1; }
                for (k = 0; k < ln && produced < raw_len; k++) {
                    b = hist[(produced - dist) % LZ_WIN];
                    hist[produced % LZ_WIN] = b;
                    chunk[cn++] = b;
                    produced++;
                    if (cn == sizeof(chunk)) {
                        if (wr(wctx, produced - cn, chunk, cn) != 0) { free(hist); return -1; }
                        cn = 0;
                    }
                }
            } else {
                if (ip >= comp_len || rd(rctx, ip, &b, 1) != 0) { free(hist); return -1; }
                ip++;
                hist[produced % LZ_WIN] = b;
                chunk[cn++] = b;
                produced++;
                if (cn == sizeof(chunk)) {
                    if (wr(wctx, produced - cn, chunk, cn) != 0) { free(hist); return -1; }
                    cn = 0;
                }
            }
        }
    }
    if (cn && wr(wctx, produced - cn, chunk, cn) != 0) { free(hist); return -1; }
    free(hist);
    return 0;
}

#ifdef LZSS_HOST_TEST
#include <stdio.h>
#include <stdlib.h>
static uint8_t *g_in, *g_out;
static uint32_t g_out_len, g_out_cap;
static int rd_mem(void *ctx, uint32_t off, uint8_t *dst, uint32_t len)
{
    uint8_t *src = ctx ? (uint8_t *)ctx : g_in;
    memcpy(dst, src + off, len);
    return 0;
}
static int wr_mem(void *ctx, uint32_t off, const uint8_t *src, uint32_t len)
{
    (void)ctx;
    if (off + len > g_out_cap) return -1;
    memcpy(g_out + off, src, len);
    if (off + len > g_out_len) g_out_len = off + len;
    return 0;
}
int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw, *back;
    uint32_t n, clen = 0, crc = 0, zcrc;
    long sz;
    if (argc < 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    raw = malloc((size_t)sz);
    fread(raw, 1, (size_t)sz, f);
    fclose(f);
    n = (uint32_t)sz;
    g_in = raw;
    g_out_cap = n;
    g_out = malloc(n);
    g_out_len = 0;
    if (lzss_compress(rd_mem, raw, n, wr_mem, NULL, &clen, &crc) != 0) {
        fprintf(stderr, "compress failed\n");
        return 1;
    }
    zcrc = lzss_crc32(g_out, clen, 0);
    fprintf(stderr, "comp %u crc %08x %08x\n", clen, crc, zcrc);
    if (zcrc != crc) return 1;
    back = malloc(n);
    g_in = g_out;
    {
        uint32_t woff = 0;
        /* decompress via wr that checks later */
    }
    {
        static uint8_t *dec;
        dec = back;
        /* local writer */
    }
    /* use a second pair */
    {
        uint8_t *comp = malloc(clen);
        memcpy(comp, g_out, clen);
        g_out_len = 0;
        /* rewrite wr to back */
        /* call decompress with custom by swapping */
        struct wrap { uint8_t *p; } dummy;
        (void)dummy;
        if (lzss_decompress(rd_mem, comp, clen, wr_mem, NULL, n) != 0) {
            fprintf(stderr, "decompress failed\n");
            return 1;
        }
        if (g_out_len != n || memcmp(g_out, raw, n) != 0) {
            fprintf(stderr, "mismatch out %u\n", g_out_len);
            return 1;
        }
        if (argc >= 3) {
            FILE *o = fopen(argv[2], "wb");
            uint8_t hdr[32];
            uint32_t magic = 0x314C5A46u;
            if (!o) return 1;
            memset(hdr, 0xFF, sizeof(hdr));
            memcpy(hdr + 0, &magic, 4);
            memcpy(hdr + 4, &n, 4);
            memcpy(hdr + 8, &clen, 4);
            memcpy(hdr + 12, &crc, 4);
            if (fwrite(hdr, 1, 32, o) != 32 || fwrite(comp, 1, clen, o) != clen) return 1;
            fclose(o);
            fprintf(stderr, "wrote %s\n", argv[2]);
        }
    }
    fprintf(stderr, "ROUNDTRIP OK\n");
    return 0;
}
#endif
