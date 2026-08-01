#include "file_io.h"
#include <stdio.h>
#include <string.h>
#include "../firmware/display.h"
#include "model.h"

static FILE *temp_file = NULL;

void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // 1. Töm hela skärmbufferten i RAM
    memset(buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);

    // 2. Nollställ datamodellen
    model_init();

    // 3. Fyll GapBuffern med data från filen
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            // Endast utskrivbara tecken och radbrytningar
            if ((ch >= 32 && ch <= 126) || ch == '\n') {
                model_insert_char((char)ch);
            }
        }
        fclose(file);
    }

    int doc_length = model_get_text_length();

    // 4. Hitta startpunkten. Vi backar minst en full skärmlängd tecken bakåt,
    // vilket är tillräckligt för att garanterat fylla vyn.
    int start_index = doc_length - (MAX_ROWS * MAX_COLS);
    if (start_index < 0) start_index = 0;

    // Försök backa till närmaste radbrytning för att få en ren start
    while (start_index > 0 && model_char_at(start_index - 1) != '\n') {
        start_index--;
    }

    // 5. Den "tysta" layout-motorn. Vi itererar framåt i RAM.
    int r = 0;
    int c = 0;

    for (int i = start_index; i < doc_length; i++) {
        char ch = model_char_at(i);

        if (ch == '\n') {
            r++;
            c = 0;
        } else {
            BUF_AT(buffer, r, c) = ch;
            c++;

            // Tyst Word Wrap: Radbrytning utan SPI-anrop
            if (c >= MAX_COLS) {
                int break_col = c - 1;
                int row_start = r * MAX_COLS;

                // Leta efter mellanslag
                while (break_col > 0 && buffer[row_start + break_col] != ' ') {
                    break_col--;
                }

                if (break_col > 0) {
                    int word_len = c - break_col - 1;
                    char temp_str[word_len];

                    // Plocka upp ordet och rensa spåret
                    for (int j = 0; j < word_len; j++) {
                        temp_str[j] = buffer[row_start + break_col + 1 + j];
                        buffer[row_start + break_col + 1 + j] = ' ';
                    }
                    r++;
                    c = word_len;

                    // Tyst Jump: Om vi slår i botten, rulla skärmen uppåt i RAM
                    if (r >= MAX_ROWS) {
                        memmove(buffer, buffer + MAX_COLS, (MAX_ROWS - 1) * MAX_COLS);
                        memset(buffer + (MAX_ROWS - 1) * MAX_COLS, ' ', MAX_COLS);
                        r = MAX_ROWS - 1;
                    }

                    // Sätt in ordet på den nya raden
                    int new_row_start = r * MAX_COLS;
                    for (int j = 0; j < word_len; j++) {
                        buffer[new_row_start + j] = temp_str[j];
                    }
                } else {
                    // Brutal radbrytning (långt ord utan mellanslag)
                    r++;
                    c = 0;
                }
            }
        }

        // Tyst Jump igen för vanliga radbrytningar
        if (r >= MAX_ROWS) {
            memmove(buffer, buffer + MAX_COLS, (MAX_ROWS - 1) * MAX_COLS);
            memset(buffer + (MAX_ROWS - 1) * MAX_COLS, ' ', MAX_COLS);
            r = MAX_ROWS - 1;
        }
    }

    // 6. Justera vyhöjden enligt specifikation (lämna JUMP_LINES tomma i botten)[cite: 2]
    int target_row = MAX_ROWS - JUMP_LINES;
    if (r > target_row) {
        int lines_to_shift = r - target_row;
        // Skifta hela bufferten uppåt
        memmove(buffer, buffer + (lines_to_shift * MAX_COLS), (MAX_ROWS - lines_to_shift) * MAX_COLS);

        // Rensa de nedersta raderna som vi lämnar tomma
        memset(buffer + (MAX_ROWS - lines_to_shift) * MAX_COLS, ' ', lines_to_shift * MAX_COLS);
        r = target_row;
    }

    *cursor_row = r;
    *cursor_col = c;

    // 7. Ett enda stort SPI-anrop (i det snabba A2-läget)[cite: 1]
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

void save_document_to_file(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return; // Felhantering för filåtkomst kan adderas här
    }

    int doc_length = model_get_text_length();

    for (int i = 0; i < doc_length; i++) {
        char ch = model_char_at(i);

        // Skriv endast giltiga tecken samt radbrytningar
        if ((unsigned char)ch >= 32 || ch == '\n') {
            fputc(ch, file);
        }
    }

    fclose(file);
}
