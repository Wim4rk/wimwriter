#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "lib/Config/DEV_Config.h"
#include "lib/e-Paper/EPD_IT8951.h"
#include "lib/Fonts/fonts.h"

// gcc shapes.c IT8951.c DEV_Config.c GUI_Paint.c -o shapes -I. -lbcm2835 -lpthread -D BCM

int main(int argc, char *argv[])
{
    // 1. Initiera hårdvara och IT8951-kontroller
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    UDOUBLE Vcom = 2140;
    IT8951_Init(Vcom);

    // 2. Allokera minne för den virtuella framebufferten
    UWORD Imagesize = ((IT8951_Width % 8 == 0) ? (IT8951_Width / 8) : (IT8951_Width / 8 + 1)) * IT8951_Height;
    UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        return -1;
    }

    Paint_NewImage(BlackImage, IT8951_Width, IT8951_Height, 0, WHITE);
    Paint_Clear(WHITE);

    // 3. Slumpa ut geometriska former över ytan
    srand(time(NULL));
    int total_shapes = 40;

    for (int i = 0; i < total_shapes; i++) {
        UWORD x1 = rand() % (IT8951_Width - 250);
        UWORD y1 = rand() % (IT8951_Height - 250);
        UWORD x2 = x1 + (rand() % 200 + 30);
        UWORD y2 = y1 + (rand() % 200 + 30);

        int type = rand() % 3;

        if (type == 0) {
            // Tom kvadrat
            Paint_DrawRectangle(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        } else if (type == 1) {
            // Fylld kvadrat
            Paint_DrawRectangle(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        } else {
            // Rakt streck
            Paint_DrawLine(x1, y1, x2, y2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
    }

    // 4. Skicka hela bufferten till skärmen
    IT8951_Display_Frame(BlackImage, 0, 0, IT8951_Width, IT8951_Height, INIT_Mode);

    // 5. Städa upp
    free(BlackImage);
    DEV_Module_Exit();

    return 0;
}
