/* gfx.h -- software 2D primitives on a 32-bit XRGB canvas */
#ifndef GENECYST_CANVAS_H
#define GENECYST_CANVAS_H
#include <stdint.h>

typedef struct {
    uint32_t *px;
    int w, h;
    int cx0, cy0, cx1, cy1;   /* clip rect (exclusive max) */
} Canvas;

/* Genecyst palette, sampled from the original at 320x240 */
#define C_BLACK   0x000000
#define C_RED     0xC31411   /* menu / window body */
#define C_DKRED   0x950D10   /* menu shade / separators */
#define C_LTRED   0xED5355   /* blood drip highlight */
#define C_WINFRAME 0x790B0D  /* 1px window outline */
#define C_WINTITLE 0x5E090A  /* window title bar / bottom button band */
#define C_WINBODY  0x9D0F10  /* window body */
#define C_FOCUS    0x209C07  /* green focus outline */
#define C_BTN      0x60646F  /* button / close box / scrollbar body */
#define C_BTNLIGHT 0x7C8190  /* button top & right edge */
#define C_BTNDARK  0x444750  /* button left & bottom edge */
#define C_TRACK    0x292B31  /* scrollbar track, close box glyph */
#define C_TEXT    0xD4DCF6   /* normal text (light blue-white) */
#define C_WHITE   0xF2FBFF
#define C_SHADOW  0x151618   /* text drop shadow */
#define C_GRAY3   0xB1B7CD
#define C_GRAY2   0x9095A7
#define C_GRAY1   0x707482
#define C_GRAY0   0x50545E
#define C_DKGRAY  0x313339
#define C_GREEN   0x209C07   /* list selection / active menu title / focus */
#define C_LTGREEN 0x40E040
#define C_YELLOW  0xF0C000
#define C_MSGGREEN 0x58F94F  /* message log text */
#define C_MSGGRAY  0x989EB0  /* newest message line */
#define C_HOTKEY   0x58F94F  /* menu hotkeys */

void cv_init(Canvas *c, uint32_t *px, int w, int h);
void cv_clip(Canvas *c, int x, int y, int w, int h);
void cv_noclip(Canvas *c);
void cv_clear(Canvas *c, uint32_t color);
void cv_pixel(Canvas *c, int x, int y, uint32_t color);
void cv_hline(Canvas *c, int x, int y, int w, uint32_t color);
void cv_vline(Canvas *c, int x, int y, int h, uint32_t color);
void cv_fill(Canvas *c, int x, int y, int w, int h, uint32_t color);
void cv_rect(Canvas *c, int x, int y, int w, int h, uint32_t color);
/* 3D box: light top-left, dark bottom-right */
void cv_bevel(Canvas *c, int x, int y, int w, int h, uint32_t light, uint32_t dark);
/* 1-bit bitmap, LSB = leftmost pixel of each row word */
void cv_bitmap(Canvas *c, int x, int y, const uint16_t *rows, int w, int h, uint32_t color);
void cv_bitmap8(Canvas *c, int x, int y, const uint8_t *rows, int w, int h, uint32_t color);
/* text with 1px drop shadow (shadow drawn first, +1,+1); returns advance */
int  cv_text(Canvas *c, int x, int y, const char *s, uint32_t color);
int  cv_text_shadow(Canvas *c, int x, int y, const char *s, uint32_t color, uint32_t shadow);
int  cv_char(Canvas *c, int x, int y, int ch, uint32_t color);
/* small upper-case font (5 rows) with 1px drop shadow; returns advance */
int  cv_text_small(Canvas *c, int x, int y, const char *s, uint32_t color);
int  cv_text_small_noshadow(Canvas *c, int x, int y, const char *s, uint32_t color);
/* blit XRGB source into canvas at x,y (clipped) */
void cv_blit(Canvas *c, int x, int y, const uint32_t *src, int src_pitch_px, int w, int h);
/* blit a palette-indexed image (255 = transparent) through a 256-entry palette */
void cv_blit_indexed(Canvas *c, int x, int y, const uint8_t *src, int w, int h, const uint32_t *palette);
/* same, but every opaque pixel is drawn in one colour (for glyph-like images) */
void cv_blit_mask(Canvas *c, int x, int y, const uint8_t *src, int w, int h, uint32_t color);
#endif
