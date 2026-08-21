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
    // Om filnamnet redan är en absolut sökväg, använd den direkt.
    if (filename[0] == '/') {
        snprintf(full_path, max_len, "%s", filename);
        return;
    }

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

// 1. Hjälpfunktion för att mappa UTF-8 till intern Latin-1
char map_utf8_to_internal(unsigned char byte1, unsigned char byte2) {
    if (byte1 == 0xC3) {
        if (byte2 == 0xA5) return (char)0xE5; // å
        if (byte2 == 0xA4) return (char)0xE4; // ä
        if (byte2 == 0xB6) return (char)0xF6; // ö
        if (byte2 == 0x85) return (char)0xC5; // Å
        if (byte2 == 0x84) return (char)0xC4; // Ä
        if (byte2 == 0x96) return (char)0xD6; // Ö
    }
    return 0;
}

// Hjälpfunktion för att konvertera intern Latin-1 till UTF-8 och skriva till fil
void write_internal_to_utf8(unsigned char ch, FILE *file) {
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

// 2. Funktion för att tvätta strängar (säkrar filnamn och commit-meddelanden)
void sanitize_string(const char *input, char *output, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < max_len - 1; i++) {
        unsigned char ch = (unsigned char)input[i];

        // Om indatan är i UTF-8 (t.ex. vid inläsning från OS)
        if (ch == 0xC3 && input[i+1] != '\0') {
            unsigned char next_ch = input[i+1];
            if (next_ch == 0xA5 || next_ch == 0xA4) output[j++] = 'a';
            else if (next_ch == 0xB6) output[j++] = 'o';
            else if (next_ch == 0x85 || next_ch == 0x84) output[j++] = 'A';
            else if (next_ch == 0x96) output[j++] = 'O';
            i++; // Hoppa över nästa byte
        }
        // Om indatan är i intern Latin-1 (t.ex. inskrivet via editorns tangentbordslogik)
        else if (ch == 0xE5 || ch == 0xE4) output[j++] = 'a';
        else if (ch == 0xF6) output[j++] = 'o';
        else if (ch == 0xC5 || ch == 0xC4) output[j++] = 'A';
        else if (ch == 0xD6) output[j++] = 'O';
        // Släpp igenom standard ASCII, ersätt mellanslag med understreck
        else if (ch >= 32 && ch <= 126) {
            if (ch == ' ') output[j++] = '_';
            else output[j++] = ch;
        }
    }
    output[j] = '\0';
}

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
                char mapped = map_utf8_to_internal((unsigned char)ch, (unsigned char)next_ch);
                if (mapped != 0) {
                    model_insert_char(mapped);
                }
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

                if (break_col > 0 && (c - break_col - 1) > 0) {
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

    // 6. Justera vyhöjden enligt specifikation (lämna JUMP_LINES tomma i botten)
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

    // 7. Ett enda stort SPI-anrop (i det snabba A2-läget)
    // Ser ut att vara en dubbel skärmuppdatering på gång här:
    // stitch_and_render_screen(buffer, target_addr);
}

void save_document_to_file(const char *filename) {
    // 1. Tvätta filnamnet så vi inte får in ogiltiga tecken i filsystemet
    char safe_filename[256];
    sanitize_string(filename, safe_filename, sizeof(safe_filename));

    char filepath[512];
    get_full_path(safe_filename, filepath, sizeof(filepath));

    ensure_directory_exists(filepath);

    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        return;
    }

    int doc_length = model_get_text_length();

    // 2. Skriv innehållet med vår nya hjälpfunktion
    for (int i = 0; i < doc_length; i++) {
        unsigned char ch = (unsigned char)model_char_at(i);
        write_internal_to_utf8(ch, file);
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
