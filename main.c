#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>

#include "lib/Config/DEV_Config.h"
#include "lib/e-Paper/EPD_IT8951.h"
#include "lib/Fonts/fonts.h"

char map_evdev_to_ascii(int code) {
    switch(code) {
        case 30: return 'A';
        case 48: return 'B';
        case 46: return 'C';
        case 57: return ' ';
        default: return 'X';
    }
}

// Hämtar och packar glyph från Font24
void prepare_font24_glyph(char c, UBYTE *dest_buffer, int *width, int *height) {
    int index = c - ' ';
    if (index < 0 || index > 94) {
        index = 0;
    }

    *width = Font24.Width;
    *height = Font24.Height;

    int bytes_per_row = (Font24.Width + 7) / 8;
    int char_offset = index * Font24.Height * bytes_per_row;
    const UBYTE *src = &Font24.Table[char_offset];

    for (int y = 0; y < Font24.Height; y++) {
        for (int x = 0; x < bytes_per_row; x++) {
            dest_buffer[y * bytes_per_row + x] = src[y * bytes_per_row + x];
        }
    }
}

void render_char_fast(char c, int x, int y, UDOUBLE target_addr, IT8951_Load_Img_Info *load_info, IT8951_Area_Img_Info *area_info) {
    int w, h;
    static UBYTE glyph_cache[150]; // Font24 är större, kräver lite mer utrymme

    prepare_font24_glyph(c, glyph_cache, &w, &h);

    load_info->Source_Buffer_Addr = glyph_cache;
    load_info->Target_Memory_Addr = target_addr;
    load_info->Pixel_Format = IT8951_3BPP;
    load_info->Endian_Type = IT8951_LDIMG_B_ENDIAN;
    load_info->Rotate = IT8951_ROTATE_0;

    area_info->Area_X = x;
    area_info->Area_Y = y;
    area_info->Area_W = w;
    area_info->Area_H = h;

    EPD_IT8951_HostAreaPackedPixelWrite_1bp(load_info, area_info, false);
    EPD_IT8951_Display_AreaBuf(x, y, w, h, 2, target_addr);
}

int main() {
    int cursor_x = 100;
    int cursor_y = 100;

    IT8951_Dev_Info dev_info;
    IT8951_Load_Img_Info load_img_info;
    IT8951_Area_Img_Info area_img_info;

    if (DEV_Module_Init() != 0) {
        return -1;
    }

    dev_info = EPD_IT8951_Init(2140);
    UDOUBLE base_addr = ((UDOUBLE)dev_info.Memory_Addr_H << 16) | dev_info.Memory_Addr_L;

    if (dev_info.Panel_W == 0) {
        printf("Ingen kontakt med skärmen.\n");
        DEV_Module_Exit();
        return -1;
    }

    EPD_IT8951_Clear_Refresh(dev_info, base_addr, 0);

    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        DEV_Module_Exit();
        return 1;
    }

    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {
            char current_char = map_evdev_to_ascii(ev.code);

            render_char_fast(current_char, cursor_x, cursor_y, base_addr, &load_img_info, &area_img_info);

            cursor_x += Font24.Width;
            if (cursor_x > (dev_info.Panel_W - Font24.Width)) {
                cursor_x = 100;
                cursor_y += Font24.Height;
            }
        }
    }

    close(fd);
    DEV_Module_Exit();
    return 0;
}
