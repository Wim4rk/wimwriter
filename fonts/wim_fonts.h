#ifndef WIM_FONTS_H
#define WIM_FONTS_H

#include <stdint.h>

/* Dimensioner och bytestorlekar för statisk allokering */

#define FONT_24X32_W 24
#define FONT_24X32_H 32
#define FONT_24X32_BYTES 768

#define FONT_24X41_W 24
#define FONT_24X41_H 43
#define FONT_24X41_BYTES 984

/*
 * Typsnittsdatan deklareras externt här.
 * Arrayerna instansieras sedan i wim_fonts.h för att laddas direkt i RAM
 * vid uppstart av maskinen.
 */
extern const uint8_t wim_font_24x32[256][FONT_24X32_BYTES];
extern const uint8_t wim_font_24x41[128][FONT_24X43_BYTES];

#endif // WIM_FONTS_H
