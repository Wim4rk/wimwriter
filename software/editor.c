#include "editor.h"
#include "file_io.h"
#include "../firmware/keyboard.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

EditorState current_state = STATE_EDITING;

// Variabler för filhantering
static bool is_suggested_name = false;
static char current_filename[256] = "";
static int previous_file_index = 0;
static int current_file_index = 0;

// ==========================================
// PLATSHÅLLARE (STUBS) - Fylls i senare
// ==========================================
static void show_help_box(void) {}
static void hide_help_box_and_redraw(void) {}
static void show_file_in_status_bar(void) {}
static void hide_status_bar_and_redraw(void) {}
static void show_next_file(void) {}
static void save_to_sd(const char *filename) {}
static void force_full_redraw(void) {}

// Bygger strängen och skickar den till skärmen
static void update_status_bar_visuals(UDOUBLE target_addr) {
    char status_text[256];
    snprintf(status_text, sizeof(status_text), "Spara som: %s", current_filename);
    render_status_bar(status_text, target_addr);
}

static void append_char_to_filename(char c) {
    int len = strlen(current_filename);
    // Skydda bufferten från att svämma över
    if (len < sizeof(current_filename) - 1) {
        current_filename[len] = c;
        current_filename[len + 1] = '\0';
    }
}

static void remove_last_char_from_filename(void) {
    int len = strlen(current_filename);
    if (len > 0) {
        current_filename[len - 1] = '\0';
    }
}

static void clear_filename_buffer(void) {
    memset(current_filename, 0, sizeof(current_filename));
}

static void generate_default_filename(void){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(current_filename, sizeof(current_filename), "wimwriter - %04d-%02d-%02d_%02d%02d.txt",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
}

static void process_text_input(char c, char text_buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
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
                *cursor_col = MAX_COLS - 1;

                // Stega bakåt förbi de osynliga null-terminatorer som word_wrap har lämnat efter sig
                while (*cursor_col > 0 && text_buffer[*cursor_row][*cursor_col] == '\0') {
                    (*cursor_col)--;
                }
            } else {
                // Vi är i det absoluta övre vänstra hörnet, avbryt radering
                break;
            }

            // 1. Logisk radering i RAM-minnet
            text_buffer[*cursor_row][*cursor_col] = '\0';

            // 2. Visuell radering på e-bläckskärmen i A2-läge[cite: 1]
            int px_back = get_physical_x(*cursor_col);
            int py_back = get_physical_y(*cursor_row);

            clear_area(px_back, py_back, GLYPH_W, GLYPH_H, target_addr);
            break;

        default: // Vanliga tecken
            if (c > 0) {
                int px = get_physical_x(*cursor_col);
                int py = get_physical_y(*cursor_row);

                // Lagra i RAM och rita ut
                text_buffer[*cursor_row][*cursor_col] = c;
                render_char(c, px, py, target_addr);

                (*cursor_col)++;

                // Hantera automatisk radbrytning
                if (*cursor_col >= MAX_COLS) {
                    word_wrap(text_buffer, cursor_row, cursor_col, target_addr);
                }
            }
            break;
    }

    // Hoppa upp (Jump) om vi når botten av den definierade skrivytan[cite: 2]
    if (*cursor_row >= MAX_ROWS) {
        display_jump(text_buffer, cursor_row, cursor_col, target_addr);
    }
}

// ==========================================
// HUVUDLOGIK
// ==========================================
void handle_input(struct input_event *ev, UDOUBLE target_addr, char text_buffer[MAX_ROWS][MAX_COLS], int *cursor_row, int *cursor_col) {

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
            else {
                // Standard textinmatning skickas till redigeringsmotorn
                if (c > 0 || key_code == KEY_BACKSPACE) {
                    process_text_input(c > 0 ? c : 127, text_buffer, cursor_row, cursor_col, target_addr);
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
                hide_status_bar_and_redraw();
                current_state = STATE_EDITING;
            }
            else if (c > 0) {
                hide_status_bar_and_redraw();
                // TODO: load_file_into_buffer(...)
                force_full_redraw();
                current_state = STATE_EDITING;
                process_text_input(c, text_buffer, cursor_row, cursor_col, target_addr);
            }
            break;

        // case STATE_NAMING_FILE:
        //     if (c > 0) {
        //         if (c == '\n') { // Enter
        //             save_to_sd(current_filename);
        //             current_state = STATE_EDITING;
        //             hide_status_bar_and_redraw();
        //         }
        //         else if (c == 127) { // Backspace
        //             if (is_suggested_name) {
        //                 clear_filename_buffer();
        //                 is_suggested_name = false;
        //             } else {
        //                 remove_last_char_from_filename();
        //             }
        //             // TODO: Uppdatera statusraden visuellt
        //         }
        //         else {
        //             if (is_suggested_name) {
        //                 clear_filename_buffer();
        //                 is_suggested_name = false;
        //                 // TODO: Rensa bort den gamla texten från statusraden visuellt via clear_area()
        //             }
        //             append_char_to_filename(c);
        //             // TODO: Rita ut det nya tecknet i statusraden
        //         }
        //     }
        //     else if (key_code == KEY_ESC) {
        //         hide_status_bar_and_redraw();
        //         current_state = STATE_EDITING;
        //     }
        //     break;
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
    }
}
