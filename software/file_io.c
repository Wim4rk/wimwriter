#include "file_io.h"
#include "../firmware/display.h"
#include <stdio.h>
#include <string.h>

void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    memset(buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS); // Rensar hela den allokerade ytan

    // TODO: Din logik för att läsa från SD-kortet

    *cursor_row = current_max_rows - JUMP_LINES;
    *cursor_col = 0;

    // Skicka med target_addr här
    stitch_and_render_screen(buffer, target_addr);
}
