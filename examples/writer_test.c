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

    // Y-koordinat för cachen (längst ner på din 1072 px höga skärm)
    const int cache_y = 1050; 
    const int num_chars = 104; // ASCII 32 till 135
    
    // En temporär buffert för att ladda upp ETT tecken i taget (14x20 pixlar, 1 byte/pixel)
    UBYTE char_buffer[Font20.Width * Font20.Height];

    for (int c = 0; c < num_chars; c++) {
        // Hitta var detta teckens råa bitmapp startar i Font20Table
        int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
        const UBYTE *ptr = &Font20.table[c * bytes_per_char];

        // Förbered Waveshare-strukturerna för att skriva till skärmens RAM
        IT8951_Load_Img_Info cache_load;
        cache_load.Endian_Type = 0;
        cache_load.Pixel_Format = 8; 
        cache_load.Rotate = 0;
        cache_load.Source_Buffer_Addr = char_buffer;
        cache_load.Target_Memory_Addr = target_addr;

        IT8951_Area_Img_Info cache_area;
        cache_area.Area_X = c * Font20.Width;
        cache_area.Area_Y = cache_y;
        cache_area.Area_W = Font20.Width;
        cache_area.Area_H = Font20.Height;

        // Skriv upp tecknet till skärmens dolda minne
        EPD_IT8951_HostAreaPackedPixelWrite_8bp(&cache_load, &cache_area);
    }
    printf("Glyph-cache redo i VRAM!\n");
    // =================================================================
    

    // 2. Öppna tangentbordet (event0)
    const char *kbd_dev = "/dev/input/event0";
    int kbd_fd = open(kbd_dev, O_RDONLY);
    if (kbd_fd == -1) {
        perror("Kunde inte öppna tangentbordet! Körde du med sudo?");
        DEV_Module_Exit();
        return EXIT_FAILURE;
    }

    printf("Skärm redo (Bredd: %d, Höjd: %d) och tangentbord anslutet!\n", dev_info.Panel_W, dev_info.Panel_H);
    printf("Skriv på tangentbordet för att testa cachen (A, B, C, Å, Ä, Ö)...\n");
    printf("Tryck på Ctrl+C i terminalen för att avsluta.\n");

    // Vi definierar en startposition för vår "markör"
    int cursor_x = 100;
    int cursor_y = 100;

    struct input_event ev;

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

            // 1. Packa upp bitmappen från Font20 för JUST denna bokstav till en temporär 8bpp-buffert
            int cache_index = ascii_char - 32;
            UBYTE single_char_buf[Font20.Width * Font20.Height];
            
            int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
            const UBYTE *ptr = &Font20.table[cache_index * bytes_per_char];

            for (int y = 0; y < Font20.Height; y++) {
                UBYTE byte1 = ptr[y * 2];
                UBYTE byte2 = ptr[y * 2 + 1];
                unsigned int row_bits = (byte1 << 8) | byte2;

                for (int x = 0; x < Font20.Width; x++) {
                    if ((row_bits << x) & 0x8000) {
                        single_char_buf[y * Font20.Width + x] = 0x00; // Svart
                    } else {
                        single_char_buf[y * Font20.Width + x] = 0xFF; // Vit
                    }
                }
            }

            // 2. Beräkna målposition på skärmen (Roterat och klart)
            int target_x = dev_info.Panel_W - cursor_x - Font20.Width;
            int target_y = dev_info.Panel_H - cursor_y - Font20.Height;

            // 3. Skriv bokstavens pixlar till skärmens VRAM-buffert på rätt koordinater
            IT8951_Load_Img_Info draw_load;
            draw_load.Endian_Type = 0;
            draw_load.Pixel_Format = 8; // 8-bitars gråskala
            draw_load.Rotate = 0;
            draw_load.Source_Buffer_Addr = single_char_buf;
            draw_load.Target_Memory_Addr = target_addr;

            IT8951_Area_Img_Info draw_area;
            draw_area.Area_X = target_x;
            draw_area.Area_Y = target_y;
            draw_area.Area_W = Font20.Width;
            draw_area.Area_H = Font20.Height;

            EPD_IT8951_HostAreaPackedPixelWrite_8bp(&draw_load, &draw_area);

            // 4. BE SKÄRMEN ATT UPPDATERA DETTA OMRÅDE FRÅN SITT INTERNA MINNE (target_addr)
            EPD_IT8951_Display_AreaBuf(
                target_x, 
                target_y, 
                Font20.Width, 
                Font20.Height, 
                A2_Mode, 
                target_addr
            );

            // 5. Flytta markören framåt
            cursor_x += Font20.Width;

            // Radbrytning om vi når kanten (marginal på 100 pixlar)
            if (cursor_x + Font20.Width > dev_info.Panel_W - 100) {
                cursor_x = 100;
                cursor_y += (Font20.Height + 4);
            }
        }
    }
    
    // Stäng allt snyggt (Körs om loopen mot förmodan skulle avbrytas)
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
