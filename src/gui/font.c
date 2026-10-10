#include "font.h"
#include <string.h>

static const FontGlyph fallback = { '?', 5, {0x0e,0x11,0x10,0x08,0x04,0x00,0x04,0x00,0x00} };

const FontGlyph *font_glyph(int ch)
{
    static const FontGlyph *table[256];
    static int built;
    if (!built) {
        for (int i = 0; i < font_extra_count; i++)
            if (font_extra_glyphs[i].code < 256) table[font_extra_glyphs[i].code] = &font_extra_glyphs[i];
        for (int i = 0; i < font_genecyst_count; i++)   /* extracted glyphs win */
            if (font_genecyst_glyphs[i].code < 256) table[font_genecyst_glyphs[i].code] = &font_genecyst_glyphs[i];
        built = 1;
    }
    if (ch >= 0 && ch < 256 && table[ch]) return table[ch];
    if (ch >= 'a' && ch <= 'z' && table[ch - 32]) return table[ch - 32];
    if (ch >= 'A' && ch <= 'Z' && table[ch + 32]) return table[ch + 32];
    return table['?'] ? table['?'] : &fallback;
}

int font_text_width(const char *s)
{
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        w += (*p == ' ') ? 3 : font_glyph(*p)->width + FONT_SPACING;
    return w;
}

/* ---- small font --------------------------------------------------------- */
const FontGlyph *font_small_glyph(int ch)
{
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    for (int i = 0; i < font_small_count; i++) if (font_small_glyphs[i].code == ch) return &font_small_glyphs[i];
    for (int i = 0; i < font_small_extra_count; i++) if (font_small_extra_glyphs[i].code == ch) return &font_small_extra_glyphs[i];
    for (int i = 0; i < font_small_extra_count; i++) if (font_small_extra_glyphs[i].code == '?') return &font_small_extra_glyphs[i];
    return &font_small_glyphs[0];
}

int font_small_width(const char *s)
{
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        w += (*p == ' ') ? FONT_SMALL_SPACE : font_small_glyph(*p)->width + FONT_SPACING;
    return w;
}
