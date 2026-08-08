#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "file_io.h"
#include "../firmware/display.h"
#include "model.h"

#include <stdlib.h>
#include <sys/stat.h>

// Hjälpfunktion för att kontrollera och skapa kataloger
static void ensure_directory_exists(char *filepath) {
    char *p;
    // Stega framåt och leta efter varje snedstreck efter rotkatalogen
    for (p = strchr(filepath + 1, '/'); p != NULL; p = strchr(p + 1, '/')) {
        *p = '\0'; // Pausa strängen temporärt för att isolera den aktuella mappen

        struct stat st = {0};
        if (stat(filepath, &st) == -1) {
            mkdir(filepath, 0700);
        }

        *p = '/'; // Återställ snedstrecket innan vi letar vidare
    }
}

static void get_full_path(const char *filename, char *full_path, size_t max_len) {
    const char *home_dir = NULL;
    const char *sudo_user = getenv("SUDO_USER");

    if (sudo_user != NULL) {
        // Hämta hemkatalogen för den användare som anropade sudo
        struct passwd *pw = getpwnam(sudo_user);
        if (pw != NULL) {
            home_dir = pw->pw_dir;
        }
    } else {
        // Om programmet körs utan sudo, hämta aktuell användares hemkatalog
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            home_dir = pw->pw_dir;
        } else {
            home_dir = getenv("HOME");
        }
    }

    if (home_dir != NULL) {
        snprintf(full_path, max_len, "%s/Dokument/writer/%s", home_dir, filename);
    } else {
        // Fallback om systemanropen av någon anledning misslyckas
        // Sparar då filen i den aktuella arbetskatalogen
        snprintf(full_path, max_len, "%s", filename);
    }
}

static FILE *temp_file = NULL;

void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    char filepath[512];
    get_full_path(filename, filepath, sizeof(filepath));

    memset(buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
    model_init();

    FILE *file = fopen(filepath, "r");
    if (file != NULL) {
        int ch;
        while ((ch = fgetc(file)) != EOF) {
            // Identifiera starten på ett svenskt UTF-8-tecken
            if (ch == 0xC3) {
                int next_ch = fgetc(file);
                if (next_ch == 0xA5) model_insert_char(0xE5);      // å
                else if (next_ch == 0xA4) model_insert_char(0xE4); // ä
                else if (next_ch == 0xB6) model_insert_char(0xF6); // ö
                else if (next_ch == 0x85) model_insert_char(0xC5); // Å
                else if (next_ch == 0x84) model_insert_char(0xC4); // Ä
                else if (next_ch == 0x96) model_insert_char(0xD6); // Ö
            } else if ((ch >= 32 && ch <= 126) || ch == '\n') {
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

void save_document_to_file(const char *filename) {
    char filepath[512];
    get_full_path(filename, filepath, sizeof(filepath));

    ensure_directory_exists(filepath);

    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        return; // Tips: Här kan vi lägga till en utskrift i statusraden om fel
    }

    int doc_length = model_get_text_length();

    for (int i = 0; i < doc_length; i++) {
        unsigned char ch = (unsigned char)model_char_at(i);

        // Översätt intern Latin-1 till UTF-8
        if (ch == 0xE5) { fputc(0xC3, file); fputc(0xA5, file); }      // å
        else if (ch == 0xE4) { fputc(0xC3, file); fputc(0xA4, file); } // ä
        else if (ch == 0xF6) { fputc(0xC3, file); fputc(0xB6, file); } // ö
        else if (ch == 0xC5) { fputc(0xC3, file); fputc(0x85, file); } // Å
        else if (ch == 0xC4) { fputc(0xC3, file); fputc(0x84, file); } // Ä
        else if (ch == 0xD6) { fputc(0xC3, file); fputc(0x96, file); } // Ö
        else if (ch >= 32 || ch == '\n') {
            fputc(ch, file); // Standard ASCII
        }
    }
    fclose(file);
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
