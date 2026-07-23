#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>

#include "lib/Config/DEV_Config.h"
#include "lib/e-Paper/EPD_IT8951.h"
// Inkludera din nya teckentabell istället för standard fonts.h
#include "fonts/wim_font_courier.h"

#define GLYPH_W 32
#define GLYPH_H 64
#define GLYPH_SIZE_BYTES 2048 // 32 * 64 vid 8bpp
#define IT8951_A2_MODE 6

// 1. Enkel evdev-mappning för bokstäver (A-Z) och mellanslag
// Detta behöver du bygga ut för att täcka hela det mekaniska tangentbordet
char map_keycode_to_char(int code) {
    if (code >= KEY_Q && code <= KEY_P) {
        // En väldigt grundläggande QWERTY-mappning som exempel
        const char qwerty[] = "0000000000000000qwertyuiop000000000asdfghjkl00000zxcvbnm";
        if (code < sizeof(qwerty)) return qwerty[code];
    }
    if (code == KEY_SPACE) return ' ';
    return -1; // Ogiltigt / ohanterat tecken
}

// 2. Funktion: Rendera ett tecken blixtsnabbt över SPI ("bare-metal")
void render_fast_char(char c, int x, int y, UDOUBLE target_addr) {
    // Endast utskrivbara tecken i standard-ASCII + svensk offset
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Peka direkt in i din statiska array. Ingen CPU-overhead!
    load_info.Source_Buffer_Addr = (UBYTE*)wim_font_32x64[(int)c];
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP; // Tvinga 8bpp för 1:1 pixel/byte-mappning
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = GLYPH_W;
    area_info.Area_H = GLYPH_H;

    // Förbered hårdvaran för data
    EPD_IT8951_WaitForDisplayReady();
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    // Bypass Waveshares klumpiga loop. Mata in datan rakt över SPI!
    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    const uint8_t *pixel_ptr = wim_font_32x64[(int)c];
    for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
        DEV_SPI_WriteByte(*pixel_ptr++);
    }

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Utför "damage box"-uppdateringen i A2-läge (mode 6)
    EPD_IT8951_Display_Area(x, y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
}

// 3. Huvudprogram
int main() {
    int cursor_x = 100;
    int cursor_y = 100;
    IT8951_Dev_Info Dev_Info;

    if (DEV_Module_Init() != 0) {
        return -1;
    }

    printf("Modul initierad. Startar EPD...\n");
    fflush(stdout);

    // Init-anrop med VCOM satt till 2140 (-2.14V) enligt specifikationen
    Dev_Info = EPD_IT8951_Init(2140);
    printf("Init-anrop avklarat.\n");
    fflush(stdout);

    UDOUBLE target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

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

    // Extremt lättviktig evdev-loop
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) { // 1 = Key press (nedtryckning)

            char c = map_keycode_to_char(ev.code);

            if (c != -1) {
                render_fast_char(c, cursor_x, cursor_y, target_addr);

                cursor_x += GLYPH_W;

                // Radbrytning
                if(cursor_x > (Dev_Info.Panel_W - GLYPH_W - 100)) { // 100 px högermarginal
                    cursor_x = 100; // Återställ till vänstermarginal
                    cursor_y += GLYPH_H; // Flytta ned en rad
                }
            }
        }
    }

    close(fd);
    DEV_Module_Exit();
    return 0;
}
