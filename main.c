#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "IT8951.h"
#include "fonts.h"

// Skärmens fysiska dimensioner för HD-hatten
#define LCD_WIDTH   1448
#define LCD_HEIGHT  1072

int main(void)
{
    // 1. Initiera hårdvara och SPI via BCM2835
    if (DEV_Module_Init() != 0) {
        printf("Fel: Kunde inte initiera modul.\n");
        return -1;
    }

    // Hämta VCOM (exempelvis -2.14V skrivs som 2140)
    UWORD VCOM = 2140;

    // Initiera IT8951-kontrollern
    IT8951_Init(VCOM);

    // 2. Skapa en virtuell framebuffer i RAM (1-bpp, 1 bit per pixel)
    // Storlek i bytes: (1448 * 1072) / 8 = 194368 bytes
    UDOUBLE Imagesize = ((LCD_WIDTH % 8 == 0) ? (LCD_WIDTH / 8) : (LCD_WIDTH / 8 + 1)) * LCD_HEIGHT;
    UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
    if (BlackImage == NULL) {
        printf("Fel: Kunde inte allokera minne för framebuffern.\n");
        DEV_Module_Exit();
        return -1;
    }

    // Rensa bufferten (vit bakgrund = 0xFF i 1-bpp för e-paper beroende på inversion,
    // men Paint_NewImage nollställer eller sätter färg via Paint_Clear)
    Paint_NewImage(BlackImage, LCD_WIDTH, LCD_HEIGHT, 0, WHITE);
    Paint_Clear(WHITE);

    // 3. Gör en initial fullständig rensning av skärmen
    IT8951_Clear_Screen(INIT_Mode);

    // Exempelposition för skrivningen
    UDOUBLE cursor_x = 50;
    UDOUBLE cursor_y = 50;

    // Exempel: Skriv ut ett tecken med Font20 och uppdatera via Bounding Box (A2-läge)
    char sample_char = 'A';

    // Rita tecknet i vår RAM-buffer med Waveshares ritfunktion
    Paint_DrawChar(cursor_x, cursor_y, sample_char, &Font20, WHITE, BLACK);

    // 4. Skicka endast den förändrade ytan (Bounding Box) till IT8951
    // Font20 har en viss bredd och höjd (t.ex. Width och Height)
    UWORD box_x = cursor_x;
    UWORD box_y = cursor_y;
    UWORD box_w = Font20.Width;
    UWORD box_h = Font20.Height;

    // Ladda upp den specifika regionen från minnet till IT8951 Target Memory Address
    // (Detta är en förenklad struktur beroende på hur RAM-adresserna i IT8951 hanteras)
    IT8951_Display_Area(box_x, box_y, box_w, box_h, A2_Mode);

    // Städa upp vid avslut
    free(BlackImage);
    DEV_Module_Exit();
    return 0;
}
