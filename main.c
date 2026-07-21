#include <stdio.h>
#include <stdlib.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "EPD_IT8951.h"
#include "fonts.h"

int main(void)
{
    // 1. Initiera hårdvara och IT8951 med VCOM (-2.14V skrivs som 2140)
    if (DEV_Module_Init() != 0) return -1;

    IT8951_Dev_Info Dev_Info;
    Dev_Info = EPD_IT8951_Init(2140);

    // 2. Allokera minne för den virtuella 1-bpp framebuffern i RAM (~194 KB)
    UDOUBLE Imagesize = ((Dev_Info.Panel_W % 8 == 0) ? (Dev_Info.Panel_W / 8) : (Dev_Info.Panel_W / 8 + 1)) * Dev_Info.Panel_H;
    UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        DEV_Module_Exit();
        return -1;
    }

    // Koppla bufferten till Waveshares Paint-modul
    Paint_NewImage(BlackImage, Dev_Info.Panel_W, Dev_Info.Panel_H, 0, WHITE);
    Paint_Clear(WHITE);

    // Initial fullständig rensning av skärmen
    UDOUBLE Target_Memory_Addr = (UDOUBLE)Dev_Info.Memory_Addr_L | ((UDOUBLE)Dev_Info.Memory_Addr_H << 16);
    EPD_IT8951_Clear_Refresh(Dev_Info, Target_Memory_Addr, INIT_Mode);

    // 3. Tangenttryckning / teckenrendering med font24
    UDOUBLE cursor_x = 50;
    UDOUBLE cursor_y = 50;
    char c = 'A';

    Paint_DrawChar(cursor_x, cursor_y, c, &Font24, WHITE, BLACK);

    // 4. Skicka endast tecknets yta (bounding box) till skärmen i A2-läge
    EPD_IT8951_Display_Area(cursor_x, cursor_y, Font24.Width, Font24.Height, A2_Mode);

    // Städa upp
    free(BlackImage);
    DEV_Module_Exit();
    return 0;
}
