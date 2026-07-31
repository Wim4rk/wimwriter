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

LineSpacing current_spacing_mode = SPACING_ONE_AND_HALF;

// Beräknar hur många extra pixlar som ska läggas till per rad
int get_line_spacing_px(void) {
    switch (current_spacing_mode) {
        case SPACING_ONE_AND_HALF: return current_font.height / 2;
        case SPACING_DOUBLE: return current_font.height;
        case SPACING_SINGLE:
        default: return 0;
    }
}

// Gör det möjligt att byta radavstånd "on the fly" (t.ex. via en framtida funktionstangent)
void set_line_spacing(LineSpacing mode) {
    current_spacing_mode = mode;
    calculate_layout_points(current_font.width, current_font.height);
}

// Ny funktion: Skriver enbart data till IT8951:s interna minne via SPI (Tyst överföring)
void send_buffer_to_ram(UBYTE *buffer, int x, int y, int w, int h, UDOUBLE target_addr) {
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
}

// Din befintliga funktion (omskriven).
// Andra delar av koden som anropar denna kommer att fungera precis som förut.
void send_and_display_buffer(UBYTE *buffer, int x, int y, int w, int h, UDOUBLE target_addr, int update_mode) {
    // 1. Skicka datan till minnet
    send_buffer_to_ram(buffer, x, y, w, h, target_addr);

    // 2. Trigga utritningen
    EPD_IT8951_Display_Area(x, y, w, h, update_mode);
}

void clear_area(int x, int y, int width, int height, UDOUBLE target_addr) {
    if (width <= 0 || height <= 0) return;
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
    // 1. Antalet rader vi ska bevara (från botten av den nuvarande texten)
    int keep_rows = JUMP_LINES;
    if (keep_rows > current_max_rows) {
        keep_rows = current_max_rows;
    }

    int keep_chars = keep_rows * current_max_cols;
    int total_chars = current_max_rows * current_max_cols;

    // 2. Räkna ut varifrån i bufferten vi ska kopiera de sista raderna
    int source_offset = (current_max_rows - keep_rows) * current_max_cols;

    // 3. Flytta de sista raderna till toppen av bufferten (rad 0)
    memmove(buffer, buffer + source_offset, keep_chars);

    // 4. Rensa resten av skärmen från gammal text
    memset(buffer + keep_chars, ' ', total_chars - keep_chars);

    // 5. Sätt skrivprompten på den första tomma raden precis under den bevarade texten
    *cursor_row = keep_rows;

    // (OBS: *cursor_col nollställs inte här, eftersom word_wrap kan
    // ha placerat början på ett nytt ord där innan hoppet triggades)

    // 6. Rita upp den nya, rena skärmen
    stitch_and_render_screen(buffer, target_addr);
}


// Ny hjälpfunktion: Renderar en eller flera hela rader i ett enda anrop
void render_rows_stitched(int start_row, int end_row, char *buffer, UDOUBLE target_addr) {
    if (start_row < 0) start_row = 0;
    if (end_row >= current_max_rows) end_row = current_max_rows - 1;
    if (start_row > end_row) return;

    int num_rows = end_row - start_row + 1;
    int physical_w = SCREEN_WIDTH;
    int spacing_px = get_line_spacing_px();
    int physical_h = num_rows * (current_font.height + spacing_px);
    int physical_y_start = SCREEN_HEIGHT - MARGIN_TOP - ((end_row + 1) * (current_font.height + spacing_px)) + spacing_px;

    // Bounding box för raderna (dimensionerad för max 2 rader med 64px font = 1448 x 128 px i RAM)
    // Bounding box för raderna (dimensionerad för max höjd inklusive extremt radavstånd = 1448 x 384 px i RAM)
    static UBYTE row_buffer[1448 * 384];
    memset(row_buffer, 0xFF, physical_w * physical_h); // Fyll hela ytan med vitt

    // Rita in alla faktiska tecken från textbufferten in i denna nya vita låda
    for (int r = start_row; r <= end_row; r++) {
        for (int c = 0; c < current_max_cols; c++) {
            char ch = BUF_AT(buffer, r, c);
            if (ch >= 32 && ch <= 126 && ch != ' ') {
                const UBYTE *glyph = pre_flipped_glyphs[(int)ch];
                int char_px = SCREEN_WIDTH - MARGIN_LEFT - ((c + 1) * current_font.width);
                int char_py_abs = SCREEN_HEIGHT - MARGIN_TOP - ((r + 1) * current_font.height);
                int rel_y = char_py_abs - physical_y_start; // Relativ höjd inuti row_buffer

                // Kopiera in glyfen i lokala bufferten
                for (int h = 0; h < current_font.height; h++) {
                    memcpy(&row_buffer[(rel_y + h) * physical_w + char_px],
                           &glyph[h * current_font.width],
                           current_font.width);
                }
            }
        }
    }

    // ETT enda massivt SPI-anrop för att radera det gamla ordet och rita dit det nya
    send_and_display_buffer(row_buffer, 0, physical_y_start, physical_w, physical_h, target_addr, IT8951_A2_MODE);
}

void word_wrap(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    int old_row = *cursor_row;
    int row_start = old_row * current_max_cols;
    int break_col = *cursor_col - 1;

    // Leta bakåt efter senaste mellanslaget
    while (break_col > 0 && buffer[row_start + break_col] != ' ') {
        break_col--;
    }

    if (break_col > 0) {
        int word_len = *cursor_col - break_col - 1;

        // 1. Plocka ut ordet till RAM och städa (skriv över med ' ') i textmodellen
        char temp_str[word_len + 1];
        for (int i = 0; i < word_len; i++) {
            temp_str[i] = buffer[row_start + break_col + 1 + i];
            buffer[row_start + break_col + 1 + i] = ' ';
        }

        // 2. Uppdatera markören till ny rad och sätt in ordet logiskt i textmodellen
        (*cursor_row)++;
        *cursor_col = word_len;

        if (*cursor_row >= current_max_rows) {
            // FIX: Klistra in ordet i RAM på den "dolda" raden innan vi hoppar
            int new_row_start = (*cursor_row) * current_max_cols;
            for (int i = 0; i < word_len; i++) {
                buffer[new_row_start + i] = temp_str[i];
            }

            // Låt hoppet sköta all utritning, nu följer ordet med!
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        } else {
            // (Behåll din befintliga else-logik här)
            int new_row_start = (*cursor_row) * current_max_cols;
            for (int i = 0; i < word_len; i++) {
                buffer[new_row_start + i] = temp_str[i];
            }
            render_rows_stitched(old_row, *cursor_row, buffer, target_addr);
        }

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
    int physical_w = SCREEN_WIDTH;
    int physical_h = MARGIN_BOTTOM; // 68 pixlar enligt din layout
    int physical_y = 0; // Fysiskt högst upp på skärmen

    // Statisk buffert för hela statusraden (1448 x 68 pixlar)
    // Undviker malloc och skonar processorn
    static UBYTE status_buffer[SCREEN_WIDTH * MARGIN_BOTTOM];

    // Fyll hela ytan med vitt. Detta ersätter behovet av clear_area()
    memset(status_buffer, 0xFF, sizeof(status_buffer));

    // Fysisk X-koordinat börjar i högerkant (din visuella vänsterkant)
    int physical_x = SCREEN_WIDTH - current_font.width;

    for (int i = 0; text[i] != '\0' && physical_x >= 0; i++) {
        char c = text[i];

        // Hämta endast tecken vi kan skriva ut
        if (c >= 32 && c <= 126) {
            const UBYTE *glyph = pre_flipped_glyphs[(int)c];

            // Kopiera in den roterade glyfen i vår statusbuffert
            for (int h = 0; h < current_font.height; h++) {
                memcpy(&status_buffer[h * physical_w + physical_x],
                       &glyph[h * current_font.width],
                       current_font.width);
            }
        }

        // Stega fysiskt åt vänster för nästa tecken
        physical_x -= current_font.width;
    }

    // Ett enda massivt SPI-anrop till IT8951 för hela statusraden
    send_and_display_buffer(status_buffer, 0, physical_y, physical_w, physical_h, target_addr, IT8951_A2_MODE);
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

void calculate_layout_points(int font_w, int font_h) {
    int spacing_px = get_line_spacing_px();

    current_max_cols = (SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT) / font_w;

    // Beräkna max antal rader med radavståndet inkluderat, baserat på dina marginaler[cite: 2]
    current_max_rows = (SCREEN_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM) / (font_h + spacing_px);

    for (int col = 0; col < current_max_cols; col++) {
        point_x[col] = SCREEN_WIDTH - MARGIN_LEFT - ((col + 1) * font_w);
    }
    for (int row = 0; row < current_max_rows; row++) {
        point_y[row] = SCREEN_HEIGHT - MARGIN_TOP - ((row + 1) * (font_h + spacing_px)) + spacing_px;
    }
}
