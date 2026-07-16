#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include "../lib/Fonts/fonts.h"

// Vi inkluderar Waveshares egna headers för att prata med skärmen
#include "../lib/Config/DEV_Config.h"
#include "../lib/e-Paper/EPD_IT8951.h"

int main(int argc, char *argv[]) {
    // 1. Initiera hårdvaran och SPI-bussen via Waveshares bibliotek
    if (DEV_Module_Init() != 0) {
        printf("Kunde inte initiera hårdvarumodulen!\n");
        return EXIT_FAILURE;
    }

    // Sätt VCOM manuellt (-2.14 V -> -2140 mV)
    double vcom = -2.14;
    UWORD vcom_mv = (UWORD)(vcom * -1000); 

    printf("Initierar skärmen med VCOM: -%.2f V (%d mV)...\n", -vcom, vcom_mv);
    
    // Denna funktion hämtar skärmens info och sätter VCOM
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(vcom_mv);
    
    // Räkna ut den fullständiga 32-bitars måladressen i IT8951:s RAM
    UDOUBLE target_addr = ((UDOUBLE)dev_info.Memory_Addr_H << 16) | dev_info.Memory_Addr_L;

    // Rensar skärmen till vit (0xFF) i det säkra, högkvalitativa GC16-läget vid uppstart
    printf("Rensar skärmen...\n");
    EPD_IT8951_Clear_Refresh(dev_info, target_addr, GC16_Mode);

    // =================================================================
    // INITIALISERA GLYPH CACHE (TECKENKARTA) I VRAM
    // =================================================================
    printf("Bygger glyph-cache för Font20 (inkl. Å, Ä, Ö, é, à)...\n");

    // RIKTIG LYSSNINGSLOOP (while istället för if)
    while (read(kbd_fd, &ev, sizeof(struct input_event)) > 0) {
	if (ev.type == EV_KEY && ev.value == 1) {
            int ascii_char = 32;
                
            if (ev.code == 30) ascii_char = 'a'; 
            else if (ev.code == 48) ascii_char = 'b'; 
            else if (ev.code == 46) ascii_char = 'c'; 
            else if (ev.code == 39) ascii_char = 131; // Å
            else if (ev.code == 40) ascii_char = 132; // Ä
            else if (ev.code == 43) ascii_char = 133; // Ö
            else ascii_char = 32; 

            int cache_index = ascii_char - 32;
            UBYTE single_char_buf[Font20.Width * Font20.Height];
            
            // Nollställ bufferten till VIT (0xFF) innan vi ritar bokstaven
            for(int i = 0; i < Font20.Width * Font20.Height; i++) {
                single_char_buf[i] = 0xFF;
            }
            
            int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
            const UBYTE *ptr = &Font20.table[cache_index * bytes_per_char];

            for (int y = 0; y < Font20.Height; y++) {
                UBYTE byte1 = ptr[y * 2];
                UBYTE byte2 = ptr[y * 2 + 1];
                unsigned int row_bits = (byte1 << 8) | byte2;

                for (int x = 0; x < Font20.Width; x++) {
                    if ((row_bits << x) & 0x8000) {
                        single_char_buf[y * Font20.Width + x] = 0x00; // Svart
                    }
                }
            }

            int target_x = dev_info.Panel_W - cursor_x - Font20.Width;
            int target_y = dev_info.Panel_H - cursor_y - Font20.Height;

            IT8951_Load_Img_Info draw_load;
            draw_load.Endian_Type = 0;
            draw_load.Pixel_Format = 2; // RÄTTAD: 2 betyder 8-bpp gråskala i IT8951!
            draw_load.Rotate = 0;
            draw_load.Source_Buffer_Addr = single_char_buf;
            draw_load.Target_Memory_Addr = target_addr;

            IT8951_Area_Img_Info draw_area;
            draw_area.Area_X = target_x;
            draw_area.Area_Y = target_y;
            draw_area.Area_W = Font20.Width;
            draw_area.Area_H = Font20.Height;

            EPD_IT8951_HostAreaPackedPixelWrite_8bp(&draw_load, &draw_area);

            // Använd din fungerande AreaBuf för att tvinga skärmen att visa minnet
            EPD_IT8951_Display_AreaBuf(target_x, target_y, Font20.Width, Font20.Height, A2_Mode, target_addr);

            cursor_x += Font20.Width;

            if (cursor_x + Font20.Width > dev_info.Panel_W - 100) {
    }
    
    // Stäng allt snyggt (Körs om loopen mot förmodan skulle avbrytas)
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
