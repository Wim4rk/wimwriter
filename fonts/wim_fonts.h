#ifndef WIM_FONTS_H
#define WIM_FONTS_H

#include <stdint.h>

/* Dimensioner och bytestorlekar för statisk allokering */

#define FONT_24X43_W 24
#define FONT_24X43_H 43
#define FONT_24X43_BYTES 1032

extern const uint8_t wim_font_24x43[256][FONT_24X43_BYTES];

#endif // WIM_FONTS_H
