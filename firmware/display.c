#include <stdio.h>
#include "display.h"
#include "wim_font_courier.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"

extern void EPD_IT8951_ReadBusy(void);

void init_display(UDOUBLE *target_addr) {
    IT8951_Dev_Info Dev_Info;

    if (DEV_Module_Init() != 0) {
        printf("Fel vid initiering av displayen\n");
        return;
    }

    Dev_Info = EPD_IT8951_Init(2140);
    *target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    EPD_IT8951_Clear_Refresh(Dev_Info, *target_addr, 0);
}

void render_char(char c, int x, int y, UDOUBLE target_addr) {
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Vänd tecknet rätt genom att läsa font-arrayen baklänges (uppochner och spegelvänt)
    static UBYTE flipped_glyph[GLYPH_SIZE_BYTES];
    const uint8_t *src = wim_font_32x64[(int)c];
    for (int i = 0; i < GLYPH_SIZE_BYTES; i++) {
        flipped_glyph[GLYPH_SIZE_BYTES - 1 - i] = src[i];
    }

    load_info.Source_Buffer_Addr = (UBYTE*)flipped_glyph;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = GLYPH_W;
    area_info.Area_H = GLYPH_H;

    // EPD_IT8951_WaitForDisplayReady(); // Blockerar inputtråden
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // Denna loop är för långsam
    // for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
    //     DEV_SPI_WriteByte(flipped_glyph[i]);
    // }
    fast_spi_write_nbyte(flipped_glyph, GLYPH_SIZE_BYTES);

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    EPD_IT8951_Display_Area(x, y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
}


void cleanup_display(void) {
     DEV_Module_Exit();
}

int get_physical_x(int col) {
    return SCREEN_WIDTH - MARGIN_RIGHT - ((col + 1) * GLYPH_W);
}

int get_physical_y(int row) {
    return SCREEN_HEIGHT - MARGIN_BOTTOM - ((row + 1) * GLYPH_H);
}

void redraw_buffer(char (buffer)[MAX_ROWS][MAX_COLS], UDOUBLE target_addr) {
    for (int row = 0; row < MAX_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            // Vi skickar ALLA tecken, även mellanslag, för att "sudda" den gamla texten
            render_char(buffer[row][col], get_physical_x(col), get_physical_y(row), target_addr);
        }
    }
}

void display_jump(char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // 1. Flytta upp de 5 nedersta raderana till de 5 översta
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            buffer[row][col] = buffer[MAX_ROWS - JUMP_LINES + row][col];
        }
    }

    // 2. tÖM RESTEN AV BUFFERTEN (RAD 6 - 15)
    for (int row = JUMP_LINES; row < MAX_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            buffer[row][col] = ' ';
        }
    }

    // 3. Flytta markören till rad 6[cite: 1, 2]
    *cursor_row = JUMP_LINES;
    *cursor_col = 0;

    // 4. Rita om hela skärytan i A2-läge[cite: 1]
    redraw_buffer(buffer, target_addr);
}
