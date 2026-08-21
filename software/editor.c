#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include "editor.h"
#include "file_io.h"
#include "../firmware/keyboard.h"
#include "../firmware/display.h"
#include "sync.h"
#include "model.h"


bool status_bar_visible = false;
time_t status_bar_timestamp = 0;

bool is_wifi_active = false;
bool is_insert_mode = true;

EditorState current_state = STATE_EDITING;

#define RENDER_THRESHOLD 8
#define MAX_FILES_IN_DIR 50
#define BROWSER_MAX_VISIBLE 10

// Variabler för filhantering
static bool is_suggested_name = false;
static char current_filename[1024] = "";
static char previous_filename[1024] = "";
static int previous_file_index = 0;
static int current_file_index = 0;
static int pending_start_col = -1;
static int filename_len = 0;
static bool just_created_new_file = false;
static char base_path[1024] = "";
static char current_path[1024] = "";

static int pending_exit_key = 0;

static int browser_selected_index = 0;
static int browser_scroll_offset = 0;

static bool is_mid_text_edit = false;

static char commit_message[256] = "";
static int commit_len = 0;

typedef struct {
    char filename[256];
    time_t last_modified;
    bool is_dir;
} FileEntry;

static FileEntry file_list[MAX_FILES_IN_DIR];
static int total_files_found = 0;

static int pending_bs_start_row = -1; // Spårar raden där raderingen inleddes

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

    // 2. Fyll insidan med vitt (0xF0)
    for (int y = 2; y < box_h - 2; y++) {
        memset(&help_buffer[y * box_w + 2], 0xF0, box_w - 4);
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

    char status_text[1024];
    snprintf(status_text, sizeof(status_text), "Sparad: %s", filename);
    render_status_bar(status_text, target_addr);
}

static void update_status_bar_visuals(UDOUBLE target_addr) {
    char status_text[1050];
    if (filename_len == 0) {
        snprintf(status_text, sizeof(status_text), "[Tryck Enter för att slänga dokumentet]");
    } else {
        snprintf(status_text, sizeof(status_text), "Spara som: %s", current_filename);
    }
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

static bool generate_default_filename(UDOUBLE target_addr) {
    int highest_num = 0;
    char search_path[1024];

    // Bestäm vilken katalog vi ska söka i
    if (strlen(current_path) > 0) {
        snprintf(search_path, sizeof(search_path), "%s", current_path);
    } else {
        snprintf(search_path, sizeof(search_path), "%s", base_path);
    }

    // Öppna katalogen och leta efter det högsta indexet
    DIR *d = opendir(search_path);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            int current_num;
            // Leta efter filnamn som inleds med en siffra följt av _wimwriter
            if (sscanf(dir->d_name, "%d_wimwriter", &current_num) == 1) {
                if (current_num > highest_num) {
                    highest_num = current_num;
                }
            }
        }
        closedir(d);
    }

    int next_num = highest_num + 1;

    if (next_num > 99) {
        clear_filename_buffer();
        return false; // Gränsen är nådd
    }

    // Bygg det nya filnamnet
    if (strlen(current_path) > 0) {
        snprintf(current_filename, sizeof(current_filename), "%s/%02d_wimwriter.md", current_path, next_num);
    } else {
        snprintf(current_filename, sizeof(current_filename), "%02d_wimwriter.md", next_num);
    }

    filename_len = strlen(current_filename);
    return true;
}

// Sortera filer och mappar enligt bokstavsordning
static int compare_file_entries(const void *a, const void *b) {
    FileEntry *entry_a = (FileEntry *)a;
    FileEntry *entry_b = (FileEntry *)b;

    // Enbart alfabetisk sortering
    return strcasecmp(entry_a->filename, entry_b->filename);
}

static const char* get_user_home_dir(void) {
    const char *sudo_user = getenv("SUDO_USER");
    if (sudo_user != NULL) {
        struct passwd *pw = getpwnam(sudo_user);
        if (pw != NULL) return pw->pw_dir;
    } else {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) return pw->pw_dir;
    }
    return getenv("HOME");
}

static void scan_directory_for_files(void) {
    DIR *d;
    struct dirent *dir;
    struct stat file_stat;

    // Sätt basvägen en gång om den är tom
    if (strlen(base_path) == 0) {
        const char *home = get_user_home_dir();
        snprintf(base_path, sizeof(base_path), "%s/Dokument/writer", home);
        snprintf(current_path, sizeof(current_path), "%s", base_path);
    }

    total_files_found = 0;
    current_file_index = 0;

    d = opendir(current_path);
    if (d) {
        while ((dir = readdir(d)) != NULL && total_files_found < MAX_FILES_IN_DIR) {
            // Ignorera dolda filer samt uppåt-navigering (., ..)
            if (dir->d_name[0] == '.') continue;

            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, dir->d_name);

            if (stat(full_path, &file_stat) == 0) {
                bool is_directory = S_ISDIR(file_stat.st_mode);

                // Tillåt endast kataloger, .txt och .md
                if (is_directory || strstr(dir->d_name, ".txt") != NULL || strstr(dir->d_name, ".md") != NULL) {
                    snprintf(file_list[total_files_found].filename, sizeof(file_list[total_files_found].filename), "%s", dir->d_name);
                    file_list[total_files_found].last_modified = file_stat.st_mtime;
                    file_list[total_files_found].is_dir = is_directory;

                    total_files_found++;
                }
            }
        }
        closedir(d);

        if (total_files_found > 0) {
            qsort(file_list, total_files_found, sizeof(FileEntry), compare_file_entries);
        }
    }
}

void open_latest_file(char *text_buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    scan_directory_for_files();

    if (total_files_found > 0) {
        // Inkludera hela sökvägen
        snprintf(current_filename, sizeof(current_filename), "%s/%s", current_path, file_list[0].filename);
        filename_len = strlen(current_filename);

        load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);
    } else {
        clear_filename_buffer();
        just_created_new_file = true;
        render_status_bar("Ny fil. Spara med F2.", target_addr);
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

static void show_file_browser(UDOUBLE target_addr) {
    int box_w = 700;
    int box_h = 500;
    int phys_x = (SCREEN_WIDTH - box_w) / 2;
    int phys_y = (SCREEN_HEIGHT - box_h) / 2;

    static UBYTE browser_buffer[700 * 500];

    // Fyll ramen svart (2px) och insidan med vitt (0xF0 för att passa IT8951 A2-läge)
    memset(browser_buffer, 0x00, sizeof(browser_buffer));
    for (int y = 2; y < box_h - 2; y++) {
        memset(&browser_buffer[y * box_w + 2], 0xF0, box_w - 4);
    }

    int start_local_y = box_h - FONT_H - 40;

    // 1. Rita ut katalogens namn högst upp
    char title_text[1048];
    if (strcmp(current_path, base_path) == 0) {
        snprintf(title_text, sizeof(title_text), "[ Hem ]");
    } else {
        // Leta upp det sista snedstrecket för att enbart visa den aktuella mappens namn
        char *last_slash = strrchr(current_path, '/');
        if (last_slash != NULL) {
            snprintf(title_text, sizeof(title_text), "[ %s ]", last_slash + 1);
        } else {
            snprintf(title_text, sizeof(title_text), "[ %s ]", current_path);
        }
    }

    int title_len = strlen(title_text);
    int current_local_x = box_w - 40 - FONT_W;
    // Kopiera in katalogtexten i bufferten
    for (int c = 0; c < title_len; c++) {
        unsigned char uc = (unsigned char)title_text[c];

        // Fånga upp UTF-8 och konvertera till intern Latin-1
        if (uc == 0xC3 && title_text[c+1] != '\0') {
            unsigned char next_ch = (unsigned char)title_text[c+1];
            if (next_ch == 0xA5) uc = 0xE5;      // å
            else if (next_ch == 0xA4) uc = 0xE4; // ä
            else if (next_ch == 0xB6) uc = 0xF6; // ö
            else if (next_ch == 0x85) uc = 0xC5; // Å
            else if (next_ch == 0x84) uc = 0xC4; // Ä
            else if (next_ch == 0x96) uc = 0xD6; // Ö
            c++; // Hoppa över nästa byte
        }

        if (uc >= 32) {
            const UBYTE *glyph = pre_flipped_glyphs[uc];
            for (int h = 0; h < FONT_H; h++) {
                memcpy(&browser_buffer[(start_local_y + h) * box_w + current_local_x],
                        &glyph[h * FONT_W], FONT_W);
            }
        }
        current_local_x -= FONT_W;
    }

    // 2. Rita understrecket 5 pixlar under texten (2 pixlar tjockt)
    int underline_y = start_local_y + FONT_H + 5;
    int line_start_x = current_local_x + FONT_W; // X-koordinat där texten slutade
    int line_end_x = box_w - 40;

    for (int x = line_start_x; x < line_end_x; x++) {
        browser_buffer[underline_y * box_w + x] = 0x00;       // Linje rad 1
        browser_buffer[(underline_y + 1) * box_w + x] = 0x00; // Linje rad 2
    }

    start_local_y -= (FONT_H + 30); // Skapa marginal ned till filerna

    // 3. Loopa ut filerna
    // Max 24 tecken för filnamnet för att markören ska rymmas i marginalen
    int max_visning = 24;

    for (int i = 0; i < BROWSER_MAX_VISIBLE; i++) {
        int file_idx = browser_scroll_offset + i;
        if (file_idx >= total_files_found) break;

        char display_text[300];

        // Trunkera filnamnet via %.*s
        if (file_idx == browser_selected_index) {
            snprintf(display_text, sizeof(display_text), "-> %.*s", max_visning, file_list[file_idx].filename);
        } else {
            // Tre mellanslag bevarar indenteringen
            snprintf(display_text, sizeof(display_text), "   %.*s", max_visning, file_list[file_idx].filename);
        }

        int len = strlen(display_text);

        // Flytta ut texten i den visuella vänstermarginalen (8 pixlar från kanten).
        // Eftersom skärmen är fysiskt roterad räknar vi från buffertens högerkant.
        int file_local_x = box_w - 8 - FONT_W;

        for (int c = 0; c < len; c++) {
            unsigned char uc = (unsigned char)display_text[c];

            // Fånga upp UTF-8 från filsystemet och konvertera till intern Latin-1
            if (uc == 0xC3 && display_text[c+1] != '\0') {
                unsigned char next_ch = (unsigned char)display_text[c+1];
                if (next_ch == 0xA5) uc = 0xE5;      // å
                else if (next_ch == 0xA4) uc = 0xE4; // ä
                else if (next_ch == 0xB6) uc = 0xF6; // ö
                else if (next_ch == 0x85) uc = 0xC5; // Å
                else if (next_ch == 0x84) uc = 0xC4; // Ä
                else if (next_ch == 0x96) uc = 0xD6; // Ö
                c++; // Hoppa över nästa byte i strängen
            }

            if (uc >= 32) {
                const UBYTE *glyph = pre_flipped_glyphs[uc];
            for (int h = 0; h < FONT_H; h++) {
                memcpy(&browser_buffer[(start_local_y + h) * box_w + file_local_x],
                        &glyph[h * FONT_W],
                        FONT_W);
            }
        }
        file_local_x -= FONT_W;

        start_local_y -= (FONT_H + 15);
    }

    // 4. Skicka hela bufferten till skärmen i A2-läget
    send_and_display_buffer(browser_buffer, phys_x, phys_y, box_w, box_h, target_addr, IT8951_A2_MODE);
}

void restore_hidden_text(char *text_buffer, int cursor_row, int cursor_col, UDOUBLE target_addr) {
    if (!is_mid_text_edit) return;

    int r = cursor_row;
    int c = cursor_col;

    // Läs in tecknen som ligger efter luckan i datamodellen
    for (int i = document_model.gap_end; i < MAX_DOC_SIZE; i++) {
        char ch = document_model.data[i];

        if (ch == '\n') {
            r++;
            c = 0;
        } else {
            BUF_AT(text_buffer, r, c) = ch;
            c++;
            if (c >= MAX_COLS) {
                r++;
                c = 0;
            }
        }

        // Rita inte utanför skärmens dimensioner
        if (r >= MAX_ROWS) break;
    }

    // Uppdatera skärmen tyst via A2-läget
    stitch_and_render_screen(text_buffer, target_addr);
    is_mid_text_edit = false;
}

void editor_handle_idle(int idle_ticks, char *text_buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr) {
    // Om vi har redigerat mitt i texten och tagit en kort paus, rita om skärmen
    if (is_mid_text_edit && idle_ticks >= 2) {
        restore_hidden_text(text_buffer, *cursor_row, *cursor_col, target_addr);
    }
}

static void process_text_input(char c, char *text_buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr, bool more_keys_waiting) {

    unsigned char uc = (unsigned char)c;
    bool is_ctrl_bs = (c == 127 && keyboard_is_ctrl_pressed());

    if (!is_ctrl_bs) {
        if (c == 127) {
            model_delete_char();
        } else if (uc >= 32 || c == '\n') {
            // Radbrytningar (Enter) hanteras alltid som Insert
            if (is_insert_mode || c == '\n') {
                model_insert_char(c);
            } else {
                model_overwrite_char(c);
            }
        }
        append_to_temp_file(c);
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

                    while (*cursor_col > 0 && (BUF_AT(text_buffer, *cursor_row, *cursor_col) == '\0' || BUF_AT(text_buffer, *cursor_row, *cursor_col) == ' ')) {
                        (*cursor_col)--;
                    }

                    if (*cursor_col < MAX_COLS - 1) {
                        (*cursor_col)++;
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
                if (pending_bs_start_row != -1) {
                    editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);
                }

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
    // 1. Flusha framåt (inskriven text)
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

    // 2. Flusha bakåt (raderad text)
    if (pending_bs_start_row != -1) {
        // render_rows_stitched hanterar start och slut automatiskt
        // Den ritar hela radhöjden med vit bakgrund och städar bort artefakter
        render_rows_stitched(cursor_row, pending_bs_start_row, text_buffer, target_addr);
        pending_bs_start_row = -1;
    }
}

static void show_commit_box(UDOUBLE target_addr) {
    int box_w = 700;
    int box_h = 200;
    int phys_x = (SCREEN_WIDTH - box_w) / 2;
    int phys_y = (SCREEN_HEIGHT - box_h) / 2;

    static UBYTE commit_buffer[700 * 200];

    // 1. Fyll ramen svart (2px) och insidan med vitt (0xF0)
    memset(commit_buffer, 0x00, sizeof(commit_buffer));
    for (int y = 2; y < box_h - 2; y++) {
        memset(&commit_buffer[y * box_w + 2], 0xF0, box_w - 4);
    }

    int start_local_y = box_h - FONT_H - 20;

    // 2. Rita rubrik
    const char *title = "[ git commit -m ]";
    int title_len = strlen(title);
    int current_local_x = box_w - 20 - FONT_W;

    for (int c = 0; c < title_len; c++) {
        unsigned char uc = (unsigned char)title[c];
        if (uc >= 32) {
            const UBYTE *glyph = pre_flipped_glyphs[uc];
            for (int h = 0; h < FONT_H; h++) {
                memcpy(&commit_buffer[(start_local_y + h) * box_w + current_local_x],
                       &glyph[h * FONT_W], FONT_W);
            }
        }
        current_local_x -= FONT_W; // Stega fysiskt vänster
    }

    start_local_y -= (FONT_H + 30);

    // 3. Rita inmatad text + prompt
    char display_text[300];
    snprintf(display_text, sizeof(display_text), "> %s_", commit_message);
    int text_len = strlen(display_text);
    int text_local_x = box_w - 20 - FONT_W;

    for (int c = 0; c < text_len; c++) {
        unsigned char uc = (unsigned char)display_text[c];
        if (uc >= 32) {
            const UBYTE *glyph = pre_flipped_glyphs[uc];
            for (int h = 0; h < FONT_H; h++) {
                memcpy(&commit_buffer[(start_local_y + h) * box_w + text_local_x],
                       &glyph[h * FONT_W], FONT_W);
            }
        }
        text_local_x -= FONT_W;
    }

    // Rita hela rutan till skärmen asynkront i A2-läget
    send_and_display_buffer(commit_buffer, phys_x, phys_y, box_w, box_h, target_addr, IT8951_A2_MODE);
}

void editor_shutdown(UDOUBLE target_addr) {
    // 1. Spara filen om den har ett namn, annars generera ett och spara
    if (strlen(current_filename) > 0) {
        save_document_to_file(current_filename);
    } else if (model_get_text_length() > 0) {
        generate_default_filename(target_addr);
        save_document_to_file(current_filename);
    }

    // 2. Synkronisera mot NAS
    sync_to_git("Auto-commit vid nedstängning", target_addr);

    // 3. Rensa skärmen totalt
    // Fyll hela skärmbufferten med vitt (0xF0 för 8bpp)
    memset(full_screen_buffer, 0xF0, SCREEN_WIDTH * SCREEN_HEIGHT);

    // Använd GC16 (Mode 2) för en djupgående rensning av hela panelen
    send_and_display_buffer(full_screen_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, target_addr, 2);

    // 4. Ge det avslutande meddelandet
    // (render_status_bar använder A2-läge och ritar endast i överkanten)
    render_status_bar("Systemet avstängt. Du kan bryta strömmen.", target_addr);

    // Ge IT8951-kontrollern marginal att rita färdigt innan SPI-kommunikationen bryts
    sleep(2);
}

void handle_input(struct input_event *ev, UDOUBLE target_addr, char *text_buffer, int *cursor_row, int *cursor_col, bool more_keys_waiting) {
    if (ev->type != EV_KEY) return;

    char c = keyboard_get_char(ev);

    if(ev->value == 0) return;

    int key_code = ev->code;

    switch (current_state) {

        case STATE_EDITING:
            // Återställ dold text omedelbart om en navigeringstangent trycks ned
            if (is_mid_text_edit &&
                (key_code == KEY_LEFT || key_code == KEY_RIGHT ||
                key_code == KEY_UP || key_code == KEY_DOWN ||
                key_code == KEY_HOME || key_code == KEY_END)) {

                restore_hidden_text(text_buffer, *cursor_row, *cursor_col, target_addr);
            }

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

            if ((c == 's' || c == 'S') && keyboard_is_ctrl_pressed()) {
                if (strlen(current_filename) == 0) {
                    pending_exit_key = 0;
                    is_suggested_name = true;
                    if (generate_default_filename(target_addr)) {
                        update_status_bar_visuals(target_addr);
                    } else {
                        is_suggested_name = false;
                        render_status_bar("Max 99 filer. Ange namn:", target_addr);
                    }
                    current_state = STATE_NAMING_FILE;
                } else {
                    save_to_sd(current_filename, target_addr);
                }
                return;
            }

            if (key_code == KEY_F1) {
                show_help_box(target_addr);
                current_state = STATE_HELP;
            }
            else if (key_code == KEY_F2) {
                if (strlen(current_filename) == 0) {
                    pending_exit_key = KEY_F2;
                    is_suggested_name = true;
                    if (generate_default_filename(target_addr)) {
                        update_status_bar_visuals(target_addr);
                    } else {
                        is_suggested_name = false;
                        render_status_bar("Max 99 filer. Ange namn:", target_addr);
                    }
                    current_state = STATE_NAMING_FILE;
                } else {
                    save_to_sd(current_filename, target_addr);
                }
            }
            else if (key_code == KEY_F3) {
                if (strlen(current_filename) == 0 && model_get_text_length() > 0) {
                    pending_exit_key = KEY_F3;
                    is_suggested_name = true;
                    if (generate_default_filename(target_addr)) {
                        update_status_bar_visuals(target_addr);
                    } else {
                        is_suggested_name = false;
                        render_status_bar("Max 99 filer. Ange namn:", target_addr);
                    }
                    current_state = STATE_NAMING_FILE;
                } else {
                    // Spara den aktuella filen en gång för alla, om den har ett namn
                    if (strlen(current_filename) > 0) {
                        save_document_to_file(current_filename);
                    }

                    if (keyboard_is_shift_pressed()) {
                        if (strlen(previous_filename) > 0) {
                            char temp[1024];
                            strncpy(temp, current_filename, sizeof(temp));

                            // Vi behöver inte spara här igen
                            strncpy(current_filename, previous_filename, sizeof(current_filename));
                            strncpy(previous_filename, temp, sizeof(previous_filename));

                            load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);
                            refresh_display_full(text_buffer, target_addr);
                        }
                    } else {
                        // Öppna filväljaren i bokstavsordning
                        // Skanna katalogen endast när vi faktiskt ska visa filväljaren
                        scan_directory_for_files();
                        browser_selected_index = 0;
                        browser_scroll_offset = 0;
                        show_file_browser(target_addr);
                        current_state = STATE_FILE_BROWSER;
                    }
                }
            }
            else if (key_code == KEY_F4) {
                if (strlen(current_filename) == 0 && model_get_text_length() > 0) {
                    pending_exit_key = KEY_F4;
                    is_suggested_name = true;
                    if (generate_default_filename(target_addr)) {
                        update_status_bar_visuals(target_addr);
                    } else {
                        is_suggested_name = false;
                        render_status_bar("Max 99 filer. Ange namn:", target_addr);
                    }
                    current_state = STATE_NAMING_FILE;
                } else {
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
            }
            else if (key_code == KEY_F5) {
                refresh_display_full(text_buffer, target_addr);
            }
            else if (key_code == KEY_F9) {
                if (keyboard_is_shift_pressed()) {
                    // Manuell Git Pull
                    pull_from_git(target_addr);

                    // Ladda om den aktuella filen från SD-kortet för att fånga upp ändringarna
                    if (strlen(current_filename) > 0) {
                        load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);

                        // Rita upp skärmen på nytt i GC16-läge
                        refresh_display_full(text_buffer, target_addr);

                        char status_msg[300];
                        snprintf(status_msg, sizeof(status_msg), "Uppdaterad: %s", current_filename);
                        render_status_bar(status_msg, target_addr);
                    } else {
                        render_status_bar("Synkronisering (pull) slutförd.", target_addr);
                    }
                } else {
                    // Standard Git Commit & Push
                    commit_len = 0;
                    memset(commit_message, 0, sizeof(commit_message));
                    show_commit_box(target_addr);
                    current_state = STATE_GIT_COMMIT;
                }
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
            else if (key_code == KEY_INSERT) {
                is_insert_mode = !is_insert_mode;

                if (is_insert_mode) {
                    render_status_bar("Insert", target_addr);
                } else {
                    // Städar undan "Insert" när vi växlar till Overwrite-läge
                    hide_status_bar_and_redraw(target_addr);
                }
            }
            else if (key_code == KEY_LEFT) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                if (document_model.gap_start > 0) {
                    model_move_cursor_left();

                    // Uppdatera de visuella koordinaterna bakåt
                    if (*cursor_col > 0) {
                        (*cursor_col)--;
                    } else if (*cursor_row > 0) {
                        (*cursor_row)--;
                        *cursor_col = MAX_COLS - 1;
                    }

                    // Stega förbi eventuell tom utfyllnad (padding) från ordflätningen
                    while ((*cursor_row > 0 || *cursor_col > 0) &&
                            BUF_AT(text_buffer, *cursor_row, *cursor_col) == ' ') {
                        if (*cursor_col > 0) {
                            (*cursor_col)--;
                        } else if (*cursor_row > 0) {
                            (*cursor_row)--;
                            *cursor_col = MAX_COLS - 1;
                        }
                    }

                    // Stäng av prompten tillfälligt om den var synlig
                    if (prompt_visible) {
                        render_char(' ', prompt_px, prompt_py, target_addr);
                        prompt_visible = false;
                    }
                }
            }
            else if (key_code == KEY_RIGHT) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                if (document_model.gap_end < MAX_DOC_SIZE) {
                    model_move_cursor_right();

                    // Uppdatera de visuella koordinaterna framåt
                    (*cursor_col)++;
                    if (*cursor_col >= MAX_COLS) {
                        *cursor_col = 0;
                        (*cursor_row)++;
                    }

                    // Stäng av prompten tillfälligt
                    if (prompt_visible) {
                        render_char(' ', prompt_px, prompt_py, target_addr);
                        prompt_visible = false;
                    }
                }
            }
            else if (key_code == KEY_END && keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                // Flytta luckan hela vägen till slutet i minnet
                while (document_model.gap_end < MAX_DOC_SIZE) {
                    model_move_cursor_right();
                }

                // Läs om skärmbufferten för att synkronisera vy och modell
                load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);
                refresh_display_full(text_buffer, target_addr);
            }
            else if (key_code == KEY_HOME && keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                // Flytta luckan hela vägen till början i minnet
                while (document_model.gap_start > 0) {
                    model_move_cursor_left();
                }

                // Läs om skärmbufferten och uppdatera
                load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);
                refresh_display_full(text_buffer, target_addr);
            }
            else if (key_code == KEY_HOME && !keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                // Backa tills vi når kolumn noll
                while (*cursor_col > 0 && document_model.gap_start > 0) {
                    model_move_cursor_left();
                    (*cursor_col)--;
                }

                if (prompt_visible) {
                    render_char(' ', prompt_px, prompt_py, target_addr);
                    prompt_visible = false;
                }
            }
            else if (key_code == KEY_END && !keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                // Gå framåt tills vi träffar en radbrytning eller slutet på skärmbredden
                while (*cursor_col < MAX_COLS - 1 && document_model.gap_end < MAX_DOC_SIZE) {
                    char next_c = model_char_at(document_model.gap_start);
                    if (next_c == '\n') break;

                    model_move_cursor_right();
                    (*cursor_col)++;
                }

                if (prompt_visible) {
                    render_char(' ', prompt_px, prompt_py, target_addr);
                    prompt_visible = false;
                }
            }
            else if (key_code == KEY_UP) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                if (*cursor_row > 0 && document_model.gap_start > 0) {
                    int target_col = *cursor_col;
                    int start_row = *cursor_row;

                    // Stega bakåt tills vi når raden ovanför
                    while (*cursor_row == start_row && document_model.gap_start > 0) {
                        model_move_cursor_left();

                        if (*cursor_col > 0) {
                            (*cursor_col)--;
                        } else {
                            (*cursor_row)--;
                            *cursor_col = MAX_COLS - 1;
                        }

                        // Stega logiskt förbi eventuell tom utfyllnad (word wrap-padding)
                        while ((*cursor_row > 0 || *cursor_col > 0) &&
                                BUF_AT(text_buffer, *cursor_row, *cursor_col) == ' ') {
                            if (*cursor_col > 0) {
                                (*cursor_col)--;
                            } else {
                                (*cursor_row)--;
                                *cursor_col = MAX_COLS - 1;
                            }
                        }
                    }

                    // Nu är vi på slutet av raden ovanför. Backa tills vi når rätt kolumn.
                    while (*cursor_col > target_col && document_model.gap_start > 0) {
                        char prev_c = model_char_at(document_model.gap_start - 1);
                        if (prev_c == '\n') break;

                        model_move_cursor_left();
                        (*cursor_col)--;
                    }

                    if (prompt_visible) {
                        render_char(' ', prompt_px, prompt_py, target_addr);
                        prompt_visible = false;
                    }
                }
            }
            else if (key_code == KEY_DOWN) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                int target_col = *cursor_col;
                int start_row = *cursor_row;

                // Stega framåt tills vi når nästa rad
                while (*cursor_row == start_row && document_model.gap_end < MAX_DOC_SIZE) {
                    char next_c = model_char_at(document_model.gap_start);
                    model_move_cursor_right();

                    if (next_c == '\n') {
                        (*cursor_row)++;
                        *cursor_col = 0;
                    } else {
                        (*cursor_col)++;
                        if (*cursor_col >= MAX_COLS) {
                            (*cursor_row)++;
                            *cursor_col = 0;
                        }
                    }
                }

                // Stega framåt på den nya raden tills vi når target_col eller slutet av raden
                while (*cursor_col < target_col && document_model.gap_end < MAX_DOC_SIZE) {
                    char next_c = model_char_at(document_model.gap_start);
                    if (next_c == '\n') break;

                    model_move_cursor_right();
                    (*cursor_col)++;
                }

                if (prompt_visible) {
                    render_char(' ', prompt_px, prompt_py, target_addr);
                    prompt_visible = false;
                }
            }
            else if (key_code == KEY_LEFT && keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                bool found_word = false;

                while (document_model.gap_start > 0) {
                    char prev_c = model_char_at(document_model.gap_start - 1);

                    // Identifiera om vi har hittat ett ord, eller om vi ska stanna
                    if (prev_c != ' ' && prev_c != '\n') {
                        found_word = true;
                    } else if (found_word) {
                        break;
                    }

                    model_move_cursor_left();

                    if (*cursor_col > 0) {
                        (*cursor_col)--;
                    } else if (*cursor_row > 0) {
                        (*cursor_row)--;
                        *cursor_col = MAX_COLS - 1;
                    }

                    // Hantera eventuell utfyllnad från word-wrap i skärmbufferten
                    while ((*cursor_row > 0 || *cursor_col > 0) &&
                            BUF_AT(text_buffer, *cursor_row, *cursor_col) == ' ' &&
                            model_char_at(document_model.gap_start) != ' ') {

                        if (*cursor_col > 0) {
                            (*cursor_col)--;
                        } else if (*cursor_row > 0) {
                            (*cursor_row)--;
                            *cursor_col = MAX_COLS - 1;
                        }
                    }
                }

                if (prompt_visible) {
                    render_char(' ', prompt_px, prompt_py, target_addr);
                    prompt_visible = false;
                }
            }
            else if (key_code == KEY_RIGHT && keyboard_is_ctrl_pressed()) {
                editor_flush_queue(text_buffer, *cursor_row, *cursor_col, target_addr);

                bool found_word = false;

                while (document_model.gap_end < MAX_DOC_SIZE) {
                    char next_c = model_char_at(document_model.gap_start);

                    // Identifiera ordgränsen framåt
                    if (next_c != ' ' && next_c != '\n') {
                        found_word = true;
                    } else if (found_word) {
                        break;
                    }

                    model_move_cursor_right();

                    if (next_c == '\n') {
                        (*cursor_row)++;
                        *cursor_col = 0;
                    } else {
                        (*cursor_col)++;
                        if (*cursor_col >= MAX_COLS) {
                            (*cursor_row)++;
                            *cursor_col = 0;
                        }
                    }
                }

                if (prompt_visible) {
                    render_char(' ', prompt_px, prompt_py, target_addr);
                    prompt_visible = false;
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

        case STATE_FILE_BROWSER:
            if (key_code == KEY_ESC) {
                hide_help_box_and_redraw(text_buffer, target_addr);
                current_state = STATE_EDITING;
            }
            else if (key_code == KEY_DOWN) {
                // Byt fil nedåt
                if (browser_selected_index < total_files_found - 1) {
                    browser_selected_index++;
                    if (browser_selected_index >= browser_scroll_offset + BROWSER_MAX_VISIBLE) {
                        browser_scroll_offset += BROWSER_MAX_VISIBLE;
                    }
                    show_file_browser(target_addr);
                }
            }
            else if (key_code == KEY_UP) {
                // Byt fil uppåt
                if (browser_selected_index > 0) {
                    browser_selected_index--;
                    if (browser_selected_index < browser_scroll_offset) {
                        browser_scroll_offset -= BROWSER_MAX_VISIBLE;
                        if (browser_scroll_offset < 0) browser_scroll_offset = 0;
                    }
                    show_file_browser(target_addr);
                }
            }
            else if (key_code == KEY_RIGHT) {
                // Stega in i katalogen om markören står på en mapp
                if (total_files_found > 0 && file_list[browser_selected_index].is_dir) {
                    char temp_path[1024];
                    snprintf(temp_path, sizeof(temp_path), "%s/%s", current_path, file_list[browser_selected_index].filename);
                    snprintf(current_path, sizeof(current_path), "%s", temp_path);

                    scan_directory_for_files();
                    browser_selected_index = 0;
                    browser_scroll_offset = 0;
                    show_file_browser(target_addr);
                }
            }
            else if (key_code == KEY_LEFT) {
                // Stega uppåt, men stoppa om vi redan är i 'Hem' (base_path)
                if (strcmp(current_path, base_path) != 0) {
                    char *last_slash = strrchr(current_path, '/');

                    if (last_slash != NULL && last_slash != current_path) {
                        *last_slash = '\0'; // Klipp strängen vid sista snedstrecket
                    } else {
                        // Säkerhetsnät om strängen är skadad
                        snprintf(current_path, sizeof(current_path), "%s", base_path);
                    }

                    scan_directory_for_files();
                    browser_selected_index = 0;
                    browser_scroll_offset = 0;
                    show_file_browser(target_addr);
                }
            }
            else if (c == '\n') {
                if (total_files_found > 0) {
                    if (file_list[browser_selected_index].is_dir) {
                        // Behandla Enter på en katalog exakt som Pil Höger
                        char temp_path[1024];
                        snprintf(temp_path, sizeof(temp_path), "%s/%s", current_path, file_list[browser_selected_index].filename);
                        snprintf(current_path, sizeof(current_path), "%s", temp_path);

                        scan_directory_for_files();
                        browser_selected_index = 0;
                        browser_scroll_offset = 0;
                        show_file_browser(target_addr);
                    } else {
                        // Det är en textfil. Ladda in den med absolut sökväg.
                        snprintf(current_filename, sizeof(current_filename), "%s/%s", current_path, file_list[browser_selected_index].filename);
                        filename_len = strlen(current_filename);

                        load_file_into_buffer(current_filename, text_buffer, cursor_row, cursor_col, target_addr);

                        refresh_display_full(text_buffer, target_addr);
                        current_state = STATE_EDITING;
                    }
                }
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
                        if (filename_len == 0) {
                            // Inget namn angett, trigga raderingsfrågan
                            render_status_bar("Slänga det osparade dokumentet? (J/N)", target_addr);
                            current_state = STATE_CONFIRM_DISCARD;
                        }
                        else if (access(current_filename, F_OK) == 0) {
                            char warning_text[1024];
                            snprintf(warning_text, sizeof(warning_text), "Skriv över fil: '%s'?", current_filename);
                            render_status_bar(warning_text, target_addr);
                            current_state = STATE_CONFIRM_OVERWRITE;
                        } else {
                            save_to_sd(current_filename, target_addr);

                            // Fånga upp om vi var på väg någonstans
                            if (pending_exit_key == KEY_F3) {
                                scan_directory_for_files();
                                browser_selected_index = 0;
                                browser_scroll_offset = 0;
                                show_file_browser(target_addr);
                                current_state = STATE_FILE_BROWSER;
                            } else if (pending_exit_key == KEY_F4) {
                                strncpy(previous_filename, current_filename, sizeof(previous_filename));
                                model_init();
                                memset(text_buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
                                *cursor_row = JUMP_LINES;
                                *cursor_col = 0;
                                clear_filename_buffer();
                                just_created_new_file = true;
                                stitch_and_render_screen(text_buffer, target_addr);
                                render_status_bar("Ny fil.", target_addr);
                                current_state = STATE_EDITING;
                            } else {
                                hide_status_bar_and_redraw(target_addr);
                                current_state = STATE_EDITING;
                            }
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

            case STATE_CONFIRM_DISCARD:
                if (c == 'j' || c == 'J') {
                    // Töm dokumentet
                    model_init();
                    memset(text_buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
                    *cursor_row = JUMP_LINES;
                    *cursor_col = 0;
                    clear_filename_buffer();
                    just_created_new_file = true;

                    if (pending_exit_key == KEY_F3) {
                        scan_directory_for_files();
                        if (total_files_found > 0) {
                            browser_selected_index = 0;
                            browser_scroll_offset = 0;
                            show_file_browser(target_addr);
                            current_state = STATE_FILE_BROWSER;
                        } else {
                            stitch_and_render_screen(text_buffer, target_addr);
                            current_state = STATE_EDITING;
                        }
                    } else {
                        stitch_and_render_screen(text_buffer, target_addr);
                        render_status_bar("Dokumentet slängdes. Ny fil.", target_addr);
                        current_state = STATE_EDITING;
                    }
                    pending_exit_key = 0;
                }
                else if (c == 'n' || c == 'N') {
                    is_suggested_name = true;
                    if (generate_default_filename(target_addr)) {
                        update_status_bar_visuals(target_addr);
                    } else {
                        is_suggested_name = false;
                        render_status_bar("Max 99 filer. Ange namn:", target_addr);
                    }
                    current_state = STATE_NAMING_FILE;
                }
                else if (key_code == KEY_ESC) {
                    hide_status_bar_and_redraw(target_addr);
                    current_state = STATE_EDITING;
                }
                break;

            case STATE_GIT_COMMIT:
                if (c > 0) {
                    if (c == '\n') {
                        // 1. Städa bort rutan från skärmen
                        hide_help_box_and_redraw(text_buffer, target_addr);
                        current_state = STATE_EDITING;

                        // 2. Tvätta texten och påbörja synkningen
                        char safe_msg[256];
                        sanitize_string(commit_message, safe_msg, sizeof(safe_msg));
                        sync_to_git(safe_msg, target_addr);
                    }
                    else if (c == 127) {
                        // Backspace
                        if (commit_len > 0) {
                            commit_len--;
                            commit_message[commit_len] = '\0';
                            show_commit_box(target_addr);
                        }
                    }
                    else {
                        // Standard inmatning
                        if (commit_len < sizeof(commit_message) - 1) {
                            commit_message[commit_len] = c;
                            commit_len++;
                            commit_message[commit_len] = '\0';
                            show_commit_box(target_addr);
                        }
                    }
                }
                else if (key_code == KEY_ESC) {
                    // Avbryt synk
                    hide_help_box_and_redraw(text_buffer, target_addr);
                    current_state = STATE_EDITING;
                }
                break;

        } // Avslutar switch (current_state)

        // Felsökningsutskrift i terminalen
        if (c == '\n') {
            putchar('\n');
        } else if (c == 127) {
            printf("\b \b");
        } else if (c >= 32 && c <= 126) {
            putchar(c);
        } else if ((unsigned char)c == 0xE5) { printf("\xC3\xA5"); } // å
        else if ((unsigned char)c == 0xE4) { printf("\xC3\xA4"); } // ä
        else if ((unsigned char)c == 0xF6) { printf("\xC3\xB6"); } // ö
        else if ((unsigned char)c == 0xC5) { printf("\xC3\x85"); } // Å
        else if ((unsigned char)c == 0xC4) { printf("\xC3\x84"); } // Ä
        else if ((unsigned char)c == 0xD6) { printf("\xC3\x96"); } // Ö
        fflush(stdout);

    } // Avslutar funktionen handle_input
