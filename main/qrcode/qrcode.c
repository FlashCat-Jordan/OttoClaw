/*
 * QR Code generator — compact implementation for ESP32
 * Based on Nayuki's qrcodegen (MIT License, https://www.nayuki.io/page/qr-code-generator-library)
 * Adapted: single-file C, minimal malloc, suitable for embedded use.
 *
 * Supported: Byte mode, ECC Level M, versions 1-10, mask patterns 0-7.
 */

#include "qrcode.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Internal storage — static, no heap for the grid itself            */
/* ------------------------------------------------------------------ */

/* QR grid stored as bit-packed rows; max 57 cols → 8 bytes/row */
#define GRID_BYTES_PER_ROW  ((QRCODE_MAX_MODULES + 7) / 8)

static uint8_t s_modules[QRCODE_MAX_MODULES][GRID_BYTES_PER_ROW];   /* dark=1 */
static uint8_t s_isFunc [QRCODE_MAX_MODULES][GRID_BYTES_PER_ROW];   /* function module mask */
static int     s_size;   /* current QR side length */

/* ------------------------------------------------------------------ */
/*  Bit helpers                                                        */
/* ------------------------------------------------------------------ */
static inline void grid_set(uint8_t grid[][GRID_BYTES_PER_ROW], int r, int c, bool v)
{
    if (v) grid[r][c >> 3] |=  (uint8_t)(1u << (c & 7));
    else   grid[r][c >> 3] &= (uint8_t)~(1u << (c & 7));
}
static inline bool grid_get(const uint8_t grid[][GRID_BYTES_PER_ROW], int r, int c)
{
    return (grid[r][c >> 3] >> (c & 7)) & 1;
}

/* ------------------------------------------------------------------ */
/*  Reed-Solomon GF(256) arithmetic                                   */
/* ------------------------------------------------------------------ */
static uint8_t rs_mul(uint8_t a, uint8_t b)
{
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) r ^= a;
        bool hi = (a >> 7) & 1;
        a = (uint8_t)(a << 1);
        if (hi) a ^= 0x1D;   /* x^8+x^4+x^3+x^2+1 mod poly */
        b >>= 1;
    }
    return r;
}

/* Compute 'ec_len' error correction bytes for 'data_len' input bytes */
static void rs_encode(const uint8_t *data, int data_len, uint8_t *ec, int ec_len)
{
    /* Generator polynomial coefficients for degree ec_len */
    /* Precompute generator */
    static uint8_t gen[30];   /* max EC codewords per block we'll need */
    if (ec_len > 30) return;
    memset(gen, 0, (size_t)ec_len);
    gen[0] = 1;
    for (int i = 0; i < ec_len; i++) {
        uint8_t alpha = 1;
        for (int j = 0; j < i; j++) alpha = rs_mul(alpha, 2);
        for (int j = i; j >= 0; j--) {
            gen[j] = rs_mul(gen[j], alpha);
            if (j > 0) gen[j] ^= gen[j-1];
        }
    }

    /* Long division */
    memset(ec, 0, (size_t)ec_len);
    for (int i = 0; i < data_len; i++) {
        uint8_t factor = data[i] ^ ec[0];
        memmove(ec, ec+1, (size_t)(ec_len-1));
        ec[ec_len-1] = 0;
        for (int j = 0; j < ec_len; j++)
            ec[j] ^= rs_mul(gen[ec_len-1-j], factor);
    }
}

/* ------------------------------------------------------------------ */
/*  QR version / capacity tables  (ECC level M = index 1)            */
/* ------------------------------------------------------------------ */
/* Number of data codewords for versions 1-10, ECC-M */
static const uint16_t DATA_CODEWORDS_M[11] = {
    0,  /* v0 unused */
    16, 28, 44, 64, 86, 108, 124, 154, 182, 216
};
/* Number of EC codewords per block, ECC-M */
static const uint8_t EC_PER_BLOCK_M[11] = {
    0,
    10, 16, 26, 18, 24, 16, 18, 22, 22, 26
};
/* Number of EC blocks, ECC-M */
static const uint8_t EC_BLOCKS_M[11] = {
    0,
    1, 1, 1, 2, 2, 4, 4, 4, 5, 5
};
/* Format info bits (mask 0..7, ECC M) — precomputed */
static const uint16_t FORMAT_BITS_M[8] = {
    0x5412, 0x5125, 0x5E7C, 0x5B4B,
    0x45F9, 0x40CE, 0x4F97, 0x4AA0
};
/* Alignment pattern center positions per version */
static const uint8_t ALIGN_POS[11][7] = {
    {0},                       /* v0  */
    {0},                       /* v1  none */
    {6,18,0},                  /* v2  */
    {6,22,0},                  /* v3  */
    {6,26,0},                  /* v4  */
    {6,30,0},                  /* v5  */
    {6,34,0},                  /* v6  */
    {6,22,38,0},               /* v7  */
    {6,24,42,0},               /* v8  */
    {6,26,46,0},               /* v9  */
    {6,28,50,0},               /* v10 */
};

/* ------------------------------------------------------------------ */
/*  Drawing function patterns                                          */
/* ------------------------------------------------------------------ */
static void draw_module(int r, int c, bool dark)
{
    grid_set(s_modules, r, c, dark);
    grid_set(s_isFunc,  r, c, true);
}

static void draw_finder(int row, int col)
{
    for (int dr = -1; dr <= 7; dr++) {
        for (int dc = -1; dc <= 7; dc++) {
            int r = row + dr, c = col + dc;
            if (r < 0 || r >= s_size || c < 0 || c >= s_size) continue;
            bool dark = (dr >= 0 && dr <= 6 && dc >= 0 && dc <= 6) &&
                        ((dr == 0 || dr == 6 || dc == 0 || dc == 6) ||
                         (dr >= 2 && dr <= 4 && dc >= 2 && dc <= 4));
            draw_module(r, c, dark);
        }
    }
}

static void draw_timing(void)
{
    for (int i = 0; i < s_size; i++) {
        draw_module(6, i, i % 2 == 0);
        draw_module(i, 6, i % 2 == 0);
    }
}

static void draw_alignment(int version)
{
    if (version < 2) return;
    const uint8_t *pos = ALIGN_POS[version];
    for (int i = 0; pos[i] != 0; i++) {
        for (int j = 0; pos[j] != 0; j++) {
            int r = pos[i], c = pos[j];
            /* Skip if overlaps with finder */
            if (grid_get(s_isFunc, r, c)) continue;
            for (int dr = -2; dr <= 2; dr++)
                for (int dc = -2; dc <= 2; dc++)
                    draw_module(r+dr, c+dc,
                        dr == -2 || dr == 2 || dc == -2 || dc == 2 || (dr==0 && dc==0));
        }
    }
}

static void draw_format(int mask_id)
{
    uint16_t bits = FORMAT_BITS_M[mask_id];
    /* Around top-left finder */
    for (int i = 0; i <= 5; i++) draw_module(8, i, (bits >> i) & 1);
    draw_module(8, 7, (bits >> 6) & 1);
    draw_module(8, 8, (bits >> 7) & 1);
    draw_module(7, 8, (bits >> 8) & 1);
    for (int i = 9; i <= 14; i++) draw_module(14-i, 8, (bits >> i) & 1);
    /* Around top-right and bottom-left finders */
    for (int i = 0; i <= 7; i++) draw_module(8, s_size-1-i, (bits >> i) & 1);
    for (int i = 8; i <= 14; i++) draw_module(s_size-15+i, 8, (bits >> i) & 1);
    draw_module(s_size-8, 8, 1); /* Dark module always */
}

/* ------------------------------------------------------------------ */
/*  Codeword placement                                                 */
/* ------------------------------------------------------------------ */
static void place_codewords(const uint8_t *cw, int cw_len)
{
    int bit_idx = 0;
    int total_bits = cw_len * 8;

    for (int right = s_size-1; right >= 1; right -= 2) {
        if (right == 6) right = 5; /* skip timing col */
        for (int vert = s_size-1; vert >= 0; vert--) {
            for (int dc = 0; dc < 2; dc++) {
                int c = right - dc;
                /* Upward vs downward */
                int r = ((right & 2) == 0) ? (s_size-1-vert) : vert;
                if (grid_get(s_isFunc, r, c)) continue;
                bool dark = false;
                if (bit_idx < total_bits)
                    dark = (cw[bit_idx >> 3] >> (7 - (bit_idx & 7))) & 1;
                bit_idx++;
                grid_set(s_modules, r, c, dark);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Masking                                                            */
/* ------------------------------------------------------------------ */
static bool mask_bit(int mask, int r, int c)
{
    switch (mask) {
        case 0: return (r+c) % 2 == 0;
        case 1: return r % 2 == 0;
        case 2: return c % 3 == 0;
        case 3: return (r+c) % 3 == 0;
        case 4: return (r/2 + c/3) % 2 == 0;
        case 5: return (r*c)%2 + (r*c)%3 == 0;
        case 6: return ((r*c)%2 + (r*c)%3) % 2 == 0;
        case 7: return ((r+c)%2 + (r*c)%3) % 2 == 0;
        default: return false;
    }
}

static void apply_mask(int mask)
{
    for (int r = 0; r < s_size; r++)
        for (int c = 0; c < s_size; c++)
            if (!grid_get(s_isFunc, r, c) && mask_bit(mask, r, c))
                grid_set(s_modules, r, c, !grid_get(s_modules, r, c));
}

static int penalty_score(void)
{
    int penalty = 0;
    /* Rule 1: 5+ in a row */
    for (int r = 0; r < s_size; r++) {
        int run = 1;
        bool last = grid_get(s_modules, r, 0);
        for (int c = 1; c < s_size; c++) {
            bool cur = grid_get(s_modules, r, c);
            if (cur == last) { run++; if (run == 5) penalty += 3; else if (run > 5) penalty++; }
            else { run = 1; last = cur; }
        }
    }
    for (int c = 0; c < s_size; c++) {
        int run = 1;
        bool last = grid_get(s_modules, 0, c);
        for (int r = 1; r < s_size; r++) {
            bool cur = grid_get(s_modules, r, c);
            if (cur == last) { run++; if (run == 5) penalty += 3; else if (run > 5) penalty++; }
            else { run = 1; last = cur; }
        }
    }
    /* Rule 2: 2x2 blocks */
    for (int r = 0; r < s_size-1; r++)
        for (int c = 0; c < s_size-1; c++) {
            bool v = grid_get(s_modules, r, c);
            if (v == grid_get(s_modules, r, c+1) &&
                v == grid_get(s_modules, r+1, c) &&
                v == grid_get(s_modules, r+1, c+1))
                penalty += 3;
        }
    /* Rule 3: finder-like patterns */
    static const bool pat1[11] = {true,false,true,true,true,false,true,false,false,false,false};
    static const bool pat2[11] = {false,false,false,false,true,false,true,true,true,false,true};
    for (int r = 0; r < s_size; r++)
        for (int c = 0; c <= s_size-11; c++) {
            bool ok1=true, ok2=true;
            for (int k=0;k<11;k++) {
                if (grid_get(s_modules,r,c+k) != pat1[k]) ok1=false;
                if (grid_get(s_modules,r,c+k) != pat2[k]) ok2=false;
            }
            if (ok1||ok2) penalty += 40;
        }
    for (int c = 0; c < s_size; c++)
        for (int r = 0; r <= s_size-11; r++) {
            bool ok1=true, ok2=true;
            for (int k=0;k<11;k++) {
                if (grid_get(s_modules,r+k,c) != pat1[k]) ok1=false;
                if (grid_get(s_modules,r+k,c) != pat2[k]) ok2=false;
            }
            if (ok1||ok2) penalty += 40;
        }
    /* Rule 4: dark proportion */
    int dark = 0;
    for (int r = 0; r < s_size; r++)
        for (int c = 0; c < s_size; c++)
            if (grid_get(s_modules, r, c)) dark++;
    int total = s_size * s_size;
    int pct = dark * 100 / total;
    int k = (pct > 50) ? (pct - 50)/5 : (50 - pct)/5;
    penalty += k * 10;
    return penalty;
}

/* ------------------------------------------------------------------ */
/*  Main encode function                                               */
/* ------------------------------------------------------------------ */

/* Maximum codeword buffer: version 10 ECC-M = 216 data + ~134 EC */
#define MAX_CW_BUF  360

int qrcode_encode_text(const char *text, uint8_t *out_modules, int *out_size)
{
    if (!text || !out_modules || !out_size) return -1;
    int text_len = (int)strlen(text);

    /* Find minimum version that fits */
    int version = -1;
    for (int v = 1; v <= QRCODE_MAX_VERSION; v++) {
        /* Byte mode header: 4 + 8 bits = 12, plus data bits, plus terminator 4 */
        int capacity_bits = DATA_CODEWORDS_M[v] * 8;
        int needed = 4 + 8 + text_len * 8 + 4;
        if (needed <= capacity_bits) { version = v; break; }
    }
    if (version < 0) return -1;  /* too long */

    s_size = 17 + 4 * version;

    /* Clear grids */
    memset(s_modules, 0, sizeof(s_modules));
    memset(s_isFunc,  0, sizeof(s_isFunc));

    /* Draw function patterns */
    draw_finder(0, 0);
    draw_finder(0, s_size-7);
    draw_finder(s_size-7, 0);
    draw_timing();
    draw_alignment(version);

    /* Encode data bits into byte array */
    static uint8_t data_cw[MAX_CW_BUF];
    memset(data_cw, 0, sizeof(data_cw));

    int bit = 0;
    /* Mode indicator: byte = 0100 */
    data_cw[bit>>3] |= (uint8_t)(0x4 << (4 - (bit & 7))); /* careful shift */
    /* Actually do it bit by bit for clarity */
    /* Reset and redo bit by bit */
    memset(data_cw, 0, sizeof(data_cw));
    bit = 0;

    /* Append bit helper (local lambda via macro) */
#define APPEND_BIT(b) do { \
    if (b) data_cw[bit>>3] |= (uint8_t)(1u << (7-(bit&7))); \
    bit++; \
} while(0)
#define APPEND_BITS(val, n) do { \
    for (int _i = (n)-1; _i >= 0; _i--) APPEND_BIT(((val)>>_i)&1); \
} while(0)

    /* Mode: byte = 0b0100 */
    APPEND_BITS(0x4, 4);
    /* Character count: 8 bits */
    APPEND_BITS(text_len, 8);
    /* Data bytes */
    for (int i = 0; i < text_len; i++) APPEND_BITS((uint8_t)text[i], 8);
    /* Terminator (up to 4 bits) */
    int total_data_bits = DATA_CODEWORDS_M[version] * 8;
    for (int i = 0; i < 4 && bit < total_data_bits; i++) APPEND_BIT(0);
    /* Pad to byte boundary */
    while (bit % 8 != 0) APPEND_BIT(0);
    /* Pad bytes */
    int pad_byte = 0;
    while (bit < total_data_bits) {
        APPEND_BITS(pad_byte ? 0x11 : 0xEC, 8);
        pad_byte ^= 1;
    }
#undef APPEND_BIT
#undef APPEND_BITS

    int data_cw_count = DATA_CODEWORDS_M[version];

    /* Compute EC codewords — handle multiple blocks */
    int num_blocks    = EC_BLOCKS_M[version];
    int ec_per_block  = EC_PER_BLOCK_M[version];
    int total_cw      = data_cw_count + num_blocks * ec_per_block;

    /* Split data into blocks and compute EC */
    /* Short block len = floor(data_cw_count / num_blocks) */
    /* Long block count = data_cw_count % num_blocks */
    int short_block = data_cw_count / num_blocks;
    int long_blocks = data_cw_count % num_blocks;  /* these have 1 extra data CW */

    static uint8_t ec_buf[5][30];   /* max 5 blocks, max 30 EC per block */
    static uint8_t block_data[5][50]; /* max block data codewords */
    int block_sizes[5];

    int src = 0;
    for (int b = 0; b < num_blocks; b++) {
        block_sizes[b] = short_block + (b >= (num_blocks - long_blocks) ? 1 : 0);
        memcpy(block_data[b], data_cw + src, (size_t)block_sizes[b]);
        src += block_sizes[b];
        rs_encode(block_data[b], block_sizes[b], ec_buf[b], ec_per_block);
    }

    /* Interleave into final codeword sequence */
    static uint8_t final_cw[MAX_CW_BUF];
    memset(final_cw, 0, sizeof(final_cw));
    int fw = 0;
    /* Data: column by column */
    int max_block_size = short_block + (long_blocks > 0 ? 1 : 0);
    for (int col = 0; col < max_block_size; col++)
        for (int b = 0; b < num_blocks; b++)
            if (col < block_sizes[b])
                final_cw[fw++] = block_data[b][col];
    /* EC: column by column */
    for (int col = 0; col < ec_per_block; col++)
        for (int b = 0; b < num_blocks; b++)
            final_cw[fw++] = ec_buf[b][col];

    /* Place codewords (before masking) */
    place_codewords(final_cw, total_cw);

    /* Draw format info (temporary mask 0 to evaluate, will redraw) */
    draw_format(0);

    /* Try all 8 masks, pick best penalty score */
    int best_mask = 0;
    int best_score = 0x7FFFFFFF;
    static uint8_t saved_modules[QRCODE_MAX_MODULES][GRID_BYTES_PER_ROW];
    memcpy(saved_modules, s_modules, sizeof(s_modules));

    for (int mask = 0; mask < 8; mask++) {
        /* Restore to pre-mask state */
        memcpy(s_modules, saved_modules, sizeof(s_modules));
        /* Redraw format (clears old format bits first) */
        draw_format(mask);
        apply_mask(mask);
        int score = penalty_score();
        if (score < best_score) { best_score = score; best_mask = mask; }
    }

    /* Apply best mask for real */
    memcpy(s_modules, saved_modules, sizeof(s_modules));
    draw_format(best_mask);
    apply_mask(best_mask);

    /* Export to flat byte array (1 byte per module: 0=light, 1=dark) */
    for (int r = 0; r < s_size; r++)
        for (int c = 0; c < s_size; c++)
            out_modules[r * s_size + c] = grid_get(s_modules, r, c) ? 1 : 0;

    *out_size = s_size;
    return 0;
}
