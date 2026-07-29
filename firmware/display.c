#include <stdio.h>
#include <string.h>
#include "display.h"
#include "wim_fonts.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"

extern void EPD_IT8951_ReadBusy(void);

// Allokera minnet för de globala variablerna här
char view_buffer[current_max_rows][MAX_COLS];
uint8_t full_screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// I display.c
int current_max_cols = 0;
int current_current_max_rows = 0;
int point_x[ABSOLUTE_MAX_COLS];
int point_y[ABSOLUTE_MAX_ROWS];

// Cache för färdigvända tecken - spara beräkning under skrivning
static UBYTE pre_flipped_glyphs[128][2048];

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

void clear_buffer() {
    for (int i = 0; i < (ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS); i++) {
        text_buffer[i] = ' ';
    }
}

void redraw_buffer(char buffer[current_max_rows][MAX_COLS], UDOUBLE target_addr) {
    // 1. Allokera en lokal bildbuffert för hela skärmytan i RAM
    // (Alternativt kan en statisk buffer deklareras för att slippa malloc på stacken)
    static UBYTE screen_frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT]; // Anpassa storlek efter pixelformat (t.ex. 8bpp)

    // Fyll bufferten med vit bakgrund (eller nollor beroende på initiering)
    // Här utgår vi från att 0xFF är vit/bakgrund
    memset(screen_frame_buffer, 0xFF, sizeof(screen_frame_buffer));

    // 2. Bygg ihop skärmbilden lokalt i RAM via snabba memcpy
    for (int row = 0; row < current_max_rows; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            char c = BUF_AT(buffer, row, col);
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
    // Filtrera bort icke-utskrivbara tecken för att spara cykler
    if (c < 32 || c > 126) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Peka på det förberedda tecknet i den roterade RAM-cachen
    load_info.Source_Buffer_Addr = (UBYTE*)pre_flipped_glyphs[(int)c];
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    // Hämta dynamiska dimensioner från den aktiva fontstrukturen
    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = current_font.width;
    area_info.Area_H = current_font.height;

    // Konfigurera kontrollern för den specifika minnesytan
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    // Förbered SPI-överföring (preamble)
    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // Gör blocköverföringen mot Waveshares C-bibliotek med rätt bytestorlek
    fast_spi_write_nbyte(pre_flipped_glyphs[(int)c], current_font.bytes_per_char);

    // Avsluta SPI-överföringen snyggt
    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Trigga utritning av ytan i A2-läge (Mode 6)
    EPD_IT8951_Display_Area(x, y, current_font.width, current_font.height, IT8951_A2_MODE);
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

// Enkel uppslagning
int get_physical_x(int col) {
    return point_x[col];
}

int get_physical_y(int row) {
    return point_y[row];
}

void display_jump(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // 1. Flytta upp de sista kontextraderna
    size_t bytes_to_move = JUMP_LINES * current_max_cols * sizeof(char);
    int source_index = (current_max_rows - JUMP_LINES) * current_max_cols;

    memmove(&buffer[0], &buffer[source_index], bytes_to_move);

    // 2. Rensa utrymmet under med mellanslag
    size_t bytes_to_clear = (current_max_rows - JUMP_LINES) * current_max_cols * sizeof(char);
    int clear_start_index = JUMP_LINES * current_max_cols;

    memset(&buffer[clear_start_index], ' ', bytes_to_clear);

    *cursor_row = JUMP_LINES;
    *cursor_col = 0;

    stitch_and_render_screen(buffer, target_addr);
}

void word_wrap(char buffer[current_max_rows][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    if (*cursor_col < MAX_COLS) return;

    // Om raden slutar på ett mellanslag struntar vi i att rendera det på ny rad.
    // Vi bara flyttar ner markören och nollställer kolumnen.
    if (buffer[*cursor_row][MAX_COLS - 1] == ' ') {
        *cursor_col = 0;
        (*cursor_row)++;

        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
        return;
    }

    // Om raden slutar på ett bindestreck stannar det på den övre raden.
    // Vi flyttar bara ner markören utan att flytta några tecken.
    if (buffer[*cursor_row][MAX_COLS - 1] == '-') {
        *cursor_col = 0;
        (*cursor_row)++;

        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
        return;
    }

    int break_col = MAX_COLS - 1;

    // Leta bakåt efter ett naturligt brytmönster (mellanslag eller bindestreck)
    while (break_col > 0 && buffer[*cursor_row][break_col] != ' ' && buffer[*cursor_row][break_col] != '-') {
        break_col--;
    }

    if (break_col == 0) {
        // Om inget brytmärkes hittas tvingas radbrytning på sista kolumnen
        *cursor_col = 0;
        (*cursor_row)++;

        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
        return;
    }

    // Om vi bröt vid ett bindestreck ska bindestrecket stanna kvar (index break_col),
    // och vi flyttar endast det som kommer efter. Om vi bröt vid ett mellanslag
    // hoppar vi över själva mellanslagsindexet.
    int start_move = (buffer[*cursor_row][break_col] == '-') ? break_col + 1 : break_col + 1;
    int chars_to_move = MAX_COLS - start_move;

    if (chars_to_move <= 0) {
        *cursor_col = 0;
        (*cursor_row)++;
        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
        return;
    }

    char temp_word[chars_to_move];

    // 1. Minneshantering i RAM
    memcpy(temp_word, &buffer[*cursor_row][start_move], chars_to_move);
    memset(&buffer[*cursor_row][start_move], ' ', MAX_COLS - start_move);

    // 2. Skärmoperation: Radera ordets tidigare placering i ett svep i A2-läge
    int clear_x = get_physical_x(MAX_COLS - 1);
    int clear_y = get_physical_y(*cursor_row);
    int clear_width = chars_to_move * GLYPH_W;

    clear_area(clear_x, clear_y, clear_width, GLYPH_H, target_addr);

    // 3. Förbered ny rad
    (*cursor_row)++;
    *cursor_col = 0;

    if (*cursor_row >= current_max_rows) {
        display_jump(buffer, cursor_row, cursor_col, target_addr);
    }

    memcpy(&buffer[*cursor_row][0], temp_word, chars_to_move);

    // 4. Skärmoperation: Rita ut det flyttade ordet på den nya raden
    for (int i = 0; i < chars_to_move; i++) {
        int px = get_physical_x(i);
        int py = get_physical_y(*cursor_row);
        render_char(temp_word[i], px, py, target_addr);
    }

    *cursor_col = chars_to_move;
}

// Funktionen bygger font-cachen i RAM.
// Anropas inifrån set_active_font() (och indirekt vid uppstart).
void init_glyph_cache(void) {
    // 1. Rensa hela cachen först (0xFF är vitt i 8bpp)
    memset(pre_flipped_glyphs, 0xFF, sizeof(pre_flipped_glyphs));

    // 2. Hantera endast giltiga ASCII-tecken för att spara cykler
    for (int c = 32; c < 127; c++) {
        // Hämta startadressen för det enskilda tecknet i fontens rådata
        const uint8_t* source_glyph = current_font.data + (c * current_font.bytes_per_char);

        // 3. Vänd arrayen baklänges för att rotera tecknet 180 grader för skärmen
        for (int i = 0; i < current_font.bytes_per_char; i++) {
            pre_flipped_glyphs[c][i] = source_glyph[current_font.bytes_per_char - 1 - i];
        }
    }
}

void render_status_bar(const char *text, UDOUBLE target_addr) {
    int start_y = SCREEN_HEIGHT - MARGIN_BOTTOM; // 1072 - 68 = 1004

    // 1. Rensa ytan i botten av skärmen (från x=0 till SCREEN_WIDTH)
    clear_area(0, start_y, SCREEN_WIDTH, MARGIN_BOTTOM, target_addr);

    // 2. Skriv ut texten, med 4 px offset nedåt för att ge plats åt skiljelinjen[cite: 2]
    int text_y = start_y + 4;

    // Börja rita från den absoluta vänsterkanten
    int current_x = 0;

    // Låt texten löpa hela vägen till SCREEN_WIDTH (1448 px)[cite: 2]
    for (int i = 0; text[i] != '\0' && current_x < SCREEN_WIDTH; i++) {
        render_char(text[i], current_x, text_y, target_addr);
        current_x += GLYPH_W;
    }
}

void stitch_and_render_screen(char buffer[current_max_rows][MAX_COLS], UDOUBLE target_addr) {
    // 1. Rensa den stora bildbufferten med vitt (0xFF för 8bpp)
    memset(full_screen_buffer, 0xFF, sizeof(full_screen_buffer));

    // 2. Iterera över den logiska skärmbufferten och pussla in tecknen i RAM
    for (int row = 0; row < current_max_rows; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            char c = BUF_AT(buffer, row, col);
            if (c == ' ' || c == '\0') continue;

            // Använd dina nya funktioner för att fastställa de exakta, spegelvända koordinaterna
            int start_x = get_physical_x(col);
            int start_y = get_physical_y(row);

            const UBYTE *glyph_bitmap = pre_flipped_glyphs[(int)c];

            for (int h = 0; h < GLYPH_H; h++) {
                int buffer_offset = ((start_y + h) * SCREEN_WIDTH) + start_x;
                memcpy(&full_screen_buffer[buffer_offset],
                        &glyph_bitmap[h * GLYPH_W],
                        GLYPH_W);
            }
        }
    }

    // 3. Konfigurera bildöverföring till IT8951
    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = full_screen_buffer;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = 0;
    area_info.Area_Y = 0;
    area_info.Area_W = SCREEN_WIDTH;
    area_info.Area_H = SCREEN_HEIGHT;

    // 4. Tala om för kontrollern vart datan ska och öppna kommunikationen
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // 5. Blocköverföring via fast_spi
    fast_spi_write_nbyte(full_screen_buffer, SCREEN_WIDTH * SCREEN_HEIGHT);

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // 6. Trigga uppdatering av hela ytan (A2 mode är 6 för denna HAT)[cite: 1]
    EPD_IT8951_Display_Area(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, IT8951_A2_MODE);
}

// Funktion för att byta font dynamiskt via F6 eller vid uppstart
void set_active_font(int font_choice) {
    if (font_choice == 1) {
        current_font.data = (const uint8_t*)font_16x28;
        current_font.width = FONT_16X28_W;
        current_font.height = FONT_16X28_H;
        current_font.bytes_per_char = FONT_16X28_BYTES;
    } else {
        current_font.data = (const uint8_t*)font_24x41;
        current_font.width = FONT_24X41_W;
        current_font.height = FONT_24X41_H;
        current_font.bytes_per_char = FONT_24X41_BYTES;
    }

    // Så fort fonten ändras, måste vi ladda om den spegelvända cachen
    init_glyph_cache();
}

// Körs en gång när en font laddas in via F6 eller uppstart
void calculate_layout_points(int font_w, int font_h) {
    current_max_cols = (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT) / font_w;
    current_max_rows = (SCREEN_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM) / font_h;

    for (int col = 0; col < current_max_cols; col++) {
        point_x[col] = SCREEN_WIDTH - MARGIN_LEFT - ((col + 1) * font_w);
    }
    for (int row = 0; row < current_max_rows; row++) {
        point_y[row] = SCREEN_HEIGHT - MARGIN_TOP - ((row + 1) * font_h);
    }
}
