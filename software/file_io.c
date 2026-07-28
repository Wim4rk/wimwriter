#include "file_io.h"
#include "../firmware/display.h"
#include <stdio.h>
#include <string.h>

void load_file_into_buffer(const char *filename, char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col) {
    // 1. Rensa hela bufferten
    memset(buffer, ' ', MAX_ROWS * MAX_COLS * sizeof(char));

    // 2. Läs in text från SD-kortet.
    // Här lägger du din inläsningslogik som fyller bufferten från rad 0
    // fram till MAX_ROWS - JUMP_LINES - 1.

    // 3. Sätt markören på den första lediga raden i bottenmarginalen
    *cursor_row = MAX_ROWS - JUMP_LINES;
    *cursor_col = 0;

    // 4. Uppdatera skärmen med det inlästa dokumentet
    stitch_and_render_screen(buffer);
}
