#include "file_io.h"
#include "../firmware/display.h"
#include <stdio.h>
#include <string.h>

static FILE *temp_file = NULL;

void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    memset(buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS); // Rensar hela den allokerade ytan

    // TODO: Din logik för att läsa från SD-kortet

    *cursor_row = current_max_rows - JUMP_LINES;
    *cursor_col = 0;

    // Skicka med target_addr här
    stitch_and_render_screen(buffer, target_addr);
}

void append_to_temp_file(char c) {
    if (temp_file == NULL) {
        // Öppna i append-läge. Dold fil.
        temp_file = fopen(".wimwriter_temp.txt", "a");
    }

    if (temp_file != NULL) {
        if (c == 127) {
            // Vid backspace loggar vi en speciell markör eller hanterar logiken
            // Enklast i en rå logg är att bara skriva ett styrtecken, t.ex. <BS>
            fputs("<BS>", temp_file);
        } else {
            fputc(c, temp_file);
        }
        // OBS: Undvik fflush(temp_file) här för att låta Linux sköta cachen
        // asynkront och därmed skona ARMv6-processorn från blockeringar.
    }
}

void save_buffer_to_file(const char *filename, const char *buffer, int max_rows, int max_cols) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return; // Här kan felhantering/statusmeddelande läggas till
    }

    // Iterera över bufferten och skriv till fil
    for (int r = 0; r < max_rows; r++) {
        int last_char_in_row = -1;

        // Hitta var raden slutar för att slippa spara onödiga mellanslag
        for (int c = max_cols - 1; c >= 0; c--) {
            char ch = buffer[(r * max_cols) + c];
            if (ch != ' ' && ch != '\0') {
                last_char_in_row = c;
                break;
            }
        }

        // Skriv radens tecken
        for (int c = 0; c <= last_char_in_row; c++) {
            char ch = buffer[(r * max_cols) + c];
            if (ch >= 32 && ch <= 126) {
                fputc(ch, file);
            }
        }

        // Lägg till radbrytning om det finns data, eller om det är en medveten tom rad
        fputc('\n', file);
    }

    fclose(file);
}
