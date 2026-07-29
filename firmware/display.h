#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "../lib/Config/DEV_Config.h"
#include "wim_fonts.h"

// ---------------------------------------------------------
// Hårdvaruspecifikationer för IT8951 och panelen
// ---------------------------------------------------------
#define SCREEN_WIDTH 1448
#define SCREEN_HEIGHT 1072
#define FULL_SCREEN_BUFFER_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 8)
#define IT8951_A2_MODE 6

// ---------------------------------------------------------
// Skrivytans layout (Statiska marginaler)
// ---------------------------------------------------------
#define MARGIN_LEFT 68
#define MARGIN_RIGHT 68
#define MARGIN_TOP 44
#define MARGIN_BOTTOM 68

// Parameter för hur många rader som bevaras vid Jump
#define JUMP_LINES 2

// Tillräckligt stort för att hantera en väldigt liten teckenstorlek
#define ABSOLUTE_MAX_COLS 120
#define ABSOLUTE_MAX_ROWS 60
// ---------------------------------------------------------
// Font- och Buffertkonfiguration
// ---------------------------------------------------------
typedef struct {
    const uint8_t* data;
    uint8_t width;
    uint8_t height;
    uint16_t bytes_per_char;
} ActiveFont;

extern ActiveFont current_font;

// Dynamiska variabler (räknas ut och sätts i display.c vid fontbyte)
// Våra förberedda punkter (Lookup Tables)
extern int point_x[ABSOLUTE_MAX_COLS];
extern int point_y[ABSOLUTE_MAX_ROWS];

// Variabler för gränser
extern int current_max_cols;
extern int current_max_rows;


// Den statiska bildbufferten i RAM för helskärmsuppdateringar
extern uint8_t full_screen_buffer[];

// ---------------------------------------------------------
// Initiering och grundläggande skärmstyrning
// ---------------------------------------------------------
void init_display(UDOUBLE *target_addr);
void cleanup_display(void);
void refresh_display_full(void); // Kör INIT (Mode 0) via F5

// ---------------------------------------------------------
// Renderingsfunktioner (SPI-överföring till IT8951)
// ---------------------------------------------------------
void set_active_font(int font_choice);
void render_char(char c, int physical_x, int physical_y, UDOUBLE target_addr);
void clear_area(int physical_x, int physical_y, int width, int height, UDOUBLE target_addr);
void render_status_bar(const char *text, UDOUBLE target_addr);

// ---------------------------------------------------------
// Hjälpfunktioner för layout (Hanterar virtuell padding)
// ---------------------------------------------------------
int get_physical_x(int col);
int get_physical_y(int row);

// ---------------------------------------------------------
// Buffert- och textflödeshantering
// ---------------------------------------------------------
void redraw_buffer(char *buffer, UDOUBLE target_addr);
void word_wrap(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr);
void display_jump(char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr);
void stitch_and_render_screen(char *buffer, UDOUBLE target_addr);

void calculate_layout_points(int font_w, int font_h);

// Makro för att räkna ut rätt index i RAM
#define BUF_AT(buf, r, c) buf[((r) * current_max_cols) + (c)]

#endif // DISPLAY_H
