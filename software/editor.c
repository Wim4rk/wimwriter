#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "editor.h"
#include "file_io.h"
#include "../firmware/keyboard.h"
#include "../firmware/display.h"
#include "sync.h"
#include "model.h"


bool status_bar_visible = false;
time_t status_bar_timestamp = 0;

bool is_wifi_active = false;

EditorState current_state = STATE_EDITING;

#define RENDER_THRESHOLD 8
#define MAX_FILES_IN_DIR 50

// Variabler för filhantering
static bool is_suggested_name = false;
static char current_filename[256] = "";
static int previous_file_index = 0;
static int current_file_index = 0;
static int pending_start_col = -1;
static int filename_len = 0;

static char previous_filename[256] = "";
static bool just_created_new_file = false;

typedef struct {
    char filename[256];
    time_t last_modified;
} FileEntry;

static FileEntry file_list[MAX_FILES_IN_DIR];
static int total_files_found = 0;

static void show_help_box(UDOUBLE target_addr) {
    int box_w = 700;
    int box_h = 500;

    // Centrera rutan fysiskt på skärmen
    int phys_x = (SCREEN_WIDTH - box_w) / 2;
    int phys_y = (SCREEN_HEIGHT - box_h) / 2;

    // Statisk allokering för att spara cykler och minne på ARMv6
    static UBYTE help_buffer[700 * 500];

    // 1. Fyll hela ytan med svart (skapar ramen på 2px)
    memset(help_buffer, 0x00, sizeof(help_buffer));

    // 2. Fyll insidan med vitt (0xFF)
    for (int y = 2; y < box_h - 2; y++) {
        memset(&help_buffer[y * box_w + 2], 0xFF, box_w - 4);
    }

    // Listan med funktionsknappar från specifikationen
    const char *lines[] = {
        "F1  - Denna hj\xE4lpruta",     // \xE4 = ä
        "F2  - Spara manuellt",
        "F3  - \xD6ppna / Byt fil",     // \xD6 = Ö
        "F4  - Ny fil",
        "F5  - Uppdatera sk\xE4rm",     // \xE4 = ä
        "F9  - Synkronisera mot NAS",
        "F10 - WiFi P\xE5/Av",          // \xE5 = å
        "",
        "Esc - \xC5terg\xE5"            // \xC5 = Å, \xE5 = å
    };
    int num_lines = sizeof(lines) / sizeof(lines[0]);

    // Visuell överkant är fysisk nederkant i bufferten
    int start_local_y = box_h - FONT_H - 40;

    for (int i = 0; i < num_lines; i++) {
        int len = strlen(lines[i]);

        // Visuell vänsterkant är fysisk högerkant i bufferten
        int current_local_x = box_w - 40 - FONT_W;

        for (int c = 0; c < len; c++) {
            unsigned char uc = (unsigned char)lines[i][c];
            if (uc >= 32) {
                const UBYTE *glyph = pre_flipped_glyphs[uc];
                for (int h = 0; h < FONT_H; h++) {
                    memcpy(&help_buffer[(start_local_y + h) * box_w + current_local_x],
                           &glyph[h * FONT_W],
                           FONT_W);
                }
            }
            // Stega fysiskt åt vänster för nästa tecken
            current_local_x -= FONT_W;
        }
        // Stega fysiskt nedåt i bufferten för nästa textrad
        start_local_y -= (FONT_H + 15);
    }

    send_and_display_buffer(help_buffer, phys_x, phys_y, box_w, box_h, target_addr, IT8951_A2_MODE);
}

static void hide_help_box_and_redraw(char *text_buffer, UDOUBLE target_addr) {
    // Ritar om skärmen baserat på den underliggande textbufferten
    // vilket effektivt skriver över hjälprutan.
    stitch_and_render_screen(text_buffer, target_addr);
}

static void save_to_sd(const char *filename, UDOUBLE target_addr) {
    save_document_to_file(filename);

    char status_text[300];
    snprintf(status_text, sizeof(status_text), "Sparad: %s", filename);
    render_status_bar(status_text, target_addr);
}

static void update_status_bar_visuals(UDOUBLE target_addr) {
    char status_text[300];
    snprintf(status_text, sizeof(status_text), "Spara som: %s", current_filename);
    render_status_bar(status_text, target_addr);
}

static void append_char_to_filename(char c) {
    if (filename_len < sizeof(current_filename) - 1) {
        current_filename[filename_len] = c;
        filename_len++;
        current_filename[filename_len] = '\0';
    }
}

static void remove_last_char_from_filename(void) {
    if (filename_len > 0) {
        filename_len--;
        current_filename[filename_len] = '\0';
    }
}

static void clear_filename_buffer(void) {
    memset(current_filename, 0, sizeof(current_filename));
    filename_len = 0;
}

static void generate_default_filename(void){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(current_filename, sizeof(current_filename), "wim - %04d-%02d-%02d_%02d_%02d.txt",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
    filename_len = strlen(current_filename);
}

static int compare_file_entries(const void *a, const void *b) {
    FileEntry *entry_a = (FileEntry *)a;
    FileEntry *entry_b = (FileEntry *)b;
    if (entry_a->last_modified < entry_b->last_modified) return 1;
    if (entry_a->last_modified > entry_b->last_modified) return -1;
    return 0;
}

static void scan_directory_for_files(void) {
    DIR *d;
    struct dirent *dir;
    struct stat file_stat;
    char dir_path[512];

    const char *home = getenv("HOME");
    if (!home) home = "/home/olov";
    snprintf(dir_path, sizeof(dir_path), "%s/Dokument/writer", home);

    total_files_found = 0;
    current_file_index = 0;

    d = opendir(dir_path);
    if (d) {
        while ((dir = readdir(d)) != NULL && total_files_found < MAX_FILES_IN_DIR) {
            if (dir->d_name[0] != '.') {
                if (strstr(dir->d_name, ".txt") != NULL || strstr(dir->d_name, ".md") != NULL) {
                    // Vi måste bygga hela sökvägen för att stat() ska fungera
                    char full_path[1024];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);

                    if (stat(full_path, &file_stat) == 0) {
                        snprintf(file_list[total_files_found].filename, sizeof(file_list[total_files_found].filename), "%s", dir->d_name);
                        file_list[total_files_found].last_modified = file_stat.st_mtime;
                        total_files_found++;
                    }
                }
            }
        }
        closedir(d);

        if (total_files_found > 0) {
            qsort(file_list, total_files_found, sizeof(FileEntry), compare_file_entries);
        }
    }
}

static void show_file_in_status_bar(UDOUBLE target_addr) {
    if (total_files_found == 0) return;

    char status_text[300];
    snprintf(status_text, sizeof(status_text), "Öppna: %.255s", file_list[current_file_index].filename);
    render_status_bar(status_text, target_addr);
}

static void show_next_file(UDOUBLE target_addr) {
    if (total_files_found == 0) return;

    current_file_index++;
    if (current_file_index >= total_files_found) {
        current_file_index = 0;
    }

    show_file_in_status_bar(target_addr);
}

static void process_text_input(char c, char *text_buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr, bool more_keys_waiting) {

    unsigned char uc = (unsigned char)c;
    bool is_ctrl_bs = (c == 127 && keyboard_is_ctrl_pressed());

    if (!is_ctrl_bs) {
        if (c == 127) model_delete_char();
        else if (uc >= 32 || c == '\n') model_insert_char(c);
        append_to_temp_file(c);
    }

    if (c == 127 && pending_start_col != -1) {
        if (*cursor_col > pending_start_col) {
            (*cursor_col)--;
            BUF_AT(text_buffer, *cursor_row, *cursor_col) = '\0';
        }

        editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

        if (*cursor_col == pending_start_col) {
            pending_start_col = -1;
        }
        return;
    }

    switch (c) {
        case '\n': // Enter
            editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);
            *cursor_col = 0;
            (*cursor_row)++;
            break;

        case 127: // Backspace
            editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);
            if (keyboard_is_ctrl_pressed()) {
                int r = *cursor_row;
                int c = *cursor_col;
                int old_row = *cursor_row;

                // 1. Hoppa över eventuella mellanslag bakåt (även över radbrytningar)
                while (r > 0 || c > 0) {
                    int prev_c = (c == 0) ? MAX_COLS - 1 : c - 1;
                    int prev_r = (c == 0) ? r - 1 : r;

                    if (BUF_AT(text_buffer, prev_r, prev_c) != ' ') break;

                    c = prev_c;
                    r = prev_r;
                }

                // 2. Leta bakåt tills vi hittar nästa mellanslag (för att hitta ordets början)
                while (r > 0 || c > 0) {
                    int prev_c = (c == 0) ? MAX_COLS - 1 : c - 1;
                    int prev_r = (c == 0) ? r - 1 : r;

                    if (BUF_AT(text_buffer, prev_r, prev_c) == ' ') break;

                    c = prev_c;
                    r = prev_r;
                }

                int word_len = (*cursor_row * MAX_COLS + *cursor_col) - (r * MAX_COLS + c);

                if (word_len > 0) {
                    // 3. Synka underliggande datamodell (GAP-bufferten)
                    for (int i = 0; i < word_len; i++) {
                        model_delete_char();
                    }

                    // 4. Stega markören bakåt och rensa skärmbufferten logiskt
                    for (int i = 0; i < word_len; i++) {
                        if (*cursor_col > 0) {
                            (*cursor_col)--;
                        } else if (*cursor_row > 0) {
                            (*cursor_row)--;
                            *cursor_col = MAX_COLS - 1;
                        }
                        BUF_AT(text_buffer, *cursor_row, *cursor_col) = '\0';
                    }

                    // 5. Rita om de påverkade raderna
                    render_rows_stitched(*cursor_row, old_row, text_buffer, target_addr);
                }
            } else {
                if (*cursor_col > 0) {
                    (*cursor_col)--;
                    BUF_AT(text_buffer, *cursor_row, *cursor_col) = '\0';

                    int px_back = get_physical_x(*cursor_col);
                    int py_back = get_physical_y(*cursor_row);

                    render_char(' ', px_back, py_back, target_addr);
                } else if (*cursor_row > 0) {
                    (*cursor_row)--;
                    *cursor_col = MAX_COLS - 1;

                    while (*cursor_col > 0 && BUF_AT(text_buffer, *cursor_row, *cursor_col) == '\0') {
                        (*cursor_col)--;
                    }
                } else {
                    break;
                }
            }

            int px_back = get_physical_x(*cursor_col);
            int py_back = get_physical_y(*cursor_row);
            clear_area(px_back, py_back, FONT_W, FONT_H, target_addr);
            break;

        default:
            if (uc >= 32) {
                BUF_AT(text_buffer, *cursor_row, *cursor_col) = c;

                if (pending_start_col == -1 && !more_keys_waiting) {
                    int px = get_physical_x(*cursor_col);
                    int py = get_physical_y(*cursor_row);
                    render_char(c, px, py, target_addr);

                    (*cursor_col)++;
                }
                else {
                    if (pending_start_col == -1) {
                        pending_start_col = *cursor_col;
                    }

                    (*cursor_col)++;
                    int queue_len = *cursor_col - pending_start_col;

                    if (queue_len >= RENDER_THRESHOLD || c == ' ' || !more_keys_waiting) {
                        editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);
                    }
                }

                if (*cursor_col >= MAX_COLS) {
                    editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);
                    word_wrap(text_buffer, cursor_row, cursor_col, target_addr);
                }
            }
            break;
    }

    if (*cursor_row >= MAX_ROWS) {
        display_jump(text_buffer, cursor_row, cursor_col, target_addr);
    }
}

void editor_flush_queue(char *text_buffer, int cursor_row, int cursor_col, UDOUBLE target_addr) {
    if (pending_start_col != -1 && cursor_col > pending_start_col) {
        int len = cursor_col - pending_start_col;
        char temp_str[len + 1];

        for (int i = 0; i < len; i++) {
            temp_str[i] = BUF_AT(text_buffer, cursor_row, pending_start_col + i);
        }
        temp_str[len] = '\0';

        int px = get_physical_x(pending_start_col);
        int py = get_physical_y(cursor_row);

        render_stitched_text(temp_str, px, py, target_addr);
    }
    pending_start_col = -1;
}

void handle_input(struct input_event *ev, UDOUBLE target_addr, char *text_buffer, int *cursor_row, int *cursor_col, bool more_keys_waiting) {
    if (ev->type != EV_KEY) return;

    int key_code = ev->code;
    char c = keyboard_get_char(ev);

    switch (current_state) {

        case STATE_EDITING:
            if (key_code == KEY_ESC && just_created_new_file) {
                strncpy(current_filename, previous_filename, sizeof(current_filename));
                filename_len = strlen(current_filename);

                load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);
                just_created_new_file = false;
                hide_status_bar_and_redraw(target_addr);
                break;
            }

            if (c > 0 && just_created_new_file) {
                just_created_new_file = false;
                hide_status_bar_and_redraw(target_addr);
            }

            if (key_code == KEY_F1) {
                show_help_box(target_addr);
                current_state = STATE_HELP;
            }
            else if (key_code == KEY_F2) {
                if (strlen(current_filename) == 0) {
                    is_suggested_name = true;
                    generate_default_filename();
                    update_status_bar_visuals(target_addr);
                    current_state = STATE_NAMING_FILE;
                } else {
                    save_to_sd(current_filename, target_addr);
                }
            }
            else if (key_code == KEY_F3) {
                scan_directory_for_files();

                if (total_files_found > 0) {
                    previous_file_index = 0;

                    if (strlen(current_filename) > 0) {
                        current_file_index = 1 % total_files_found;
                    }

                    show_file_in_status_bar(target_addr);
                    current_state = STATE_FILE_SWITCH;
                }
            }
            else if (key_code == KEY_F4) {
                if (strlen(current_filename) > 0) {
                    save_document_to_file(current_filename);
                    strncpy(previous_filename, current_filename, sizeof(previous_filename));
                }

                model_init();

                memset(text_buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
                *cursor_row = JUMP_LINES;
                *cursor_col = 0;

                clear_filename_buffer();
                just_created_new_file = true;

                stitch_and_render_screen(text_buffer, target_addr);
                render_status_bar("Ny fil. (Tryck Esc för att återgå)", target_addr);
            }
            else if (key_code == KEY_F5) {
                refresh_display_full(text_buffer, target_addr);
            }
            else if (key_code == KEY_F10) {
                toggle_wifi();

                if (is_wifi_active) {
                    // Om WiFi precis slogs på, tvinga fram statusraden.
                    // Ett tomt textfält uppdaterar raden, och renderingsmotorn
                    // läser av is_wifi_active för att rita in "WiFi" längst till höger.
                    render_status_bar("", target_addr);
                } else {
                    // Om WiFi stängdes av, städa undan raden omedelbart (om inget annat visas).
                    hide_status_bar_and_redraw(target_addr);
                }
            }
            else {
                if (c > 0) {
                    process_text_input(c, text_buffer, cursor_row, cursor_col, target_addr, more_keys_waiting);
                }
            }
            break;

        case STATE_HELP:
            if (key_code == KEY_ESC) {
                hide_help_box_and_redraw(text_buffer, target_addr);
                current_state = STATE_EDITING;
            }
            break;

        case STATE_FILE_SWITCH:
            if (key_code == KEY_F3) {
                show_next_file(target_addr);
            }
            else if (key_code == KEY_ESC) {
                current_file_index = previous_file_index;
                hide_status_bar_and_redraw(target_addr);
                current_state = STATE_EDITING;
            }
            else if (c == '\n') {
                hide_status_bar_and_redraw(target_addr);

                snprintf(current_filename, sizeof(current_filename), "%s", file_list[current_file_index].filename);
                filename_len = strlen(current_filename);

                load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);

                current_state = STATE_EDITING;
            }
            else if (c > 0) {
                hide_status_bar_and_redraw(target_addr);
                current_state = STATE_EDITING;
                process_text_input(c, text_buffer, cursor_row, cursor_col, target_addr, more_keys_waiting);
            }
            break;

        case STATE_NAMING_FILE:
            if (c > 0) {
                if (c == '\n') {
                    if(access(current_filename, F_OK) == 0) {
                        char warning_text[300];
                        snprintf(warning_text, sizeof(warning_text), "Skriv över fil: '%s'?", current_filename);
                        render_status_bar(warning_text, target_addr);
                        current_state = STATE_CONFIRM_OVERWRITE;
                    } else {
                        save_to_sd(current_filename, target_addr);
                        hide_status_bar_and_redraw(target_addr);
                        current_state = STATE_EDITING;
                    }
                }
                else if (c == 127) {
                    if (is_suggested_name) {
                        clear_filename_buffer();
                        is_suggested_name = false;
                    } else {
                        remove_last_char_from_filename();
                    }
                    update_status_bar_visuals(target_addr);
                }
                else {
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
        case STATE_CONFIRM_OVERWRITE:
            if (c == 'j' || c == 'J') {
                save_to_sd(current_filename, target_addr);
                hide_status_bar_and_redraw(target_addr);
                current_state = STATE_EDITING;
            }
            else if (c == 'n' || c == 'N') {
                update_status_bar_visuals(target_addr);
                current_state = STATE_NAMING_FILE;
            }
            break;

            if (c == '\n') {
                putchar('\n');
            } else if (c == 127) {
                printf("\b \b");
            } else if (c >= 32 && c <= 126) {
                putchar(c);
            }
            fflush(stdout);
    }
}
