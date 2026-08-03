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
#define JUMP_LINES 3

// ---------------------------------------------------------
// Hårdkodad Layout (Latensoptimering för ARMv6)
// ---------------------------------------------------------
#define FONT_W 24
#define FONT_H 43
#define LINE_SPACING_PX 21

// Total radhöjd blir 64 pixlar
#define ROW_HEIGHT (FONT_H + LINE_SPACING_PX)

// Skrivytans layout (Statiska marginaler)
#define MARGIN_LEFT 68
#define MARGIN_RIGHT 68
#define MARGIN_TOP 44
#define MARGIN_BOTTOM 68

// Statiskt uträknade maxvärden (Exempelvis 54 kolumner och 15 rader)
#define MAX_COLS ((SCREEN_WIDTH - MARGIN_LEFT - MARGIN_RIGHT) / FONT_W)
#define MAX_ROWS ((SCREEN_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM) / ROW_HEIGHT)

// Tillräckligt stort för att hantera en väldigt liten teckenstorlek
#define ABSOLUTE_MAX_COLS 120
#define ABSOLUTE_MAX_ROWS 60

// Den statiska bildbufferten i RAM för helskärmsuppdateringar
extern uint8_t full_screen_buffer[];

// ---------------------------------------------------------
// Initiering och grundläggande skärmstyrning
// ---------------------------------------------------------
void init_display(UDOUBLE *target_addr);
void cleanup_display(void);
void refresh_display_full(char *buffer, UDOUBLE target_addr); // F5

// ---------------------------------------------------------
// Renderingsfunktioner (SPI-överföring till IT8951)
// ---------------------------------------------------------
void render_char(char c, int physical_x, int physical_y, UDOUBLE target_addr);
void clear_area(int physical_x, int physical_y, int width, int height, UDOUBLE target_addr);
void render_status_bar(const char *text, UDOUBLE target_addr);
void render_rows_stitched(int start_row, int end_row, char *buffer, UDOUBLE target_addr);
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
void render_stitched_text(const char *text, int visual_x, int visual_y, UDOUBLE target_addr);

void hide_status_bar_and_redraw(UDOUBLE target_addr);

extern UBYTE pre_flipped_glyphs[256][2048];

// Makro för att räkna ut rätt index i RAM
#define BUF_AT(buf, r, c) buf[((r) * MAX_COLS) + (c)]

#endif // DISPLAY_H
