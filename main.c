#include <stdio.h>
#include <stdlib.h>
#include "lib/Config/DEV_Config.h"
#include "lib/GUI/GUI_Paint.h"
#include "lib/e-Paper/IT8951.h"
#include "lib/Fonts/fonts.h"

#define LCD_WIDTH   1448
#define LCD_HEIGHT  1072

int main(void)
{
    // 1. Initiera hårdvara och IT8951 med VCOM -2.14V (2140)
    if (DEV_Module_Init() != 0) return -1;
    IT8951_Init(2140);

    // 2. Allokera minne för den virtuella 1-bpp framebuffern i RAM (~194 KB)
    UDOUBLE Imagesize = ((LCD_WIDTH % 8 == 0) ? (LCD_WIDTH / 8) : (LCD_WIDTH / 8 + 1)) * LCD_HEIGHT;
    UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        DEV_Module_Exit();
        return -1;
    }

    // Koppla bufferten till Waveshares Paint-modul
    Paint_NewImage(BlackImage, LCD_WIDTH, LCD_HEIGHT, 0, WHITE);
    Paint_Clear(WHITE);
    IT8951_Clear_Screen(INIT_Mode);

    // 3. Tangenttryckning / teckenrendering
    UDOUBLE cursor_x = 50;
    UDOUBLE cursor_y = 50;
    char c = 'A';

    // Rita tecknet direkt i RAM-bufferten med Font24
    Paint_DrawChar(cursor_x, cursor_y, c, &Font24, WHITE, BLACK);

    // 4. Skicka endast tecknets yta (bounding box) till skärmen i A2-läge
    IT8951_Display_Area(cursor_x, cursor_y, Font24.Width, Font24.Height, A2_Mode);

    // Städa upp
    free(BlackImage);
    DEV_Module_Exit();
    return 0;
}
