/* res_data.h -- GUI bitmaps extracted from the original GENECYST.EXE
 * (palette-indexed, 255 = transparent). See tools/extract_exe_resources.py. */
#ifndef GENECYST_RES_DATA_H
#define GENECYST_RES_DATA_H
#include <stdint.h>
typedef struct { const char *name; int w, h; const uint8_t *px; } ResImage;
extern const uint32_t res_palette[256];
extern const ResImage res_images[];
extern const int res_image_count;
const ResImage *res_image(const char *name);   /* NULL when missing */
#endif
