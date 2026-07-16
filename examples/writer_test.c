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

            // Skip spaces or unmapped keys for this test
            if (ascii_char == 32) {
                continue;
            }

            printf("\n--- Rendering ASCII Char: %d ---\n", ascii_char);

            // Allocate a 16-bit buffer. Since Width is 14, we need 14/2 = 7 UWORDs per row.
            // Total size: (Width / 2) * Height
            int words_per_row = Font20.Width / 2;
            UWORD single_char_buf[words_per_row * Font20.Height];
            
            // Initialize buffer to pure white (0xFFFF means two white pixels side-by-side)
            for (int i = 0; i < words_per_row * Font20.Height; i++) {
                single_char_buf[i] = 0xFFFF;
            }
            
            int cache_index = ascii_char - 32;
            int bytes_per_char = ((Font20.Width + 7) / 8) * Font20.Height;
            const UBYTE *ptr = &Font20.table[cache_index * bytes_per_char];

            // Unpack font matrix bits into the packed 16-bit structure
            for (int y = 0; y < Font20.Height; y++) {
                UBYTE byte1 = ptr[y * 2];
                UBYTE byte2 = ptr[y * 2 + 1];
                unsigned int row_bits = (byte1 << 8) | byte2;

                for (int x = 0; x < Font20.Width; x++) {
                    if ((row_bits << x) & 0x8000) {
                        int word_x = x / 2;
                        int buf_idx = y * words_per_row + word_x;
                        
                        // Pack two 8-bit pixels into one 16-bit word based on even/odd column
                        if (x % 2 == 0) {
                            single_char_buf[buf_idx] &= 0xFF00; // Set low byte (pixel 1) to black (0x00)
                        } else {
                            single_char_buf[buf_idx] &= 0x00FF; // Set high byte (pixel 2) to black (0x00)
                        }
                    }
                }
            }

            int target_x = cursor_x;
            int target_y = cursor_y;

            // 3. Configure load info for the IT8951 packed controller format
            IT8951_Load_Img_Info draw_load;
            draw_load.Endian_Type = 0;
            draw_load.Pixel_Format = 2; // 8-bpp packed mode
            draw_load.Rotate = 0;
            // The function expects UBYTE*, cast our UWORD array safely
            draw_load.Source_Buffer_Addr = (UBYTE*)single_char_buf;
            draw_load.Target_Memory_Addr = target_addr;

            IT8951_Area_Img_Info draw_area;
            draw_area.Area_X = target_x;
            draw_area.Area_Y = target_y;
            draw_area.Area_W = Font20.Width;
            draw_area.Area_H = Font20.Height;

            EPD_IT8951_HostAreaPackedPixelWrite_8bp(&draw_load, &draw_area);

            // 4. Trigger localized low-latency refresh using the working AreaBuf function
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

            // Line break logic (100px safety margin)
            if (cursor_x + Font20.Width > dev_info.Panel_W - 100) {
                cursor_x = 100;
                cursor_y += (Font20.Height + 4);
            }
        }
    } 

    // Clean up hardware state on exit
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
