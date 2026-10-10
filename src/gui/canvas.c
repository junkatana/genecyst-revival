#include "canvas.h"
#include "font.h"
#include <string.h>

void cv_init(Canvas *c, uint32_t *px, int w, int h)
{
    c->px = px; c->w = w; c->h = h;
    cv_noclip(c);
}

void cv_clip(Canvas *c, int x, int y, int w, int h)
{
    c->cx0 = x < 0 ? 0 : x;
    c->cy0 = y < 0 ? 0 : y;
    c->cx1 = x + w > c->w ? c->w : x + w;
    c->cy1 = y + h > c->h ? c->h : y + h;
}

void cv_noclip(Canvas *c) { c->cx0 = 0; c->cy0 = 0; c->cx1 = c->w; c->cy1 = c->h; }

void cv_clear(Canvas *c, uint32_t color)
{
    for (int i = 0; i < c->w * c->h; i++) c->px[i] = color;
}

void cv_pixel(Canvas *c, int x, int y, uint32_t color)
{
    if (x < c->cx0 || y < c->cy0 || x >= c->cx1 || y >= c->cy1) return;
    c->px[y * c->w + x] = color;
}

void cv_hline(Canvas *c, int x, int y, int w, uint32_t color)
{
    if (y < c->cy0 || y >= c->cy1) return;
    int x0 = x < c->cx0 ? c->cx0 : x, x1 = x + w > c->cx1 ? c->cx1 : x + w;
    uint32_t *p = c->px + y * c->w;
    for (int i = x0; i < x1; i++) p[i] = color;
}

void cv_vline(Canvas *c, int x, int y, int h, uint32_t color)
{
    if (x < c->cx0 || x >= c->cx1) return;
    int y0 = y < c->cy0 ? c->cy0 : y, y1 = y + h > c->cy1 ? c->cy1 : y + h;
    for (int i = y0; i < y1; i++) c->px[i * c->w + x] = color;
}

void cv_fill(Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    for (int j = y; j < y + h; j++) cv_hline(c, x, j, w, color);
}

void cv_rect(Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    cv_hline(c, x, y, w, color);
    cv_hline(c, x, y + h - 1, w, color);
    cv_vline(c, x, y, h, color);
    cv_vline(c, x + w - 1, y, h, color);
}

void cv_bevel(Canvas *c, int x, int y, int w, int h, uint32_t light, uint32_t dark)
{
    cv_hline(c, x, y, w, light);
    cv_vline(c, x, y, h, light);
    cv_hline(c, x, y + h - 1, w, dark);
    cv_vline(c, x + w - 1, y, h, dark);
}

void cv_bitmap(Canvas *c, int x, int y, const uint16_t *rows, int w, int h, uint32_t color)
{
    for (int j = 0; j < h; j++) {
        uint16_t r = rows[j];
        if (!r) continue;
        for (int i = 0; i < w; i++)
            if (r & (1u << i)) cv_pixel(c, x + i, y + j, color);
    }
}

void cv_bitmap8(Canvas *c, int x, int y, const uint8_t *rows, int w, int h, uint32_t color)
{
    for (int j = 0; j < h; j++) {
        uint8_t r = rows[j];
        if (!r) continue;
        for (int i = 0; i < w; i++)
            if (r & (1u << i)) cv_pixel(c, x + i, y + j, color);
    }
}

int cv_char(Canvas *c, int x, int y, int ch, uint32_t color)
{
    if (ch == ' ') return 3;
    const FontGlyph *g = font_glyph(ch);
    cv_bitmap(c, x, y, g->rows, g->width, FONT_H, color);
    return g->width + FONT_SPACING;
}

int cv_text_shadow(Canvas *c, int x, int y, const char *s, uint32_t color, uint32_t shadow)
{
    int x0 = x;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == ' ') { x += 3; continue; }
        const FontGlyph *g = font_glyph(*p);
        cv_bitmap(c, x + 1, y + 1, g->rows, g->width, FONT_H, shadow);
        x += g->width + FONT_SPACING;
    }
    x = x0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) x += cv_char(c, x, y, *p, color);
    return x - x0;
}

int cv_text(Canvas *c, int x, int y, const char *s, uint32_t color)
{
    return cv_text_shadow(c, x, y, s, color, C_SHADOW);
}

void cv_blit_indexed(Canvas *c, int x, int y, const uint8_t *src, int w, int h, const uint32_t *palette)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            uint8_t p = src[j * w + i];
            if (p != 255) cv_pixel(c, x + i, y + j, palette[p]);
        }
}

void cv_blit_mask(Canvas *c, int x, int y, const uint8_t *src, int w, int h, uint32_t color)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (src[j * w + i] != 255) cv_pixel(c, x + i, y + j, color);
}

void cv_blit(Canvas *c, int x, int y, const uint32_t *src, int src_pitch_px, int w, int h)
{
    for (int j = 0; j < h; j++) {
        int dy = y + j;
        if (dy < c->cy0 || dy >= c->cy1) continue;
        int x0 = x < c->cx0 ? c->cx0 : x, x1 = x + w > c->cx1 ? c->cx1 : x + w;
        if (x1 <= x0) continue;
        memcpy(c->px + dy * c->w + x0, src + j * src_pitch_px + (x0 - x), (size_t)(x1 - x0) * 4);
    }
}

static int small_pass(Canvas *c, int x, int y, const char *s, uint32_t color)
{
    int x0 = x;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == ' ') { x += FONT_SMALL_SPACE; continue; }
        const FontGlyph *g = font_small_glyph(*p);
        cv_bitmap(c, x, y, g->rows, g->width, FONT_SMALL_H, color);
        x += g->width + FONT_SPACING;
    }
    return x - x0;
}

int cv_text_small_noshadow(Canvas *c, int x, int y, const char *s, uint32_t color)
{
    return small_pass(c, x, y, s, color);
}

int cv_text_small(Canvas *c, int x, int y, const char *s, uint32_t color)
{
    small_pass(c, x + 1, y + 1, s, C_SHADOW);
    return small_pass(c, x, y, s, color);
}
