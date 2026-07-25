#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include "firmware/display.h"
#include "keyboard.h"

#define KEYBOARD_DEVICE "/dev/input/event0"

char text_buffer[MAX_ROWS][MAX_COLS];

// Helper function. Clear logic buffer
void clear_buffer() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            text_buffer[r][c] = ' ';
        }
    }
}

// Helper function. Clear logic buffer
void clear_buffer() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            text_buffer[r][c] = ' ';
        }
    }
}

int main() {
    UDOUBLE target_addr;

    printf("Initierar IT8951-display via SPI...\n");
    init_display(&target_addr);

    printf("Kopplar upp tangentbord\n");
    int kb_fd = keyboard_init(KEYBOARD_DEVICE);
    if (kb_fd < 0) {
        printf("Kunde inte öppna tangentbordet (sudo?).\n");
        cleanup_display();
        return 1;
    }

    clear_buffer();

    // Startkoordinater
    int cursor_col = 0;
    int cursor_row = 5;

    printf("WimWriter redo.\n");

    struct input_event ev;

    // Main loop - CPU rests in read() until interrupt from keyboard
    while (1) {
        if (read(kb_fd, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_KEY) {

                // Hämta tecken från tabell
                char c = keyboard_get_char(&ev);


                if (c == '\n') { //Enter
                    cursor_col = 0;
                    cursor_row++;
                }
                else if (c == 127) { // Backspace
                    cursor_col--;
                        if (cursor_col < 0) {
                            if (cursor_row > 0) {
                                cursor_row--;
                                cursor_col = MAX_COLS - 1;
                            } else {
                                cursor_col = 0;
                            }
                        }
                        // Overwrite buffer and screen with ' '
                        text_buffer[cursor_row][cursor_col] = ' ';
                        render_char(' ', get_physical_x(cursor_col), get_physical_y(cursor_row), target_addr);
                }
                else if (c > 0) { // Regular chars
                    int px = get_physical_x(cursor_col);
                    int py = get_physical_y(cursor_row);

                    text_buffer[cursor_row][cursor_col] = c;
                    render_char(c, px, py, target_addr);

                    cursor_col++;

                    // Word wrapping
                    if (cursor_col >= MAX_COLS) {
                        cursor_col = 0;
                        cursor_row++;
                    }
                }

                // Jump mechanism
                if (cursor_row >= MAX_ROWS) {
                    display_jump(text_buffer, &cursor_row, &cursor_col, target_addr);
                }
            }
        }
    }

    keyboard_close(kb_fd);
    cleanup_display();
    return 0;
}
