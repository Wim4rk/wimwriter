#include <stdio.h>
#include <string.h>
#include <time.h>
#include "editor.h"
#include "file_io.h"
#include "../firmware/keyboard.h"
#include "model.h"

EditorState current_state = STATE_EDITING;

#define RENDER_THRESHOLD 10

// Variabler för filhantering
static bool is_suggested_name = false;
static char current_filename[256] = "";
static int previous_file_index = 0;
static int current_file_index = 0;
static int pending_start_col = -1;
static int filename_len = 0;

// ==========================================
// PLATSHÅLLARE (STUBS) - Fylls i senare
// ==========================================
static void show_help_box(void) {}
static void hide_help_box_and_redraw(void) {}
static void show_file_in_status_bar(void) {}
static void show_next_file(void) {}
static void force_full_redraw(void) {}

static void save_to_sd(const char *filename) {
    // Anropar funktionen i file_io.c
    save_buffer_to_file(filename, text_buffer, current_max_rows, current_max_cols);

    // Visuell bekräftelse i statusraden
    char status_text[256];
    snprintf(status_text, sizeof(status_text), "Sparad: %s", filename);
    render_status_bar(status_text, target_addr);
}

static void hide_status_bar_and_redraw(UDOUBLE target_addr) {
    // Om du vill implementera städningen direkt kan du lägga in:
    int start_y = SCREEN_HEIGHT - MARGIN_BOTTOM;
    clear_area(0, start_y, SCREEN_WIDTH, MARGIN_BOTTOM, target_addr);
}

// Bygger strängen och skickar den till skärmen
static void update_status_bar_visuals(UDOUBLE target_addr) {
    char status_text[256];
    snprintf(status_text, sizeof(status_text), "Spara som: %s", current_filename);
    render_status_bar(status_text, target_addr);
}

static void append_char_to_filename(char c) {
    // Kontrollera så vi inte skriver utanför bufferten
    if (filename_len < sizeof(current_filename) - 1) {
        current_filename[filename_len] = c;
        filename_len++; // Öka räknaren med 1
        current_filename[filename_len] = '\0';
    }
}

static void remove_last_char_from_filename(void) {
    if (filename_len > 0) {
        filename_len--; // Minska räknaren med 1
        current_filename[filename_len] = '\0';
    }
}

static void clear_filename_buffer(void) {
    memset(current_filename, 0, sizeof(current_filename));
    filename_len = 0; // Glöm inte att nollställa räknaren
}

static void generate_default_filename(void){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(current_filename, sizeof(current_filename), "wimwriter - %04d-%02d-%02d_%02d%02d.txt",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
    filename_len = strlen(current_filename);
}

static void process_text_input(char c, char *text_buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr, bool more_keys_waiting) {

    // 1. Logik för Datamodellen (RAM)
    if (c == 127) {
        model_delete_char();
    } else if ((c >= 32 && c <= 126) || c == '\n') {
        model_insert_char(c);
    }

    // Kontinuerlig lagring till SD-kort
    append_to_temp_file(c);

    if (c == 127 && pending_start_col != -1) {
        // 1. Tecknet har inte ritats ut, så vi raderar det bara logiskt i RAM.
        if (*cursor_col > pending_start_col) {
            (*cursor_col)--;
            BUF_AT(text_buffer, *cursor_row, *cursor_col) = '\0';

            // Radera från den underliggande datamodellen
            model_delete_char();
        }

        // 2. Om raderingen tömde hela kön, nollställ flaggan
        if (*cursor_col == pending_start_col) {
            pending_start_col = -1;
        }

        // 3. Vi är klara med raderingen. Avbryt funktionen så att
        // den vanliga backspace-logiken (med SPI-anropet) inte körs.
        return;
    }

    switch (c) {
        case '\n': // Enter
            *cursor_col = 0;
            (*cursor_row)++;
            break;

        case 127: // Backspace
            if (*cursor_col > 0) {
                // Normal radering på nuvarande rad
                (*cursor_col)--;
            } else if (*cursor_row > 0) {
                // Vi är på kolumn 0, hoppa upp en rad
                (*cursor_row)--;
                *cursor_col = current_max_cols - 1;

                // Stega bakåt förbi de osynliga null-terminatorer som word_wrap har lämnat efter sig
                while (*cursor_col > 0 && BUF_AT(text_buffer, *cursor_row, *cursor_col) == '\0') {
                    (*cursor_col)--;
                }
            } else {
                // Vi är i det absoluta övre vänstra hörnet, avbryt radering.
                // Eller ska vi ge möjlighet att rendera om skärmen ovanför och
                break;
            }

            // 1. Logisk radering i RAM-minnet
            BUF_AT(text_buffer, *cursor_row, *cursor_col) = '\0';

            // 2. Visuell radering på e-bläckskärmen i A2-läge
            int px_back = get_physical_x(*cursor_col);
            int py_back = get_physical_y(*cursor_row);

            clear_area(px_back, py_back, current_font.width, current_font.height, target_addr);
            break;

        default: // Vanliga tecken
            if (c > 0 && c >= 32 && c <= 126) {

                BUF_AT(text_buffer, *cursor_row, *cursor_col) = c;

                // STANDARDLÄGET: Ingen kö finns, och ingen ny tangent väntar
                if (pending_start_col == -1 && !more_keys_waiting) {
                    // Rendera omedelbart
                    int px = get_physical_x(*cursor_col);
                    int py = get_physical_y(*cursor_row);
                    render_char(c, px, py, target_addr);

                    (*cursor_col)++;
                }
                // CATCH-UP: Maskinen ligger efter
                else {
                    // Markera startpunkt
                    if (pending_start_col == -1) {
                        pending_start_col = *cursor_col;
                    }

                    (*cursor_col)++;

                    // Om detta var sista tecknet i kön, skjut ut hela strängen direkt
                    if (!more_keys_waiting) {
                        int len = *cursor_col - pending_start_col;
                        char temp_str[len + 1];
                        for (int i = 0; i < len; i++) {
                            temp_str[i] = BUF_AT(text_buffer, *cursor_row, pending_start_col + i);
                        }
                        temp_str[len] = '\0';

                        int px = get_physical_x(pending_start_col);
                        int py = get_physical_y(*cursor_row);
                        render_stitched_text(temp_str, px, py, target_addr);

                        // Kön kan nollställas
                        pending_start_col = -1;
                    }
                }

                // Hantera automatisk radbrytning
                // Kolla om det är här som jag får en för tidig radbrytning?
                if (*cursor_col >= current_max_cols) {
                    word_wrap(text_buffer, cursor_row, cursor_col, target_addr);
                    // Om raden bryts nollställer vi kön
                    pending_start_col = -1;
                }
            }
            break;
    }

    // Hoppa upp (Jump) om vi når botten av den definierade skrivytan[cite: 1, 2]
    if (*cursor_row >= current_max_rows) {
        display_jump(text_buffer, cursor_row, cursor_col, target_addr);
    }
}

// ==========================================
// HUVUDLOGIK
// ==========================================
void handle_input(struct input_event *ev, UDOUBLE target_addr, char *text_buffer, int *cursor_row, int *cursor_col, bool more_keys_waiting) {

    // Vi är bara intresserade av tangenttryckningar
    if (ev->type != EV_KEY) return;

    int key_code = ev->code;
    char c = keyboard_get_char(ev);

    switch (current_state) {

        case STATE_EDITING:
            if (key_code == KEY_F1) {
                show_help_box();
                current_state = STATE_HELP;
            }
            else if (key_code == KEY_F2) {
                if (strlen(current_filename) == 0) {
                    is_suggested_name = true;
                    generate_default_filename();
                    update_status_bar_visuals(target_addr);
                    current_state = STATE_NAMING_FILE;
                } else {
                    save_to_sd(current_filename);
                }
            }
            else if (key_code == KEY_F3) {
                previous_file_index = current_file_index;
                show_file_in_status_bar();
                show_next_file(); // Visar nästa fil i statusraden[cite: 2]
                current_state = STATE_FILE_SWITCH;
            }
            else if (key_code == KEY_F4) {
                // TODO: Ny fil
            }
            else {
                // Standard textinmatning skickas till redigeringsmotorn
                if (c > 0) {
                    process_text_input(c, text_buffer, cursor_row, cursor_col, target_addr, more_keys_waiting);
                }
            }
            break;

        case STATE_HELP:
            if (key_code == KEY_ESC) {
                hide_help_box_and_redraw();
                current_state = STATE_EDITING;
            }
            break;

        case STATE_FILE_SWITCH:
            if (key_code == KEY_F3) {
                show_next_file();
            }
            else if (key_code == KEY_ESC) {
                current_file_index = previous_file_index;
                hide_status_bar_and_redraw(target_addr);
                current_state = STATE_EDITING;
            }
            else if (c > 0) {
                hide_status_bar_and_redraw(target_addr);
                // TODO: load_file_into_buffer(...)
                force_full_redraw();
                current_state = STATE_EDITING;
                process_text_input(c, text_buffer, cursor_row, cursor_col, target_addr, more_keys_waiting);
            }
            break;

        case STATE_NAMING_FILE:
            if (c > 0) {
                if (c == '\n') { // Enter
                    // Vi sparar inget än, vi stänger bara rutan
                    hide_status_bar_and_redraw(target_addr);
                    current_state = STATE_EDITING;
                }
                else if (c == 127) { // Backspace
                    if (is_suggested_name) {
                        clear_filename_buffer();
                        is_suggested_name = false;
                    } else {
                        remove_last_char_from_filename();
                    }
                    update_status_bar_visuals(target_addr);
                }
                else { // Du skriver ett nytt tecken
                    if (is_suggested_name) {
                        clear_filename_buffer();
                        is_suggested_name = false;
                    }
                    append_char_to_filename(c);
                    update_status_bar_visuals(target_addr);
                }
            }
            else if (key_code == KEY_ESC) {
                hide_status_bar_and_redraw(target_addr);
                current_state = STATE_EDITING;
            }
            break;

            // Felsökningsutskrift till terminalen
            if (c == '\n') {
                putchar('\n');
            } else if (c == 127) {
                // Backspace i terminalen (flytta markör bakåt, skriv över med mellanslag, flytta bakåt igen)
                printf("\b \b");
            } else if (c >= 32 && c <= 126) {
                putchar(c);
            }
            // stdout är radbuffrad som standard, så vi tvingar fram utskriften
            fflush(stdout);
    }
}
