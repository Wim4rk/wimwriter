#include <stdio.h>
#include <string.h>
#include "display.h"
#include "wim_font_courier.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"

extern void EPD_IT8951_ReadBusy(void);

// Cache för färdigvända tecken - spara beräkning under skrivning
static UBYTE pre_flipped_glyphs[128][GLYPH_SIZE_BYTES];

void clear_area(int x, int y, int width, int height, UDOUBLE target_addr) {
    if (width <= 0 || height <= 0) return;

    int size = width * height;

    // Statisk buffert för att undvika malloc i skrivloopen.
    // Stor nog för att radera mer än en hel textrad (1448 * 64 px).
    static UBYTE white_buffer[92672];
    static bool buffer_initialized = false;

    if (!buffer_initialized) {
        memset(white_buffer, 0xFF, sizeof(white_buffer)); // 0xFF är vitt i 8bpp
        buffer_initialized = true;
    }

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = white_buffer;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = width;
    area_info.Area_H = height;

    // Skicka "damage box" till IT8951
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // Blocköverföring av vita pixlar
    fast_spi_write_nbyte(white_buffer, size);

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Tvinga uppdatering i A2-läge för att rensa ytan direkt
    EPD_IT8951_Display_Area(x, y, width, height, IT8951_A2_MODE);
}

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

void init_display(UDOUBLE *target_addr) {
    if (DEV_Module_Init() != 0) {
        return;
    }
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(2140);
    *target_addr = ((UDOUBLE)dev_info.Memory_Addr_H << 16) | dev_info.Memory_Addr_L;

    // Bygg upp fonten i RAM innan skärmen används
    init_glyph_cache();

    EPD_IT8951_Clear_Refresh(dev_info, *target_addr, GC16_Mode);
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

void display_jump(char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // 1. Minneshantering på Raspberry Pi
    size_t bytes_to_move = JUMP_LINES * MAX_COLS * sizeof(char);
    memmove(&buffer[0][0], &buffer[MAX_ROWS - JUMP_LINES][0], bytes_to_move);

    size_t bytes_to_clear = (MAX_ROWS - JUMP_LINES) * MAX_COLS * sizeof(char);
    memset(&buffer[JUMP_LINES][0], ' ', bytes_to_clear);

    *cursor_row = JUMP_LINES;
    *cursor_col = 0;

    // 2. Skärmuppdatering: Radera hela ytan under JUMP_LINES i ett svep
    int clear_start_y = get_physical_y(MAX_ROWS - 1); // Bottenradens Y
    int clear_height = (MAX_ROWS - JUMP_LINES) * GLYPH_H;
    int clear_width = MAX_COLS * GLYPH_W;
    int clear_start_x = get_physical_x(MAX_COLS - 1); // Vänstermarginalen (index är omvänt)

    clear_area(clear_start_x, clear_start_y, clear_width, clear_height, target_addr);

    // 3. Rita enbart ut kontextraderna
    for (int row = 0; row < JUMP_LINES; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            if (buffer[row][col] != ' ') {
                int px = get_physical_x(col);
                int py = get_physical_y(row);
                render_char(buffer[row][col], px, py, target_addr);
            }
        }
    }
}

void word_wrap(char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    if (*cursor_col < MAX_COLS) return;

    if (buffer[*cursor_row][MAX_COLS - 1] == ' ') {
        *cursor_col = 0;
        (*cursor_row)++;
        return;
    }

    int break_col = MAX_COLS - 1;
    while (break_col > 0 && buffer[*cursor_row][break_col] != ' ') {
        break_col--;
    }

    if (break_col == 0) {
        *cursor_col = 0;
        (*cursor_row)++;
        return;
    }

    int chars_to_move = MAX_COLS - (break_col + 1);
    char temp_word[chars_to_move];

    // 1. Minneshantering i RAM
    memcpy(temp_word, &buffer[*cursor_row][break_col + 1], chars_to_move);
    memset(&buffer[*cursor_row][break_col + 1], ' ', chars_to_move);

    // 2. Skärmoperation: Radera ordets tidigare placering i ett svep
    // Eftersom get_physical_x returnerar ett lägre värde ju högre kolumnindex är,
    // utgör tecknet längst till höger (MAX_COLS - 1) raderingsytans startpunkt på X-axeln.
    int clear_x = get_physical_x(MAX_COLS - 1);
    int clear_y = get_physical_y(*cursor_row);
    int clear_width = chars_to_move * GLYPH_W;

    clear_area(clear_x, clear_y, clear_width, GLYPH_H, target_addr);

    // 3. Förbered ny rad
    (*cursor_row)++;
    *cursor_col = 0;
    memcpy(&buffer[*cursor_row][0], temp_word, chars_to_move);

    // 4. Skärmoperation: Rita ut ordet på den nya raden
    for (int i = 0; i < chars_to_move; i++) {
        int px = get_physical_x(i);
        int py = get_physical_y(*cursor_row);
        render_char(temp_word[i], px, py, target_addr);
    }

    *cursor_col = chars_to_move;
}

// Förbereder font-cachen i RAM. Anropas en gång vid uppstart.
void init_glyph_cache(void) {
    for (int c = 32; c < 127; c++) {
        // En 180-graders rotation är detsamma som att läsa arrayen baklänges
        for (int i = 0; i < GLYPH_SIZE_BYTES; i++) {
            pre_flipped_glyphs[c][i] = wim_font_32x64[c][GLYPH_SIZE_BYTES - 1 - i];
        }
    }
}
