#include <stdio.h>
#include <string.h>
#include "display.h"
#include "wim_fonts.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"

ActiveFont current_font;

extern void EPD_IT8951_ReadBusy(void);

char view_buffer[ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS];
void init_glyph_cache(void);
uint8_t full_screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

int current_max_cols = 0;
int current_max_rows = 0;
int point_x[ABSOLUTE_MAX_COLS];
int point_y[ABSOLUTE_MAX_ROWS];

// Cache för färdigvända tecken - spara beräkning under skrivning
static UBYTE pre_flipped_glyphs[128][2048];

void send_and_display_buffer(UBYTE *buffer, int x, int y, int w, int h, UDOUBLE target_addr, int update_mode) {
    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = buffer;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = w;
    area_info.Area_H = h;

    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    // Blocköverföringen
    fast_spi_write_nbyte(buffer, w * h);

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Trigga utritningen i valt läge (t.ex. A2_MODE)
    EPD_IT8951_Display_Area(x, y, w, h, update_mode);
}

void clear_area(int x, int y, int width, int height, UDOUBLE target_addr) {
    if (width <= 0 || height <= 0) return;

    int size = width * height;

    // Statisk buffert för att undvika malloc i skrivloopen.
    // Stor nog för att radera mer än en hel textrad (1448 * 64 px).
    static UBYTE white_buffer[98500];
    static bool buffer_initialized = false;

    if (!buffer_initialized) {
        memset(white_buffer, 0xFF, sizeof(white_buffer)); // 0xFF är vitt i 8bpp
        buffer_initialized = true;
    }

    send_and_display_buffer(white_buffer, x, y, width, height, target_addr, IT8951_A2_MODE);
}

void render_char(char c, int x, int y, UDOUBLE target_addr) {
    // Filtrera bort icke-utskrivbara tecken för att spara cykler
    if (c < 32 || c > 126) return;
    send_and_display_buffer(pre_flipped_glyphs[(int)c], x, y, current_font.width, current_font.height, target_addr, IT8951_A2_MODE);
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
    int jump_offset = JUMP_LINES * current_max_cols;
    int total_chars = current_max_rows * current_max_cols;

    // 1. Flytta hela textblocket uppåt i RAM i en enda operation
    memmove(buffer, buffer + jump_offset, total_chars - jump_offset);

    // 2. Töm de nedersta raderna som nu är lediga
    memset(buffer + (total_chars - jump_offset), ' ', jump_offset);

    // 3. Justera endast radpekaren (kolumnen bibehålls)
    *cursor_row -= JUMP_LINES;

    // 4. Överför hela den uppdaterade skärmen
    stitch_and_render_screen(buffer, target_addr);
}

void word_wrap(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    int row_start = *cursor_row * current_max_cols;
    int break_col = *cursor_col - 1;

    // Leta bakåt efter senaste mellanslaget
    while (break_col > 0 && buffer[row_start + break_col] != ' ') {
        break_col--;
    }

    if (break_col > 0) {
        int word_len = *cursor_col - break_col - 1;

        // 1. Rensa fysiskt i A2-läge för ordets gamla position
        int clear_px = SCREEN_WIDTH - MARGIN_LEFT - (*cursor_col * current_font.width);
        int clear_py = SCREEN_HEIGHT - MARGIN_TOP - ((*cursor_row + 1) * current_font.height);
        clear_area(clear_px, clear_py, word_len * current_font.width, current_font.height, target_addr);

        // 2. Stega fram en rad och trigga jump om nödvändigt
        (*cursor_row)++;
        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
            row_start = *cursor_row * current_max_cols;
        }

        // 3. Flytta ordet i RAM
        memmove(&buffer[row_start], &buffer[row_start - current_max_cols + break_col + 1], word_len);
        memset(&buffer[row_start - current_max_cols + break_col + 1], ' ', word_len);

        *cursor_col = word_len;

        // 4. Rita om ordet på den nya raden
        // (Ett anrop till din funktion render_stitched_text är lämpligt här för att dumpa hela ordet direkt)

    } else {
        // Brutal radbrytning om inget mellanslag existerar
        (*cursor_row)++;
        *cursor_col = 0;
        if (*cursor_row >= current_max_rows) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
    }
}

void render_stitched_text(const char *text, int visual_x, int visual_y, UDOUBLE target_addr) {
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }

    if (len == 0) return;

    int text_pixel_width = len * current_font.width;

    // 1. Översätt visuella koordinater till fysiska (180 graders rotation)
    int physical_x = SCREEN_WIDTH - visual_x - text_pixel_width;
    int physical_y = SCREEN_HEIGHT - visual_y - current_font.height;

    // 2. RAM-buffert för exakt den textsträng som ska ritas.
    // Deklareras statiskt för att undvika minnesallokering på stacken (skonar ARMv6).
    // Dimensionerad för en full rad (1448 px) med högst 64 px typsnittshöjd.
    static UBYTE stitch_buffer[1448 * 64];

    // Rensa endast den del av bufferten vi faktiskt kommer att använda
    memset(stitch_buffer, 0xFF, text_pixel_width * current_font.height);

    // 3. Pussla in glyferna baklänges i bufferten för att motverka rotationen
    int current_x = text_pixel_width - current_font.width;

    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c < 32 || c > 126) c = ' ';

        const UBYTE *glyph = pre_flipped_glyphs[(int)c];

        for (int h = 0; h < current_font.height; h++) {
            int dest_offset = (h * text_pixel_width) + current_x;
            memcpy(&stitch_buffer[dest_offset],
                   &glyph[h * current_font.width],
                   current_font.width);
        }
        current_x -= current_font.width;
    }

    send_and_display_buffer(stitch_buffer, physical_x, physical_y, text_pixel_width, current_font.height, target_addr, IT8951_A2_MODE);
}

void render_status_bar(const char *text, UDOUBLE target_addr) {
    // 1. Rensa ytan. Visuell botten är fysisk topp.
    // Vi raderar från x=0, y=0 och fyller ut hela bredden samt höjden på 68 px[cite: 1].
    clear_area(0, 0, SCREEN_WIDTH, MARGIN_BOTTOM, target_addr);

    // 2. Fysisk Y-koordinat för texten.
    // Eftersom texten är 64 px hög och ytan 68 px[cite: 1], placeras texten
    // fysiskt kloss an mot Y = 0. De resterande 4 pixlarna bildar automatiskt
    // en tom marginal uppåt (vilket blir din skiljelinje nedåt visuellt).
    int physical_y = 0;

    // 3. Fysisk X-koordinat börjar i högerkant (din visuella vänsterkant).
    // Vi backar en teckenbredd för att få plats med det allra första tecknet.
    int physical_x = SCREEN_WIDTH - current_font.width;

    // Låt texten löpa hela vägen tills strängen är slut eller skärmkanten nås.
    for (int i = 0; text[i] != '\0' && physical_x >= 0; i++) {
        render_char(text[i], physical_x, physical_y, target_addr);

        // Stega fysiskt åt vänster (vilket bygger texten visuellt åt höger)
        physical_x -= current_font.width;
    }
}

void stitch_and_render_screen(char *buffer, UDOUBLE target_addr) {
    memset(full_screen_buffer, 0xFF, sizeof(full_screen_buffer));

    // Iterera över hela bufferten linjärt
    for (int i = 0; i < current_max_rows * current_max_cols; i++) {
        char c = buffer[i];
        if (c == ' ' || c == '\0') continue;

        int row = i / current_max_cols;
        int col = i % current_max_cols;

        // Beräkna inverterade koordinater för fysisk rotation direkt
        int px = SCREEN_WIDTH - MARGIN_LEFT - ((col + 1) * current_font.width);
        int py = SCREEN_HEIGHT - MARGIN_TOP - ((row + 1) * current_font.height);

        const UBYTE *glyph = pre_flipped_glyphs[(int)c];

        // Blockkopiera glyphen till skärmbufferten
        for (int h = 0; h < current_font.height; h++) {
            memcpy(&full_screen_buffer[(py + h) * SCREEN_WIDTH + px],
                   &glyph[h * current_font.width],
                   current_font.width);
        }
    }

    send_and_display_buffer(full_screen_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, target_addr, IT8951_A2_MODE);
}

// Funktion för att byta font dynamiskt via F6 eller vid uppstart
void set_active_font(int font_choice) {
    if (font_choice == 1) {
        current_font.data = (const uint8_t*)wim_font_16x28;
        current_font.width = FONT_16X28_W;
        current_font.height = FONT_16X28_H;
        current_font.bytes_per_char = FONT_16X28_BYTES;
    } else {
        current_font.data = (const uint8_t*)wim_font_24x41;
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
