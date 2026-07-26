#include <stdio.h>
#include "display.h"
#include "wim_font_courier.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"

extern void EPD_IT8951_ReadBusy(void);

// Cache för färdigvända tecken - spara beräkning under skrivning
static UBYTE pre_flipped_glyphs[128][GLYPH_SIZE_BYTES];

void redraw_buffer(char buffer[MAX_ROWS][MAX_COLS], UDOUBLE target_addr) {
    // 1. Allokera en lokal bildbuffert för hela skärmytan i RAM
    // (Alternativt kan en statisk buffer deklareras för att slippa malloc på stacken)
    static UBYTE screen_frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT]; // Anpassa storlek efter pixelformat (t.ex. 8bpp)

    // Fyll bufferten med vit bakgrund (eller nollor beroende på initiering)
    // Här utgår vi från att 0xFF är vit/bakgrund
    memset(screen_frame_buffer, 0xFF, sizeof(screen_frame_buffer));

    // 2. Bygg ihop skärmbilden lokalt i RAM via snabba memcpy
    for (int row = 0; row < MAX_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            char c = buffer[row][col];
            if (c < 0 || c > 127) c = ' ';

            const UBYTE *glyph = pre_flipped_glyphs[(int)c];

            int px = get_physical_x(col);
            int py = get_physical_y(row);

            // Kopiera rad för rad av glyphen till rätt position i den stora ram-bufferten
            for (int h = 0; h < GLYPH_H; h++) {
                // Beräkna destination i den stora skärmbufferten
                int dest_offset = (py + h) * SCREEN_WIDTH + px;
                // Kopiera 32 pixlar (bytes) för denna rad av tecknet
                memcpy(&screen_frame_buffer[dest_offset], &glyph[h * GLYPH_W], GLYPH_W);
            }
        }
    }

    // 3. Skicka hela den sammansatta bilden till IT8951 i ett enda block
    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = screen_frame_buffer;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = 0;
    area_info.Area_Y = 0;
    area_info.Area_W = SCREEN_WIDTH;
    area_info.Area_H = SCREEN_HEIGHT;

    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // Skicka hela skärmblocket på en gång
    fast_spi_write_nbyte(screen_frame_buffer, SCREEN_WIDTH * SCREEN_HEIGHT);

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // 4. Trigga uppdatering av hela skärmen i A2-läge
    EPD_IT8951_Display_Area(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, IT8951_A2_MODE);
}

void render_char(char c, int x, int y, UDOUBLE target_addr) {
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = (UBYTE*)pre_flipped_glyphs[(int)c];
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

    fast_spi_write_nbyte(pre_flipped_glyphs[(int)c], GLYPH_SIZE_BYTES);

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
    for (int row = 0; row < JUMP_LINES; row++) {
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

    // 3. Flytta markören till startläge.
    *cursor_row = JUMP_LINES;
    *cursor_col = 0;

    // 4. Rita om hela skärmytan i A2-läge
    redraw_buffer(buffer, target_addr);
}
