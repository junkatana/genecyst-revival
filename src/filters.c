/* filters.c -- 2x software filters: Scale2x, 2xSaI family (Kreed), scanlines,
 * TV and CRT looks. Operates on XRGB8888 with pitch == width. */
#include "filters.h"
#include <string.h>
#include <strings.h>

static const struct { const char *name, *key; int factor; } tab[FILTER_COUNT] = {
    { "None",        "none",       1 },
    { "Scale2x",     "scale2x",    2 },
    { "2xSaI",       "2xsai",      2 },
    { "Super 2xSaI", "super2xsai", 2 },
    { "Super Eagle", "supereagle", 2 },
    { "Scanlines",   "scanlines",  2 },
    { "TV",          "tv",         2 },
    { "CRT",         "crt",        2 },
};

const char *filter_name(int id) { return (id >= 0 && id < FILTER_COUNT) ? tab[id].name : "?"; }
const char *filter_key(int id)  { return (id >= 0 && id < FILTER_COUNT) ? tab[id].key : "?"; }
int filter_factor(int id)       { return (id >= 0 && id < FILTER_COUNT) ? tab[id].factor : 1; }
int filter_by_key(const char *key)
{
    for (int i = 0; i < FILTER_COUNT; i++) if (!strcasecmp(key, tab[i].key)) return i;
    return -1;
}

/* ------------------------------------------------------------ helpers ---- */
#define RGBMASK 0x00FFFFFFu
static inline uint32_t interp(uint32_t a, uint32_t b)
{
    return (((a & 0xFEFEFEu) >> 1) + ((b & 0xFEFEFEu) >> 1) + (a & b & 0x010101u)) & RGBMASK;
}
static inline uint32_t qinterp(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t x = ((a & 0xFCFCFCu) >> 2) + ((b & 0xFCFCFCu) >> 2) + ((c & 0xFCFCFCu) >> 2) + ((d & 0xFCFCFCu) >> 2);
    uint32_t y = (((a & 0x030303u) + (b & 0x030303u) + (c & 0x030303u) + (d & 0x030303u)) >> 2) & 0x030303u;
    return (x + y) & RGBMASK;
}
/* clamped fetch */
static inline uint32_t px(const uint32_t *s, int w, int h, int x, int y)
{
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return s[y * w + x] & RGBMASK;
}

static int get_result(uint32_t A, uint32_t B, uint32_t C, uint32_t D)
{
    int x = 0, y = 0, r = 0;
    if (A == C) x++; else if (B == C) y++;
    if (A == D) x++; else if (B == D) y++;
    if (x <= 1) r++;
    if (y <= 1) r--;
    return r;
}
static int get_result1(uint32_t A, uint32_t B, uint32_t C, uint32_t D)
{
    int x = 0, y = 0, r = 0;
    if (A == C) x++; else if (B == C) y++;
    if (A == D) x++; else if (B == D) y++;
    if (x <= 1) r++;
    if (y <= 1) r--;
    return r;
}
static int get_result2(uint32_t A, uint32_t B, uint32_t C, uint32_t D)
{
    int x = 0, y = 0, r = 0;
    if (A == C) x++; else if (B == C) y++;
    if (A == D) x++; else if (B == D) y++;
    if (x <= 1) r--;
    if (y <= 1) r++;
    return r;
}

/* ------------------------------------------------------------ scale2x ---- */
static void f_scale2x(const uint32_t *s, int w, int h, uint32_t *d)
{
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t B = px(s, w, h, x, y - 1), D = px(s, w, h, x - 1, y), E = px(s, w, h, x, y),
                     F = px(s, w, h, x + 1, y), H = px(s, w, h, x, y + 1);
            uint32_t e0 = E, e1 = E, e2 = E, e3 = E;
            if (B != H && D != F) {
                e0 = D == B ? D : E; e1 = B == F ? F : E;
                e2 = D == H ? D : E; e3 = H == F ? F : E;
            }
            o0[2 * x] = e0; o0[2 * x + 1] = e1; o1[2 * x] = e2; o1[2 * x + 1] = e3;
        }
    }
}

/* -------------------------------------------------------------- 2xSaI ---- */
static void f_2xsai(const uint32_t *s, int w, int h, uint32_t *d)
{
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t I = px(s,w,h,x-1,y-1), E = px(s,w,h,x,y-1), F = px(s,w,h,x+1,y-1), J = px(s,w,h,x+2,y-1);
            uint32_t G = px(s,w,h,x-1,y),   A = px(s,w,h,x,y),   B = px(s,w,h,x+1,y),   K = px(s,w,h,x+2,y);
            uint32_t H = px(s,w,h,x-1,y+1), C = px(s,w,h,x,y+1), D = px(s,w,h,x+1,y+1), L = px(s,w,h,x+2,y+1);
            uint32_t M = px(s,w,h,x-1,y+2), N = px(s,w,h,x,y+2), O = px(s,w,h,x+1,y+2), P = px(s,w,h,x+2,y+2);
            uint32_t p0, p1, p2;
            if (A == D && B != C) {
                if ((A == E && B == L) || (A == C && A == F && B != E && B == J)) p0 = A; else p0 = interp(A, B);
                if ((A == G && C == O) || (A == B && A == H && G != C && C == M)) p1 = A; else p1 = interp(A, C);
                p2 = A;
            } else if (B == C && A != D) {
                if ((B == F && A == H) || (B == E && B == D && A != F && A == I)) p0 = B; else p0 = interp(A, B);
                if ((C == H && A == F) || (C == G && C == D && A != H && A == I)) p1 = C; else p1 = interp(A, C);
                p2 = B;
            } else if (A == D && B == C) {
                if (A == B) { p0 = p1 = p2 = A; }
                else {
                    int r = 0;
                    p1 = interp(A, C); p0 = interp(A, B);
                    r += get_result1(A, B, G, E);
                    r += get_result2(B, A, K, F);
                    r += get_result2(B, A, H, N);
                    r += get_result1(A, B, L, O);
                    (void)I; (void)J; (void)M; (void)P;
                    if (r > 0) p2 = A; else if (r < 0) p2 = B; else p2 = qinterp(A, B, C, D);
                }
            } else {
                p2 = qinterp(A, B, C, D);
                if (A == C && A == F && B != E && B == J) p0 = A;
                else if (B == E && B == D && A != F && A == I) p0 = B;
                else p0 = interp(A, B);
                if (A == B && A == H && G != C && C == M) p1 = A;
                else if (C == G && C == D && A != H && A == I) p1 = C;
                else p1 = interp(A, C);
            }
            o0[2 * x] = A; o0[2 * x + 1] = p0; o1[2 * x] = p1; o1[2 * x + 1] = p2;
        }
    }
}

/* -------------------------------------------------------- Super 2xSaI ---- */
static void f_super2xsai(const uint32_t *s, int w, int h, uint32_t *d)
{
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t cB0 = px(s,w,h,x-1,y-1), cB1 = px(s,w,h,x,y-1), cB2 = px(s,w,h,x+1,y-1), cB3 = px(s,w,h,x+2,y-1);
            uint32_t c4 = px(s,w,h,x-1,y),    c5 = px(s,w,h,x,y),    c6 = px(s,w,h,x+1,y),    cS2 = px(s,w,h,x+2,y);
            uint32_t c1 = px(s,w,h,x-1,y+1),  c2 = px(s,w,h,x,y+1),  c3 = px(s,w,h,x+1,y+1),  cS1 = px(s,w,h,x+2,y+1);
            uint32_t cA0 = px(s,w,h,x-1,y+2), cA1 = px(s,w,h,x,y+2), cA2 = px(s,w,h,x+1,y+2), cA3 = px(s,w,h,x+2,y+2);
            uint32_t p1a, p1b, p2a, p2b;
            if (c2 == c6 && c5 != c3) p2b = p1b = c2;
            else if (c5 == c3 && c2 != c6) p2b = p1b = c5;
            else if (c5 == c3 && c2 == c6) {
                int r = 0;
                r += get_result(c6, c5, c1, cA1);
                r += get_result(c6, c5, c4, cB1);
                r += get_result(c6, c5, cA2, cS1);
                r += get_result(c6, c5, cB2, cS2);
                if (r > 0) p2b = p1b = c6; else if (r < 0) p2b = p1b = c5; else p2b = p1b = interp(c5, c6);
            } else {
                if (c6 == c3 && c3 == cA1 && c2 != cA2 && c3 != cA0) p2b = qinterp(c3, c3, c3, c2);
                else if (c5 == c2 && c2 == cA2 && cA1 != c3 && c2 != cA3) p2b = qinterp(c2, c2, c2, c3);
                else p2b = interp(c2, c3);
                if (c6 == c3 && c6 == cB1 && c5 != cB2 && c6 != cB0) p1b = qinterp(c6, c6, c6, c5);
                else if (c5 == c2 && c5 == cB2 && cB1 != c6 && c5 != cB3) p1b = qinterp(c6, c5, c5, c5);
                else p1b = interp(c5, c6);
            }
            if (c5 == c3 && c2 != c6 && c4 == c5 && c5 != cA2) p2a = interp(c2, c5);
            else if (c5 == c1 && c6 == c5 && c4 != c2 && c5 != cA0) p2a = interp(c2, c5);
            else p2a = c2;
            if (c2 == c6 && c5 != c3 && c1 == c2 && c2 != cB2) p1a = interp(c2, c5);
            else if (c4 == c2 && c3 == c2 && c1 != c5 && c2 != cB0) p1a = interp(c2, c5);
            else p1a = c5;
            o0[2 * x] = p1a; o0[2 * x + 1] = p1b; o1[2 * x] = p2a; o1[2 * x + 1] = p2b;
        }
    }
}

/* -------------------------------------------------------- Super Eagle ---- */
static void f_supereagle(const uint32_t *s, int w, int h, uint32_t *d)
{
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t cB1 = px(s,w,h,x,y-1), cB2 = px(s,w,h,x+1,y-1);
            uint32_t c4 = px(s,w,h,x-1,y),  c5 = px(s,w,h,x,y),   c6 = px(s,w,h,x+1,y),   cS2 = px(s,w,h,x+2,y);
            uint32_t c1 = px(s,w,h,x-1,y+1), c2 = px(s,w,h,x,y+1), c3 = px(s,w,h,x+1,y+1), cS1 = px(s,w,h,x+2,y+1);
            uint32_t cA1 = px(s,w,h,x,y+2), cA2 = px(s,w,h,x+1,y+2);
            uint32_t p1a, p1b, p2a, p2b;
            if (c2 == c6 && c5 != c3) {
                p1b = p2a = c2;
                if (c1 == c2 || c6 == cB2) { p1a = interp(c2, c5); p1a = interp(c2, p1a); }
                else p1a = interp(c5, c6);
                if (c6 == cS2 || c2 == cA1) { p2b = interp(c2, c3); p2b = interp(c2, p2b); }
                else p2b = interp(c2, c3);
            } else if (c5 == c3 && c2 != c6) {
                p2b = p1a = c5;
                if (cB1 == c5 || c3 == cS1) { p1b = interp(c5, c6); p1b = interp(c5, p1b); }
                else p1b = interp(c5, c6);
                if (c3 == cA2 || c4 == c5) { p2a = interp(c5, c2); p2a = interp(c5, p2a); }
                else p2a = interp(c2, c3);
            } else if (c5 == c3 && c2 == c6) {
                int r = 0;
                r += get_result(c6, c5, c1, cA1);
                r += get_result(c6, c5, c4, cB1);
                r += get_result(c6, c5, cA2, cS1);
                r += get_result(c6, c5, cB2, cS2);
                if (r > 0) { p1b = p2a = c2; p1a = p2b = interp(c5, c6); }
                else if (r < 0) { p2b = p1a = c5; p1b = p2a = interp(c5, c6); }
                else { p2b = p1a = c5; p1b = p2a = c2; }
            } else {
                p2b = p1a = interp(c2, c6);
                p2b = qinterp(c3, c3, c3, p2b);
                p1a = qinterp(c5, c5, c5, p1a);
                p2a = p1b = interp(c5, c3);
                p2a = qinterp(c2, c2, c2, p2a);
                p1b = qinterp(c6, c6, c6, p1b);
            }
            o0[2 * x] = p1a; o0[2 * x + 1] = p1b; o1[2 * x] = p2a; o1[2 * x + 1] = p2b;
        }
    }
}

/* ---------------------------------------------------- scanlines / TV ---- */
static inline uint32_t scale_rgb(uint32_t c, int num, int den)
{
    uint32_t r = ((c >> 16) & 255) * num / den, g = ((c >> 8) & 255) * num / den, b = (c & 255) * num / den;
    return (r << 16) | (g << 8) | b;
}
static inline uint32_t clamp_rgb(int r, int g, int b)
{
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void f_scanlines(const uint32_t *s, int w, int h, uint32_t *d)
{
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        const uint32_t *row = s + y * w;
        for (int x = 0; x < w; x++) {
            uint32_t c = row[x] & RGBMASK;
            o0[2 * x] = o0[2 * x + 1] = c;
            o1[2 * x] = o1[2 * x + 1] = scale_rgb(c, 1, 2);
        }
    }
}

static void f_tv(const uint32_t *s, int w, int h, uint32_t *d)
{
    /* horizontal interpolation, odd lines are the blend of the two neighbours at 3/4 brightness */
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t A = px(s, w, h, x, y), B = px(s, w, h, x + 1, y);
            uint32_t C = px(s, w, h, x, y + 1), D = px(s, w, h, x + 1, y + 1);
            o0[2 * x] = A; o0[2 * x + 1] = interp(A, B);
            o1[2 * x] = scale_rgb(interp(A, C), 3, 4);
            o1[2 * x + 1] = scale_rgb(qinterp(A, B, C, D), 3, 4);
        }
    }
}

static void f_crt(const uint32_t *s, int w, int h, uint32_t *d)
{
    /* 1) horizontal bloom (1/4,1/2,1/4) on the doubled row, brightness lifted
     * 2) aperture mask: even output columns favour red/blue, odd favour green
     * 3) odd output lines at ~45% (scanline gap) */
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        uint32_t *o0 = d + (2 * y) * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; x++) {
            uint32_t L = px(s, w, h, x - 1, y), A = px(s, w, h, x, y), R = px(s, w, h, x + 1, y);
            /* sub-pixel 0: between L and A, sub-pixel 1: between A and R */
            for (int sub = 0; sub < 2; sub++) {
                uint32_t N = sub ? R : L;
                int r = (((A >> 16) & 255) * 3 + ((N >> 16) & 255)) / 4;
                int g = (((A >> 8) & 255) * 3 + ((N >> 8) & 255)) / 4;
                int b = ((A & 255) * 3 + (N & 255)) / 4;
                /* lift brightness to make up for the scanline gap */
                r = r * 9 / 8; g = g * 9 / 8; b = b * 9 / 8;
                int ox = 2 * x + sub;
                if (ox & 1) { g = g * 17 / 16; r = r * 15 / 16; b = b * 15 / 16; }
                else        { g = g * 15 / 16; }
                uint32_t c = clamp_rgb(r, g, b);
                o0[ox] = c;
                o1[ox] = scale_rgb(c, 9, 20);
            }
        }
    }
}

void filter_apply(int id, const uint32_t *src, int w, int h, uint32_t *dst)
{
    switch (id) {
    case FILTER_SCALE2X:    f_scale2x(src, w, h, dst); break;
    case FILTER_2XSAI:      f_2xsai(src, w, h, dst); break;
    case FILTER_SUPER2XSAI: f_super2xsai(src, w, h, dst); break;
    case FILTER_SUPEREAGLE: f_supereagle(src, w, h, dst); break;
    case FILTER_SCANLINES:  f_scanlines(src, w, h, dst); break;
    case FILTER_TV:         f_tv(src, w, h, dst); break;
    case FILTER_CRT:        f_crt(src, w, h, dst); break;
    default: memcpy(dst, src, (size_t)w * h * 4); break;
    }
}
