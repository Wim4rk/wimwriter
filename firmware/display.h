#ifndef DISPLAY_H
#define DISPLAY_H

#include "../lib/Config/DEV_Config.h"

// Dimensioner och marginaler
#define SCREEN_WIDTH 1448
#define SCREEN_HEIGHT 1072
#define MARGIN_LEFT 68
#define MARGIN_RIGHT 68
#define MARGIN_TOP 44
#define MARGIN_BOTTOM 68

// #define MAX_COLS 41
// #define MAX_ROWS 15

#define MAX_COLS 82
#define MAX_ROWS 30

// #define GLYPH_W 32
// #define GLYPH_H 64

#define GLYPH_W 16
#define GLYPH_H 32
#define GLYPH_SIZE_BYTES 512

#define IT8951_A2_MODE 6

// Parameter för hur många rader som bevaras vid Jump
#define JUMP_LINES 4

#define FULL_SCREEN_BUFFER_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 8)

void init_display(UDOUBLE *target_addr);
void render_char(char c, int x, int y, UDOUBLE target_addr);
void cleanup_display(void);
void clear_area(int x, int y, int width, int height, UDOUBLE target_addr);
void init_glyph_cache(void);
void render_status_bar(const char *text, UDOUBLE target_addr);

// Nya hjälpfunktioner för buffert och Jump
int get_physical_x(int col);
int get_physical_y(int row);
void redraw_buffer(char buffer[MAX_ROWS][MAX_COLS], UDOUBLE target_addr);
void word_wrap(char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr);
void display_jump(char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr);

void stitch_and_render_screen(char buffer[MAX_ROWS][MAX_COLS], UDOUBLE target_addr);

// Skärmbufferten som speglar dokumentet (Vyn)
char view_buffer[MAX_ROWS][MAX_COLS];

// Den statiska bildbufferten i RAM
uint8_t full_screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

#endif
