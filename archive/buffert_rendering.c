// Förslag på olika sätt att rendera en cache

// Detta använder Waveshares Paint-bibliotek
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "EPD_IT8951.h"

// Storlek för 1-bpp framebuffer: (1448 * 1072) / 8 = 194464 bytes
#define BUFFER_SIZE (1448 * 1072 / 8)

void run_framebuffer_example(void) {
    // 1. Allokera minne för den virtuella skärmen i RAM
    UBYTE *frame_buffer = (UBYTE *)malloc(BUFFER_SIZE);
    if (frame_buffer == NULL) {
        printf("Kunde inte allokera minne för rambuffert.\n");
        return;
    }

    // 2. Initiera ytan med vit bakgrund (WHITE = 1)
    Paint_NewImage(frame_buffer, 1448, 1072, 0, WHITE);
    Paint_SelectImage(frame_buffer);
    Paint_Clear(WHITE);

    // 3. Rita lite text i bufferten med standardbiblioteket
    Paint_DrawString_EN(50, 50, "Wimwriter v0.1", &Font24, WHITE, BLACK);
    Paint_DrawString_EN(50, 90, "Skrivmaskinen är redo.", &Font20, WHITE, BLACK);

    // 4. Skicka hela bufferten till IT8951-kontrollern
    // (Använder Waveshares funktion för bildinläsning och visning)
    EPD_IT8951_Display_Frame(frame_buffer, 0, 0, 1448, 1072, 0);

    // 5. Städa upp
    free(frame_buffer);
}


// Läs font-tabellen direkt utan GUI_Paint
// Eftersom Waveshares inbyggda font-tabeller (font24.c) redan är lagrade
// i rent 1-bpp-format (där 1 är text och 0 är bakgrund) kan vi hoppa över
// GUI_Paint helt och hållet när vi bygger cachen.
void init_glyph_cache(sFONT *font) {
    // Beräkna bytes per rad med 8-bit alignment
    int bytes_per_line = (font->Width + 7) / 8;
    int bytes_per_char = font->Height * bytes_per_line;
    UWORD aligned_width = bytes_per_line * 8;

    for (int i = 32; i < 127; i++) { // ASCII 32 till 126
        int offset = (i - ' ') * bytes_per_char;

        // Kontrollera att inte indexet går utanför font-tabellen
        // (eller nollställ cachen först)
        memset(ascii_cache[i].bitmap, 0x00, sizeof(ascii_cache[i].bitmap));

        // Kopiera rådata direkt från font-strukturen till cachen
        memcpy(ascii_cache[i].bitmap, &font->table[offset], bytes_per_char);

        ascii_cache[i].width = aligned_width;
        ascii_cache[i].height = font->Height;
        ascii_cache[i].is_cached = true;
    }

    printf("Glyph-cache inläst direkt från font-tabell med korrekt (?) polaritet.\n");
}
