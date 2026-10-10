/* filters.h -- software video filters applied to the composed virtual screen
 * (game + GUI) before it is handed to the renderer. All filters produce an
 * output of (w*factor) x (h*factor) XRGB8888 pixels. */
#ifndef GENECYST_FILTERS_H
#define GENECYST_FILTERS_H
#include <stdint.h>

enum {
    FILTER_NONE = 0,
    FILTER_SCALE2X,     /* AdvMAME2x */
    FILTER_2XSAI,       /* Kreed's 2xSaI */
    FILTER_SUPER2XSAI,  /* Kreed's Super 2xSaI */
    FILTER_SUPEREAGLE,  /* Kreed's Super Eagle */
    FILTER_SCANLINES,   /* 2x with every other line darkened */
    FILTER_TV,          /* interpolated 2x + soft scanlines */
    FILTER_CRT,         /* scanlines + phosphor mask + horizontal bloom */
    FILTER_COUNT
};

const char *filter_name(int id);          /* menu label, e.g. "2xSaI" */
const char *filter_key(int id);           /* command-line key, e.g. "2xsai" */
int  filter_by_key(const char *key);      /* -1 when unknown */
int  filter_factor(int id);               /* output scale factor (1 or 2) */
/* src: w*h pixels (pitch == w); dst: (w*f)*(h*f) pixels (pitch == w*f) */
void filter_apply(int id, const uint32_t *src, int w, int h, uint32_t *dst);

#endif
