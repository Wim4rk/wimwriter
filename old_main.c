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

void prepare_char_bitmap(char c, IT8951_Load_Img_Info *load_info) {
    // Beräkna offset för glyphen (ASCII ' ' är start)
    int bytes_per_char = Font20.Height * (Font20.Width / 8);
    int offset = (c - ' ') * bytes_per_char;

    // 2. Sätt källadressen till rätt position i font-tabellen
    load_info->Source_Buffer_Addr = (UBYTE *)&Font20.table[offset];

    // 3. Konfigurera pixelformat och endianness
    load_info->Pixel_Format = IT8951_3BPP; // Använd 1 bit för font-data
    load_info->Endian_Type = IT8951_LDIMG_B_ENDIAN;
    load_info->Rotate = IT8951_ROTATE_0;
}

void render_char_fast(char c, int x, int y, UDOUBLE target_addr, IT8951_Load_Img_Info *load_info, IT8951_Area_Img_Info *area_info) {

    // Använd Font20 direkt för att beräkna storleken
    int bytes_per_line = (Font20.Width + 7) / 8; //  (14+7)/8 = 2
    int bytes_per_char = Font20.Height * bytes_per_line;

    // Debug: tvinga offset för 'A'
    // int offset = (65 - 32) * 40; // 1320
    // printf("DEBUG: Använder offset %d för '%c'\n", offset, c);

    // int offset = (c - ' ') * bytes_per_char;
    //const UBYTE *bitmap = &Font20.table[offset];
    const UBYTE *bitmap = &Font20.table[40];
    printf("DEBUG: Byte på offset 40: 0x%02X\n", bitmap[0]);

    printf("Renderar '%c' vid %d, %d\n", c, x, y);

    // Uppdatera positionen i area_info
    area_info->Area_X = x;
    area_info->Area_Y = y;
    area_info->Area_W = Font20.Width;
    area_info->Area_H = Font20.Height;

    load_info->Source_Buffer_Addr = (UBYTE*)bitmap;
    load_info->Target_Memory_Addr = target_addr;
    load_info->Pixel_Format = IT8951_3BPP; // Stor risk att 8BPP ska användas

    load_info->Pixel_Format = IT8951_3BPP;
    load_info->Rotate = IT8951_ROTATE_0;
    load_info->Endian_Type = IT8951_LDIMG_B_ENDIAN;

    // Debug
    printf("DEBUG: Första 4 byten för '%c': 0x%02X 0x%02X 0x%02X 0x%02X\n",
       c, bitmap[0], bitmap[1], bitmap[2], bitmap[3]);

    // Skriv till kontrollern
    EPD_IT8951_HostAreaPackedPixelWrite_1bp(load_info, area_info, true);

    // Uppdatera skärmen, mode 0 verkar satbilast
    EPD_IT8951_Display_AreaBuf(x, y, Font20.Width, Font20.Height, 2, target_addr);
}

int main() {
    int cursor_x = 100;
    int cursor_y = 100;

    IT8951_Load_Img_Info load_img_info;
    IT8951_Area_Img_Info area_img_info;

    //Deklarera dess lokalt:
    IT8951_Dev_Info Dev_Info;
    // UDOUBLE Init_Target_Memory_Addr;

    if (DEV_Module_Init() != 0) {
        return -1;
    }

    printf("Modul initierad. Startar EPD...\n");

    // Init-anrop
    Dev_Info = EPD_IT8951_Init(2140);
    printf("Vi klarade init-anropet!");

    // Häpmta adressen från den initierade strukturen
    // Init_Target_Memory_Addr = Dev_Info.Memory_Addr_L | (Dev_Info.Memory_Addr_H << 16);

    // EPD_IT8951_Clear_Refresh(Dev_Info, Init_Target_Memory_Addr, 0);

    printf("Skärmstorlek: %d x %d\n", Dev_Info.Panel_W, Dev_Info.Panel_H);

    UDOUBLE target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    // Testa
    printf("Debug: Memory_Addr_L: %d\n", Dev_Info.Memory_Addr_L);
    printf("Debug: Memory_Addr_H: %d\n", Dev_Info.Memory_Addr_H);

    // Testa en hård rendering
    // printf("Testar rendering av 'A'...\n");
    // render_char_fast('A', 100, 100, target_addr, &load_img_info, &area_img_info);

    // Rensar skärmen till vitt (0 = vit, 1 = svart, beroende på modell)
    // EPD_IT8951_Clear_Refresh(Dev_Info, target_addr, 1);

    if (Dev_Info.Panel_W == 0) {
        printf("VARNING: Ingen kontakt med skärmen! Panel_W är 0.\n");
    } else {
        printf("Kontakt upprättad. Panel: %d x %d\n", Dev_Info.Panel_W, Dev_Info.Panel_H);
}

    // Initial Clear
    EPD_IT8951_Clear_Refresh(Dev_Info, target_addr, 0);

    // Öppna input
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) return 1;

    printf("Skrivmaskinen startad. Lyssnar på inmatning...\n");

    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {

            // Vänta tills skärmen är ledig?
            // EPD_IT8951_WaitForDisplayReady();

            // Förbered datan för just detta tecken
            prepare_char_bitmap('A', &load_img_info);

            printf("Tangent tryckt: kod %d\n", ev.code);

            //Skriv ut glyfen
            render_char_fast('A', cursor_x, cursor_y, target_addr, &load_img_info, &area_img_info);

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
