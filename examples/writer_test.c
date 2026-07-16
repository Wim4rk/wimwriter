#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include "../lib/Fonts/fonts.h"

// Include Waveshare hardware configuration and e-Paper drivers
#include "../lib/Config/DEV_Config.h"
#include "../lib/e-Paper/EPD_IT8951.h"

int main(int argc, char *argv[]) {
    // 1. Initialize hardware module and SPI communication
    if (DEV_Module_Init() != 0) {
        printf("Failed to initialize hardware module!\n");
        return EXIT_FAILURE;
    }

    // Set VCOM manually (-2.14 V -> -2140 mV)
    double vcom = -2.14;
    UWORD vcom_mv = (UWORD)(vcom * -1000); 

    printf("Initializing display with VCOM: -%.2f V (%d mV)...\n", -vcom, vcom_mv);
    
    // Get device info and apply VCOM settings
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(vcom_mv);
    
    // Calculate the full 32-bit target memory address in IT8951 VRAM
    UDOUBLE target_addr = ((UDOUBLE)dev_info.Memory_Addr_H << 16) | dev_info.Memory_Addr_L;

    // Clear display to pure white (0xFF) using the high-quality GC16 mode at startup
    printf("Clearing screen...\n");
    EPD_IT8951_Clear_Refresh(dev_info, target_addr, GC16_Mode);

    // 2. Open the keyboard input device (event0)
    const char *kbd_dev = "/dev/input/event0";
    int kbd_fd = open(kbd_dev, O_RDONLY);
    if (kbd_fd == -1) {
        perror("Failed to open keyboard device! Did you run with sudo?");
        DEV_Module_Exit();
        return EXIT_FAILURE;
    }

    printf("Display ready (%dx%d) and keyboard connected!\n", dev_info.Panel_W, dev_info.Panel_H);
    printf("Type on the keyboard to test rendering (A, B, C, Å, Ä, Ö)...\n");
    printf("Press Ctrl+C in terminal to exit.\n");

    // Define initial typewriter cursor coordinates
    int cursor_x = 100;
    int cursor_y = 100;

    struct input_event ev;

    // Main keyboard event listener loop
    while (read(kbd_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {
                
            int ascii_char = 32;
                
            // Keycode translation to custom font ASCII indexes
            if (ev.code == 30) ascii_char = 'a'; 
            else if (ev.code == 48) ascii_char = 'b'; 
            else if (ev.code == 46) ascii_char = 'c'; 
            else if (ev.code == 39) ascii_char = 131; // Custom mapping for Å
            else if (ev.code == 40) ascii_char = 132; // Custom mapping for Ä
            else if (ev.code == 43) ascii_char = 133; // Custom mapping for Ö
            else ascii_char = 32; 

            printf("\n--- Debugging ASCII Char: %d ('%c') ---\n", ascii_char, (ascii_char >= 32 && ascii_char <= 126) ? ascii_char : '?');

            // 1. Extract the glyph bitmap from Font20 into a temporary local 8bpp buffer
	    int cache_index = ascii_char - 32;
            
            // Reverted back to UBYTE (8-bit per pixel) as expected by the _8bp function
            UBYTE single_char_buf[Font20.Width * Font20.Height];
            
            // Clear buffer to white. 0xF0 represents white in 4-bit/8-bit hybrid IT8951 VRAM
            memset(single_char_buf, 0xF0, sizeof(single_char_buf));
            
            int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
            const UBYTE *ptr = &Font20.table[cache_index * bytes_per_char];

            // Unpack bits from font table and print a live preview in the terminal window
            for (int y = 0; y < Font20.Height; y++) {
                UBYTE byte1 = ptr[y * 2];
                UBYTE byte2 = ptr[y * 2 + 1];
                unsigned int row_bits = (byte1 << 8) | byte2;

                for (int x = 0; x < Font20.Width; x++) {
                    if ((row_bits << x) & 0x8000) {
                        single_char_buf[y * Font20.Width + x] = 0x00; // Black pixel
                        printf("#"); 
                    } else {
                        printf("."); 
                    }
                }
                printf("\n");
            }
            printf("---------------------------------------\n");

            // 2. Calculate target position on screen (adjusted for current rotation layout)
            int target_x = dev_info.Panel_W - cursor_x - Font20.Width;
            int target_y = dev_info.Panel_H - cursor_y - Font20.Height;

            // 3. Write glyph pixels to the IT8951 VRAM buffer
            IT8951_Load_Img_Info draw_load;
            draw_load.Endian_Type = 0;
            draw_load.Pixel_Format = 2; // 2 explicitly defines 8-bpp mode
            draw_load.Rotate = 0;
            draw_load.Source_Buffer_Addr = single_char_buf; // Clean 8-bit pointer
            draw_load.Target_Memory_Addr = target_addr;

            IT8951_Area_Img_Info draw_area;
            draw_area.Area_X = target_x;
            draw_area.Area_Y = target_y;
            draw_area.Area_W = Font20.Width;
            draw_area.Area_H = Font20.Height;

            EPD_IT8951_HostAreaPackedPixelWrite_8bp(&draw_load, &draw_area);

            // 4. Trigger localized low-latency refresh using fast A2 mode from internal VRAM
            EPD_IT8951_Display_AreaBuf(
                target_x, 
                target_y, 
                Font20.Width, 
                Font20.Height, 
                A2_Mode, 
                target_addr
            );

            // 5. Advance typewriter cursor horizontally
            cursor_x += Font20.Width;

            // Simple line break handler when reaching the right edge (100px margin)
            if (cursor_x + Font20.Width > dev_info.Panel_W - 100) {
                cursor_x = 100;
                cursor_y += (Font20.Height + 4); // 4px line spacing
            }
        }
    } 

    // Clean up resources upon exit
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
