#include "file_io.h"
#include "../firmware/display.h"
#include <stdio.h>
#include <string.h>

void load_file_into_buffer(const char *filename, char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col) {
    // 1. Rensa bufferten helt med mellanslag
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            buffer[r][c] = ' ';
        }
    }

    // 2. Sätt markören på startpositionen för ett tomt dokument
    // Vi använder JUMP_LINES för att lämna utrymme i toppen, enligt specifikation.
    *cursor_row = JUMP_LINES;
    *cursor_col = 0;
}
