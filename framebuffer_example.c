//GCC för att bygga detta exempel:
// gcc -O3 -Wall -g -D BCM -I. -I./lib/Config -I./lib/e-Paper -I./lib/Fonts -I./lib/GUI main.c ./lib/Config/DEV_Config.c ./lib/GUI/GUI_Paint.c ./lib/e-Paper/EPD_IT8951.c ./lib/Fonts/font24.c -o wimwriter -lbcm2835 -lpthread -lm

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
