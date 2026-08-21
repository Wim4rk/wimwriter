#include <stdio.h>
#include <string.h>
#include <time.h>
#include "display.h"
#include "wim_fonts.h"
#include "DEV_Config.h"
#include "fast_spi.h"
#include "EPD_IT8951.h"
#include "../software/editor.h"
#include "sync.h"

extern void EPD_IT8951_ReadBusy(void);

char view_buffer[ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS];
void init_glyph_cache(void);
uint8_t full_screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// Cache för färdigvända tecken - spara beräkning under skrivning
UBYTE pre_flipped_glyphs[256][2048];

// Ny funktion: Skriver enbart data till IT8951:s interna minne via SPI (Tyst överföring)
void send_buffer_to_ram(UBYTE *buffer, int x, int y, int w, int h, UDOUBLE target_addr) {
    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = buffer;
    // load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Endian_Type = IT8951_LDIMG_B_ENDIAN;
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

    // IT8951_A2_MODE

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
        memset(white_buffer, 0xF0, sizeof(white_buffer)); // 0xF0 är vitt i 8bpp
        buffer_initialized = true;
    }

    send_and_display_buffer(white_buffer, x, y, width, height, target_addr, IT8951_A2_MODE);
}

void render_char(char c, int x, int y, UDOUBLE target_addr) {
    // Släpp igenom svenska tecken, men blockera styrtecken under 32
    if ((unsigned char)c < 32) return;

    // Den här raden försvann och måste tillbaka!
    send_and_display_buffer(pre_flipped_glyphs[(unsigned char)c], x, y, FONT_W, FONT_H, target_addr, IT8951_A2_MODE);
}

// Funktionen bygger font-cachen i RAM.
// Anropas inifrån set_active_font() (och indirekt vid uppstart).
void init_glyph_cache(void) {
    // 1. Rensa hela cachen. 0xF0 motsvarar vitt i 8bpp.
    memset(pre_flipped_glyphs, 0xF0, sizeof(pre_flipped_glyphs));

    // 2. Bygg cachen för standard ASCII och Latin-1 (för å, ä, ö)[cite: 2].
    for (int c = 32; c < 256; c++) {
        const uint8_t* source_glyph = (const uint8_t*)wim_font_24x43 + (c * FONT_24X43_BYTES);

        for (int i = 0; i < FONT_24X43_BYTES; i++) {
            // Hämta pixelvärdet (roterat för skärmens fysiska orientering)
            uint8_t pixel_val = source_glyph[FONT_24X43_BYTES - 1 - i];

            // Tröskla värdet: Mörkare än 128 blir rent svart, resten vitt.
            // Detta åtgärdar problemet med ihåliga tecken[cite: 3].
            pre_flipped_glyphs[c][i] = (pixel_val < 128) ? 0x00 : 0xF0;
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
    // Vänder på X eftersom fysiskt origo skiljer sig från virtuellt
    return SCREEN_WIDTH - MARGIN_LEFT - ((col + 1) * FONT_W);
}

int get_physical_y(int row) {
    // Vänder på Y (Justerar för statiskt radavstånd)
    return SCREEN_HEIGHT - MARGIN_TOP - ((row + 1) * ROW_HEIGHT) + LINE_SPACING_PX;
}

void display_jump(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // 1. Antalet rader vi ska ha kvar som kontext högst upp[cite: 2]
    int keep_rows = JUMP_LINES;

    // 2. Beräkna exakt hur många rader hela textblocket måste skiftas uppåt
    // för att markören (den aktiva raden) ska hamna på index 'keep_rows'.
    int shift_up = *cursor_row - keep_rows;

    if (shift_up <= 0) {
        return; // Inget hopp behövs, vi är inte i botten än
    }

    // 3. Antal rader som måste följa med i hoppet.
    // (+1 krävs för att plocka med själva markörraden, eftersom den
    // dolda raden kan innehålla det överflyttade ordet från word_wrap).
    int rows_to_copy = keep_rows + 1;

    int chars_to_copy = rows_to_copy * MAX_COLS;
    int source_offset = shift_up * MAX_COLS;

    // 4. Skifta upp de bevarade raderna (inklusive det nedbrutna ordet) till toppen
    memmove(buffer, buffer + source_offset, chars_to_copy);

    // 5. Rensa all text nedanför (inklusive den dolda raden som orsakade hoppet)
    int total_buffer_size = (MAX_ROWS + 2) * MAX_COLS;
    memset(buffer + chars_to_copy, ' ', total_buffer_size - chars_to_copy);

    // 6. Sätt markören på den nya raden, precis under den bevarade texten[cite: 2]
    // (Vi rör inte *cursor_col, eftersom det just nu brutna ordet står där)
    *cursor_row = keep_rows;

    // 7. Rita upp den nya skärmen. Eftersom vi använder A2-läge för allt annat
    // gör vi hela skärmuppdateringen asynkront och blixtsnabbt.
    stitch_and_render_screen(buffer, target_addr);
}


// Ny hjälpfunktion: Renderar en eller flera hela rader i ett enda anrop
void render_rows_stitched(int start_row, int end_row, char *buffer, UDOUBLE target_addr) {
    if (start_row < 0) start_row = 0;
    if (end_row >= MAX_ROWS) end_row = MAX_ROWS - 1;
    if (start_row > end_row) return;

    int num_rows = end_row - start_row + 1;
    int physical_w = SCREEN_WIDTH;
    int physical_h = num_rows * (FONT_H + LINE_SPACING_PX);
    int physical_y_start = SCREEN_HEIGHT - MARGIN_TOP - ((end_row + 1) * (FONT_H + LINE_SPACING_PX)) + LINE_SPACING_PX;

    // Bounding box för raderna (dimensionerad för max 2 rader med 64px font = 1448 x 128 px i RAM)
    // Bounding box för raderna (dimensionerad för max höjd inklusive extremt radavstånd = 1448 x 384 px i RAM)
    static UBYTE row_buffer[1448 * 384];
    memset(row_buffer, 0xF0, physical_w * physical_h); // Fyll hela ytan med vitt

    // Rita in alla faktiska tecken från textbufferten in i denna nya vita låda
    for (int r = start_row; r <= end_row; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            char ch = BUF_AT(buffer, r, c);
            if (ch >= 32 && ch <= 256 && ch != ' ') {
                const UBYTE *glyph = pre_flipped_glyphs[(unsigned char)ch];
                int char_px = get_physical_x(c);
                int char_py_abs = get_physical_y(r);
                int rel_y = char_py_abs - physical_y_start; // Relativ höjd inuti row_buffer
                // Kopiera in glyfen i lokala bufferten
                for (int h = 0; h < FONT_H; h++) {
                    memcpy(&row_buffer[(rel_y + h) * physical_w + char_px],
                           &glyph[h * FONT_W],
                           FONT_W);
                }
            }
        }
    }

    // ETT enda massivt SPI-anrop för att radera det gamla ordet och rita dit det nya
    send_and_display_buffer(row_buffer, 0, physical_y_start, physical_w, physical_h, target_addr, IT8951_A2_MODE);
}

void word_wrap(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    int old_row = *cursor_row;
    int row_start = old_row * MAX_COLS;
    int break_col = *cursor_col - 1;

    // Leta bakåt efter senaste mellanslaget eller bindestrecket
    while (break_col > 0 && buffer[row_start + break_col] != ' ' && buffer[row_start + break_col] != '-') {
        break_col--;
    }

    if (break_col > 0) {
        int word_len = *cursor_col - break_col - 1;

        // 1. Plocka ut ordet till RAM och städa i textmodellen
        char temp_str[word_len + 1];
        for (int i = 0; i < word_len; i++) {
            temp_str[i] = buffer[row_start + break_col + 1 + i];
            buffer[row_start + break_col + 1 + i] = ' ';
        }

        // 2. Uppdatera markören till ny rad och sätt in ordet logiskt i textmodellen
        (*cursor_row)++;
        *cursor_col = word_len;

        if (*cursor_row >= MAX_ROWS) {
            // FIX: Klistra in ordet i RAM på den "dolda" raden innan vi hoppar
            int new_row_start = (*cursor_row) * MAX_COLS;
            for (int i = 0; i < word_len; i++) {
                buffer[new_row_start + i] = temp_str[i];
            }

            // Låt hoppet sköta all utritning, nu följer ordet med!
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        } else {
            // (Behåll din befintliga else-logik här)
            int new_row_start = (*cursor_row) * MAX_COLS;
            for (int i = 0; i < word_len; i++) {
                buffer[new_row_start + i] = temp_str[i];
            }
            render_rows_stitched(old_row, *cursor_row, buffer, target_addr);
        }

    } else {
        // Brutal radbrytning om inget mellanslag existerar
        (*cursor_row)++;
        *cursor_col = 0;
        if (*cursor_row >= MAX_ROWS) {
            display_jump(buffer, cursor_row, cursor_col, target_addr);
        }
    }
}

void render_stitched_text(const char *text, int physical_x, int physical_y, UDOUBLE target_addr) {
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }

    if (len == 0) return;

    int text_pixel_width = len * FONT_W;

    // Eftersom physical_x är startpunkten för det FÖRSTA tecknet (visuellt längst till vänster,
    // vilket är fysiskt längst till höger pga rotation), är lådans lägsta fysiska X-koordinat:
    int box_physical_x = physical_x - text_pixel_width + FONT_W;

    // Statisk RAM-buffert för att skona ARMv6
    static UBYTE stitch_buffer[1448 * 64];
    memset(stitch_buffer, 0xF0, text_pixel_width * FONT_H);

    // Det första tecknet placeras högst upp i lokala X-koordinater inuti bufferten
    int current_local_x = text_pixel_width - FONT_W;

    for (int i = 0; i < len; i++) {
        unsigned char uc = (unsigned char)text[i];
        if (uc < 32) uc = ' '; // Rensa bara bort styrtecken

        const UBYTE *glyph = pre_flipped_glyphs[uc];
        for (int h = 0; h < FONT_H; h++) {
            memcpy(&stitch_buffer[h * text_pixel_width + current_local_x],
                   &glyph[h * FONT_W],
                   FONT_W);
        }
        current_local_x -= FONT_W; // Stega fysiskt åt vänster för nästa tecken
    }

    send_and_display_buffer(stitch_buffer, box_physical_x, physical_y, text_pixel_width, FONT_H, target_addr, IT8951_A2_MODE);
}

void render_status_bar(const char *text, UDOUBLE target_addr) {
    int physical_w = SCREEN_WIDTH;
    int physical_h = MARGIN_BOTTOM; // 68 pixlar enligt din layout
    int physical_y = 0; // Fysiskt högst upp på skärmen

    // Statisk buffert för hela statusraden (1448 x 68 pixlar)
    // Undviker malloc och skonar processorn
    static UBYTE status_buffer[SCREEN_WIDTH * MARGIN_BOTTOM];

    // Fyll hela ytan med vitt. Detta ersätter behovet av clear_area()
    memset(status_buffer, 0xF0, sizeof(status_buffer));

    // Rita ett 2 pixlar tjockt horisontellt streck.
    // Fysiskt y=66 och y=67 hamnar visuellt som ett streck precis ovanför texten.
    //
    // int line_y = MARGIN_BOTTOM - 2;
    // for (int x = 0; x < SCREEN_WIDTH; x++) {
    //     status_buffer[line_y * SCREEN_WIDTH + x] = 0x00;
    //     status_buffer[(line_y + 1) * SCREEN_WIDTH + x] = 0x00;
    // }
    is_wifi_active = get_actual_wifi_status();

    if (is_wifi_active) {
        const char *wifi_text = "WiFi";
        // Eftersom skärmen är fysiskt roterad 180 grader, är den visuella högerkanten
        // den fysiska vänsterkanten. X-koordinaten måste justeras för detta.
        int wifi_physical_x = FONT_W * strlen(wifi_text); // Fysisk X börjar nära 0

        for (int i = 0; wifi_text[i] != '\0' && wifi_physical_x >= 0; i++) {
            unsigned char uc = (unsigned char)wifi_text[i];
            const UBYTE *glyph = pre_flipped_glyphs[uc];

            for (int h = 0; h < FONT_H; h++) {
                memcpy(&status_buffer[h * physical_w + wifi_physical_x],
                        &glyph[h * FONT_W],
                        FONT_W);
            }
            wifi_physical_x -= FONT_W; // Stega fysiskt vänster (visuellt höger)
        }
    }

    // Fysisk X-koordinat börjar i högerkant (din visuella vänsterkant)
    int physical_x = SCREEN_WIDTH - FONT_W;

    for (int i = 0; text[i] != '\0' && physical_x >= 0; i++) {
        unsigned char uc = (unsigned char)text[i];

        // Fånga upp starten på ett svenskt UTF-8-tecken
        if (uc == 0xC3 && text[i+1] != '\0') {
            unsigned char next_ch = (unsigned char)text[i+1];
            if (next_ch == 0xA5) uc = 0xE5;      // å
            else if (next_ch == 0xA4) uc = 0xE4; // ä
            else if (next_ch == 0xB6) uc = 0xF6; // ö
            else if (next_ch == 0x85) uc = 0xC5; // Å
            else if (next_ch == 0x84) uc = 0xC4; // Ä
            else if (next_ch == 0x96) uc = 0xD6; // Ö

            i++; // Hoppa manuellt över nästa byte i textsträngen
        }

        // Hämta endast tecken vi kan skriva ut
        if (uc >= 32) {
            const UBYTE *glyph = pre_flipped_glyphs[uc];

            // Kopiera in den roterade glyfen i vår statusbuffert
            for (int h = 0; h < FONT_H; h++) {
                memcpy(&status_buffer[h * physical_w + physical_x],
                        &glyph[h * FONT_W],
                        FONT_W);
            }
        }

        // Stega fysiskt åt vänster för nästa tecken
        physical_x -= FONT_W;
    }

    // Ett enda massivt SPI-anrop till IT8951 för hela statusraden
    status_bar_visible = true;
    status_bar_timestamp = time(NULL);

    send_and_display_buffer(status_buffer, 0, physical_y, physical_w, physical_h, target_addr, IT8951_A2_MODE);
}

void stitch_and_render_screen(char *buffer, UDOUBLE target_addr) {
    memset(full_screen_buffer, 0xF0, sizeof(full_screen_buffer));

    // Iterera över hela bufferten linjärt
    for (int i = 0; i < MAX_ROWS * MAX_COLS; i++) {
        char c = buffer[i];
        if (c == ' ' || c == '\0') continue;

        int row = i / MAX_COLS;
        int col = i % MAX_COLS;

        // Beräkna inverterade koordinater för fysisk rotation direkt
        int px = get_physical_x(col);
        int py = get_physical_y(row);

        const UBYTE *glyph = pre_flipped_glyphs[(unsigned char)c];

        // Blockkopiera glyphen till skärmbufferten
        for (int h = 0; h < FONT_H; h++) {
            memcpy(&full_screen_buffer[(py + h) * SCREEN_WIDTH + px],
                   &glyph[h * FONT_W],
                   FONT_W);
        }
    }

    send_and_display_buffer(full_screen_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, target_addr, 2);
}


void hide_status_bar_and_redraw(UDOUBLE target_addr) {
    if (!status_bar_visible) return;

    // Skärmen är roterad; den visuella underkanten ligger på fysisk y = 0
    int start_y = 0;

    if (is_wifi_active) {
        // Skriv över med tomt textfält om WiFi är igång
        render_status_bar("", target_addr);
    } else {
        // Rensa rätt fysiska yta i kontrollerns RAM
        clear_area(0, start_y, SCREEN_WIDTH, MARGIN_BOTTOM, target_addr);
        status_bar_visible = false;
    }
}

void refresh_display_full(char *buffer, UDOUBLE target_addr) {
    // 1. Fyll skärmbufferten med vitt (0xF0)
    memset(full_screen_buffer, 0xF0, sizeof(full_screen_buffer));

    // 2. Skicka vit skärm i INIT-läge (Mode 0) för att rensa all ghosting.
    // Detta får skärmen att blinka till ordentligt och nollställa pigmenten.
    // send_and_display_buffer(full_screen_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, target_addr, 0);

    // 3. Bygg upp skärmens faktiska innehåll i RAM
    for (int i = 0; i < MAX_ROWS * MAX_COLS; i++) {
        char c = buffer[i];
        if (c == ' ' || c == '\0') continue;

        int row = i / MAX_COLS;
        int col = i % MAX_COLS;

        // Använd dina befintliga hjälpfunktioner för fysisk rotation
        int px = get_physical_x(col);
        int py = get_physical_y(row);

        const UBYTE *glyph = pre_flipped_glyphs[(unsigned char)c];

        for (int h = 0; h < FONT_H; h++) {
            memcpy(&full_screen_buffer[(py + h) * SCREEN_WIDTH + px],
                   &glyph[h * FONT_W],
                   FONT_W);
        }
    }

    // Mode 0 är INIT-läge, 2 är GC16.
    send_and_display_buffer(full_screen_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, target_addr, 2);
}
