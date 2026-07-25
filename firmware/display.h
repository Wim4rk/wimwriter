#ifndef DISPLAY_H
#define DISPLAY_H

#include "../lib/e-Paper/EPD_IT8951.h"

#define GLYPH_W 32
#define GLYPH_H 64

#define IT8951_A2_MODE 6
#define GLYPH_SIZE_BYTES 2048

void init_display(UDOUBLE *target_addr);
void render_char(char c, int x, int y, UDOUBLE target_addr);
void cleanup_display(void);

#endif
