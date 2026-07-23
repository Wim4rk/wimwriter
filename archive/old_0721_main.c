#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>

#include "lib/Config/DEV_Config.h"
#include "lib/e-Paper/EPD_IT8951.h"
#include "lib/Fonts/fonts.h"

/*
// Vi förbereder en global eller statisk konfiguration för renderingen
static IT8951_Dev_Info dev_info;

// Lägg till denna buffert
static UBYTE char_buffer[100]; //Stor nog för 20x20 pixlar
*/

/**
 * Förbereder load_info med adressen till bitmap-datan för ett specifikt tecken.
 * Index 0 i tabellen motsvarar ' ' (ASCII 32).
 */
void prepare_char_bitmap(char c, IT8951_Load_Img_Info *load_info) {
    int bytes_per_line = 2; // (Font20.Width + 7) / 8
    int bytes_per_char = Font20.Height * bytes_per_line;

    int index = (int)c - 32;

    // Säkerhetskontroll: Om tecknet är utanför standard-ASCII,
    // returnera adressen för ett '?' eller mellanslag för att undvika segmenteringsfel.
    if (index < 0 || index > 94) {
        index = 0; // Fallback till mellanslag
    }

    // Beräkna offset (40 bytes per tecken enligt font20.c)
    int offset = index * bytes_per_char;

    static UBYTE test_pattern[40];
// Skapa ett mönster: varannan byte 0xAA, varannan 0x55 (ger ränder)
for(int i=0; i<40; i++) test_pattern[i] = (i % 2 == 0) ? 0xAA : 0x55;

load_info->Source_Buffer_Addr = test_pattern;

    // Konfigurera load_info
    // Vi använder pekararitmetik för att peka direkt in i Font20-tabellen
    load_info->Source_Buffer_Addr = (UBYTE *)&Font20.table[offset];

    // Vi behåller formatinställningar här för att hålla render-funktionen ren
    load_info->Pixel_Format = IT8951_3BPP;
    load_info->Endian_Type = IT8951_LDIMG_B_ENDIAN;
    load_info->Rotate = IT8951_ROTATE_0;
}

char map_evdev_to_ascii(int code) {
    // Exempel: evdev kod 30 = 'a', 31 = 's'
    // Detta är en enkel mappning för teständamål
    switch(code) {
        case 30: return 'A'; // Hårdkodat för test
        case 48: return 'B';
        case 46: return 'C';
        case 57: return ' ';
        default: return 'x';
    }
}


void render_char_fast(char c, int x, int y, UDOUBLE target_addr, IT8951_Load_Img_Info *load_info, IT8951_Area_Img_Info *area_info) {

    UDOUBLE active_render_addr = 122480; // Testa hårdkodad address
    int index = (c - ' ');
    if (index < 0 || index > 95) return;

    // Använd konfigurationen från prepare_char_bitmap, ändra bara målet
    load_info->Target_Memory_Addr = target_addr;

    printf("Renderar '%c' vid %d, %d\n", c, x, y);

    area_info->Area_X = x;
    area_info->Area_Y = y;
    area_info->Area_W = Font20.Width;  // Skulle förmodligen vara 14...
    area_info->Area_H = Font20.Height; // Är förmodligen 20...

    static UBYTE debug_bitmap[40];
    for(int i=0; i<40; i++) debug_bitmap[i] = 0xAA;

    load_info->Source_Buffer_Addr = debug_bitmap;

    // Skriv till kontrollern
    EPD_IT8951_HostAreaPackedPixelWrite_1bp(load_info, area_info, false);

    // Uppdatera skärmen
    // EPD_IT8951_Display_AreaBuf(x, y, 16, 20, 2, target_addr);
    EPD_IT8951_Display_AreaBuf(x, y, 16, 20, 2, active_render_addr);
}

int main() {
    int cursor_x = 100;
    int cursor_y = 100;

    IT8951_Dev_Info Dev_Info;
    IT8951_Load_Img_Info load_img_info;
    IT8951_Area_Img_Info area_img_info;

    if (DEV_Module_Init() != 0) {
        return -1;
    }

    printf("Modul initierad. Startar EPD...\n");

    // Init-anrop
    Dev_Info = EPD_IT8951_Init(2140);

    // Häpmta adressen från den initierade strukturen
    // Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);

    printf("Skärmstorlek: %d x %d\n", Dev_Info.Panel_W, Dev_Info.Panel_H);

    UDOUBLE base_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    // Testa
    printf("Debug: Memory_Addr_L: %d\n", Dev_Info.Memory_Addr_L);
    printf("Debug: Memory_Addr_H: %d\n", Dev_Info.Memory_Addr_H);

    if (Dev_Info.Panel_W == 0) {
        printf("VARNING: Ingen kontakt med skärmen! Panel_W är 0.\n");
    } else {
        printf("Kontakt upprättad. Panel: %d x %d\n", Dev_Info.Panel_W, Dev_Info.Panel_H);
}

    printf("Waveshares base_addr: %d\n", base_addr);
    // Initial Clear
    EPD_IT8951_Clear_Refresh(Dev_Info, base_addr, 0);

    // Öppna input
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) return 1;

    printf("Skrivmaskinen startad. Lyssnar på inmatning...\n");

    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {

            char current_char = map_evdev_to_ascii(ev.code);

            printf("Tangent tryckt: kod %d\n", ev.code);

            // Förbered glyfen
            prepare_char_bitmap(current_char, &load_img_info);

            //Skriv ut glyfen
            render_char_fast(current_char, cursor_x, cursor_y, base_addr, &load_img_info, &area_img_info);

            cursor_x += Font20.Width;

            if(cursor_x > (Dev_Info.Panel_W - Font20.Width)) {
                cursor_x = 100;
                cursor_y += Font20.Height;
            }
        }
    }

    close(fd);
    DEV_Module_Exit();
    return 0;
}
