#pragma once
#include <stddef.h>
#include <stdint.h>

/* Streaming LZSS used to park the factory image in the spare flash above ota_1.
   Window 4096, min match 3, max match 18. Callbacks return 0 on success. */
typedef int (*lzss_read_fn)(void *ctx, uint32_t off, uint8_t *dst, uint32_t len);
typedef int (*lzss_write_fn)(void *ctx, uint32_t off, const uint8_t *src, uint32_t len);

uint32_t lzss_crc32(const uint8_t *p, uint32_t n, uint32_t crc);
int lzss_compress(lzss_read_fn rd, void *rctx, uint32_t raw_len,
                  lzss_write_fn wr, void *wctx, uint32_t *comp_len, uint32_t *crc);
int lzss_decompress(lzss_read_fn rd, void *rctx, uint32_t comp_len,
                    lzss_write_fn wr, void *wctx, uint32_t raw_len);
