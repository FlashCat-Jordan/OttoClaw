#pragma once
/*
 * QR Code generator — minimal wrapper over qrcodegen (Nayuki, MIT License)
 * Supports URL encoding up to ~100 chars (QR version 1-10, ECC Medium).
 *
 * Usage:
 *   uint8_t modules[QRCODE_MAX_MODULES * QRCODE_MAX_MODULES];
 *   int size;
 *   if (qrcode_encode_url("http://192.168.4.1", modules, &size) == 0) {
 *       // modules[row * size + col] == 1 means dark module
 *   }
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum QR version supported (version 10 = 57 modules per side) */
#define QRCODE_MAX_VERSION  10
#define QRCODE_MAX_MODULES  (17 + 4 * QRCODE_MAX_VERSION)  /* 57 */

/*
 * Encode a text string into a flat QR module array.
 *
 * @param text       Null-terminated input string
 * @param modules    Output buffer; must be at least QRCODE_MAX_MODULES^2 bytes.
 *                   Each byte is 1 (dark) or 0 (light).
 * @param out_size   Receives the actual side length (modules per side).
 *
 * @return 0 on success, -1 if input is too long or encoding failed.
 */
int qrcode_encode_text(const char *text, uint8_t *modules, int *out_size);

/*
 * Convenience wrapper for URLs (same as qrcode_encode_text).
 */
static inline int qrcode_encode_url(const char *url, uint8_t *modules, int *out_size)
{
    return qrcode_encode_text(url, modules, out_size);
}

#ifdef __cplusplus
}
#endif
