#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>

#include "lib/Config/DEV_Config.h"
#include "lib/e-Paper/EPD_IT8951.h"
#include "lib/Fonts/fonts.h"

// 1. Datastruktur för den pre-genererade cachen
typedef struct {
    UBYTE bitmap[256]; // Maxstorlek för en 1-bpp glyph. Tillräckligt för t.ex. 32x64 px.
    UWORD width;
    UWORD height;
    bool is_cached;
} GlyphCache;

// Global cache-array för standard-ASCII (0-127)
GlyphCache ascii_cache[128];

// 2. Funktion: Förbered cachen vid uppstart
void init_glyph_cache(sFONT *font) {
    int bytes_per_line = (font->Width + 7) / 8;
    int bytes_per_char = font->Height * bytes_per_line;

    for (int i = 32; i < 127; i++) { // ASCII 32 (Space) till 126 (~)
        int offset = (i - ' ') * bytes_per_char;

        // Kopiera över den monokroma datan från font-filen till RAM-cachen
        memcpy(ascii_cache[i].bitmap, &font->table[offset], bytes_per_char);
        ascii_cache[i].width = font->Width;
        ascii_cache[i].height = font->Height;
        ascii_cache[i].is_cached = true;
    }
    printf("Glyph-cache förberedd i RAM.\n");
}

// 3. Funktion: Rendera ett tecken direkt från cachen
void render_cached_char(char c, int x, int y, UDOUBLE target_addr) {
    // Om tecknet inte finns i cachen hoppar vi över
    if (c < 0 || c > 127 || !ascii_cache[(int)c].is_cached) {
        return;
    }

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Peka in i vår pre-allokerade RAM-cache för att undvika CPU-cykler
    load_info.Source_Buffer_Addr = ascii_cache[(int)c].bitmap;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_2BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = ascii_cache[(int)c].width;
    area_info.Area_H = ascii_cache[(int)c].height;

    // Skicka den lilla rutan (damage box) över SPI till controllern
    EPD_IT8951_HostAreaPackedPixelWrite_1bp(&load_info, &area_info, true);

    // Uppdatera skärmen i det snabba A2-läget (som motsvarar Mode 6 för aktuell LUT)
    EPD_IT8951_Display_AreaBuf(x, y, area_info.Area_W, area_info.Area_H, 6, target_addr);
}

// 4. Huvudprogram och inmatningsloop
int main() {
    int cursor_x = 100;
    int cursor_y = 100;
    IT8951_Dev_Info Dev_Info;

    if (DEV_Module_Init() != 0) {
        return -1;
    }

    // Tvinga en hårdvaru-reset före initiering
    DEV_Reset();

    printf("Modul initierad. Startar EPD...\n");
    fflush(stdout);

    // Init-anrop med VCOM satt till 2140 (-2.14V)
    Dev_Info = EPD_IT8951_Init(2140);
    printf("Init-anrop avklarat.\n");
    fflush(stdout);

    UDOUBLE target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    // Bygg upp font-cachen i minnet före skärmrensningen
    init_glyph_cache(&Font24);

    // Rensa skärmen vid uppstart
    EPD_IT8951_Clear_Refresh(Dev_Info, target_addr, 0);

    // Öppna inmatningsströmmen från tangentbordet
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        printf("Kunde inte öppna tangentbordet. Kontrollera behörigheter (sudo).\n");
        return 1;
    }

    printf("Skrivmaskinen startad. Lyssnar på inmatning...\n");
    fflush(stdout);

    struct input_event ev;

    // Evdev-loop som lyssnar på inmatning
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) { // 1 = Key press

            // TODO: Mappa ev.code till ett faktiskt char-tecken här.
            // För detta utkast låtsas vi att varje knapp motsvarar 'A'.
            char c = 'A';

            render_cached_char(c, cursor_x, cursor_y, target_addr);

            cursor_x += ascii_cache[(int)c].width;

            // Enkel radbrytning om markören når skärmens högra kant
            if(cursor_x > (Dev_Info.Panel_W - ascii_cache[(int)c].width)) {
                cursor_x = 100; // Återställ till vänstermarginal
                cursor_y += ascii_cache[(int)c].height; // Flytta ned en rad
            }
        }
    }

    close(fd);
    DEV_Module_Exit();
    return 0;
}
