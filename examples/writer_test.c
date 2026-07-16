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
        // Varje tecken i Font20 tar upp exakt 40 bytes (2 bytes per rad * 20 rader)
        int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
        const UBYTE *ptr = &Font20.table[c * bytes_per_char];

        // Packa upp bitmappen (1 och 0) till hela bytes (0x00=Svart, 0xFF=Vit)
        for (int y = 0; y < Font20.Height; y++) {
            // Font20 använder 2 bytes per rad i källkoden för att hålla 14 pixlars bredd
            UBYTE byte1 = ptr[y * 2];
            UBYTE byte2 = ptr[y * 2 + 1];
            
            // Slå ihop dem till ett 16-bitars ord för enklare bit-skiftning
            unsigned int row_bits = (byte1 << 8) | byte2;

            for (int x = 0; x < Font20.Width; x++) {
                // Kolla om biten på denna X-position är satt (1 = svart pixel i Waveshare)
                // Vi skiftar från högsta biten (vänster) till lägsta (höger)
                if ((row_bits << x) & 0x8000) {
                    char_buffer[y * Font20.Width + x] = 0x00; // Svart pixel
                } else {
                    char_buffer[y * Font20.Width + x] = 0xFF; // Vit (transparent/bakgrund)
                }
            }
        }

        // Förbered Waveshare-strukturerna för att skriva detta tecken till skärmens RAM
        IT8951_Load_Img_Info cache_load;
        cache_load.Endian_Type = 0;
        cache_load.Pixel_Format = 8; // 8-bitars läge (Snabba skyffeln!)
        cache_load.Rotate = 0;
        cache_load.Source_Buffer_Addr = char_buffer;
        cache_load.Target_Memory_Addr = target_addr;

        IT8951_Area_Img_Info cache_area;
        // Placera tecknen på en lång rad: X-positionen blir index * bredd
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
    printf("Skriv på TADA68 för att rita block...\n");
    printf("Tryck på Ctrl+C i terminalen för att avsluta.\n");

    // Vi definierar en startposition för vår "markör"
    int cursor_x = 100;
    int cursor_y = 100;
    const int block_width = 32;
    const int block_height = 48;

    // 1-bitars buffert: (Bredd * Höjd) / 8 bytes
    // const int buffer_size = (block_width * block_height) / 8;
    // UBYTE block_buffer[buffer_size];
    //
    // Tillbaks till 8-bitars gråskala (1 byte per pixel:
    UBYTE block_buffer[block_width * block_height];
    
    // Fyll bufferten med svarta pixlar.
    // for(int i = 0; i < buffer_size; i++) {
    for(int i = 0; i < block_width * block_height; i++) {
        block_buffer[i] = 0x00; 
    }

    struct input_event ev;

    // Vår lyssningsloop
    while (read(kbd_fd, &ev, sizeof(struct input_event)) > 0) {
        // Om en tangent trycks ned (EV_KEY och value == 1)
        if (ev.type == EV_KEY && ev.value == 1) {
            
            // Sätt upp Waveshares bild-info-struktur för uppladdningen
            IT8951_Load_Img_Info load_info;
            load_info.Endian_Type = 0;          // Little Endian
            // load_info.Pixel_Format = 1;      // 1 bit per pixel (svartvitt!)
	    load_info.Pixel_Format = 8;		//Tillbaka till 8 bits per pixel
            load_info.Rotate = 0;               // Ingen rotation (vi roterar i mjukvara)
            load_info.Source_Buffer_Addr = block_buffer;
            load_info.Target_Memory_Addr = target_addr;

            // Sätt upp strukturen för var på skärmen vi ritar
            IT8951_Area_Img_Info area_info;
	    area_info.Area_X = dev_info.Panel_W - cursor_x - block_width;
	    area_info.Area_Y = dev_info.Panel_H - cursor_y - block_height;
            area_info.Area_W = block_width;
            area_info.Area_H = block_height;

            // Skriv pixeldata till IT8951-kortets minne
            // EPD_IT8951_HostAreaPackedPixelWrite_1bp(&load_info, &area_info, true);
            EPD_IT8951_HostAreaPackedPixelWrite_8bp(&load_info, &area_info);

            // Rita ut området på den fysiska skärmen
	    // EPD_IT8951_Display_1bp(
	    EPD_IT8951_Display_AreaBuf(
   		dev_info.Panel_W - cursor_x - block_width,
    		dev_info.Panel_H - cursor_y - block_height,
    		block_width,
    		block_height,
    		A2_Mode,
    		target_addr//,
		// 0x00,
		// 0xFF
	    );
            // Flytta fram "markören" för nästa block
            cursor_x += (block_width + 8); // 8 pixlars mellanrum

            // Enkel radbrytning om vi slår i högerkanten av skärmen
            if (cursor_x + block_width > dev_info.Panel_W) {
                cursor_x = 100;
                cursor_y += (block_height + 16); // Flytta ner en rad
            }
            
            // Om vi når botten av skärmen börjar vi om från toppen
            if (cursor_y + block_height > dev_info.Panel_H) {
                cursor_y = 100;
                printf("Skärmen full! Rensar...\n");
                EPD_IT8951_Clear_Refresh(dev_info, target_addr, GC16_Mode);
            }
        }
    }

    // Stäng allt snyggt
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
