#ifndef WIM_FONT_H
#define WIM_FONT_H

#include "lib/Config/DEV_Config.h"

#define FONT_WIDTH  32
#define FONT_HEIGHT 64
#define FONT_CHAR_BYTES (FONT_WIDTH * FONT_HEIGHT)

// Struct for single 32 x 64 character
typedef struct {
    const UBYTE bitmap[FONT_CHAR_BYTES];
} WimChar;

// Swedish ASCII 0 - 127.
// Index 91 : Ä
// Index 92 : Ö
// Index 93 : Å
// Index 124 : ä
// Index 125 : å
// Index 96 : ö

#endif
