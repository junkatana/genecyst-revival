/* font.h -- proportional 1-bit GUI font (Genecyst look): 7 cap rows + 2 descender rows */
#ifndef GENECYST_FONT_H
#define GENECYST_FONT_H
#include <stdint.h>

#define FONT_H 9          /* glyph rows (7 + 2 descender) */
#define FONT_LINE 10      /* menu line pitch */
#define FONT_SPACING 1    /* columns between glyphs (shadow lives here) */
#define FONT_CHECK 1      /* pseudo-char: checkmark */
#define FONT_ARROW_R 2    /* pseudo-char: submenu arrow */

typedef struct {
    uint16_t code;        /* character code */
    uint8_t width;        /* glyph width in pixels (1..16) */
    uint16_t rows[FONT_H];/* bit i = column i */
} FontGlyph;

extern const FontGlyph font_genecyst_glyphs[];
extern const int font_genecyst_count;
extern const FontGlyph font_extra_glyphs[];
extern const int font_extra_count;

const FontGlyph *font_glyph(int ch);   /* falls back to '?' */
int font_text_width(const char *s);

/* small upper-case font used by submenus, hotkeys, dialog labels and the
 * message log: 5 rows, proportional, 1px spacing, space = 6px */
#define FONT_SMALL_H 5
#define FONT_SMALL_SPACE 6
extern const FontGlyph font_small_glyphs[];
extern const int font_small_count;
extern const FontGlyph font_small_extra_glyphs[];
extern const int font_small_extra_count;
const FontGlyph *font_small_glyph(int ch);   /* lower-case is mapped to upper-case */
int font_small_width(const char *s);
#endif
