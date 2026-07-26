#include <stdio.h>
#include <string.h>

void load_file_into_buffer(const char *filename, char buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col) {
    // 1. Rensa bufferten helt med mellanslag från början
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            buffer[r][c] = ' ';
        }
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        // Om filen inte finns (ny fil), börja på rad 0, kolumn 0
        *cursor_row = JUMP_LINES;
        *cursor_col = 0;
        return;
    }

    int row = 0;
    int col = 0;
    int ch;

    // 2. Läs filen tecken för tecken och placera i bufferten med enkel word wrap-hänsyn
    while ((ch = fgetc(file)) != EOF && row < MAX_ROWS) {
        if (ch == '\n') {
            row++;
            col = 0;
        } else {
            if (col < MAX_COLS) {
                buffer[row][col] = (char)ch;
                col++;
            } else {
                // Enkel radbrytning om raden är full i filen
                row++;
                col = 0;
                if (row < MAX_ROWS) {
                    buffer[row][col] = (char)ch;
                    col++;
                }
            }
        }
    }
    fclose(file);

    // 3. Sätt markören på slutet av texten
    // Om filen fyller skärmen eller mer, lämna marginal i botten om möjligt,
    // eller sätt markören på sista aktiva raden.
    *cursor_row = row < MAX_ROWS ? row : MAX_ROWS - 1;
    *cursor_col = col < MAX_COLS ? col : MAX_COLS - 1;
}
