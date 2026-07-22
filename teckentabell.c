#include <stdlib.h>
#include <stdio.h>
#include "DEV_Config.h"
#include "IT8951.h"
#include "GUI_Paint.h"
#include "fonts.h"

// GCC för att bygga detta exempel:
// gcc -O3 -Wall -g -D BCM -I. -I./lib/Config -I./lib/e-Paper -I./lib/Fonts -I./lib/GUI main.c ./lib/Config/DEV_Config.c ./lib/GUI/GUI_Paint.c ./lib/e-Paper/EPD_IT8951.c ./lib/Fonts/font24.c -o wimwriter -lbcm2835 -lpthread -lmgcc main.c IT8951.c DEV_Config.c GUI_Paint.c -o font_test -I. -lbcm2835 -lpthread -D BCM

int main(int argc, char *argv[])
{
    // 1. Initiera hårdvara och IT8951-kontroller
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    UDOUBLE Vcom = 2140;
    IT8951_Init(Vcom);

    // 2. Allokera minne för den virtuella framebufferten i RAM
    UWORD Imagesize = ((IT8951_Width % 8 == 0) ? (IT8951_Width / 8) : (IT8951_Width / 8 + 1)) * IT8951_Height;
    UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        perror("Kunde inte allokera bildbuffert");
        return -1;
    }

    // Skapa bildkontext och fyll med vitt
    Paint_NewImage(BlackImage, IT8951_Width, IT8951_Height, 0, WHITE);
    Paint_Clear(WHITE);

    // 3. Konfigurera teckensnitt och padding (4 pixlar)
    sFONT *font = &Font24;
    UBYTE padding = 4;

    UWORD cell_w = font->Width + (padding * 2);
    UWORD cell_h = font->Height + (padding * 2);

    UDOUBLE x = 10;
    UDOUBLE y = 10;

    // 4. Rita ut teckentabellen (ASCII 32 till 126)
    for (int i = 32; i < 127; i++) {
        // Kontrollera radbyte om vi når skärmens kant
        if (x + cell_w > IT8951_Width - 10) {
            x = 10;
            y += cell_h;
        }
        if (y + cell_h > IT8951_Height - 10) {
            break;
        }

        // Rita tecknet med 4 pixlars offset för padding
        Paint_DrawChar(x + padding, y + padding, i, font, WHITE, BLACK);

        x += cell_w;
    }

    // 5. Skicka hela bufferten till IT8951 för visning
    IT8951_Display_Frame(BlackImage, 0, 0, IT8951_Width, IT8951_Height, INIT_Mode);

    // 6. Städa upp resurser
    free(BlackImage);
    DEV_Module_Exit();

    return 0;
}
