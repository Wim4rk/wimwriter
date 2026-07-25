#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include "display.h"
#include "keyboard.h"
#include "firmware/display.h"

#define KEYBOARD_DEVICE "/dev/input/event0"

#define SCREEN_WIDTH 1448
#define SCREEN_HEIGHT 1072

#define MARGIN_LEFT 68
#define MARGIN_RIGHT 68
#define MARGIN_TOP 44
#define MARGIN_BOTTOM 68

#define MAX_COLS 41
#define MAX_ROWS 15

char text_buffer[MAX_ROWS][MAX_COLS];

// Helper function. Calculate physical X-coord on screen
int get_physical_x(int col){
    return MARGIN_LEFT + (col * GLYPH_W);
}

// Helper function. Calculate physical Y-coord on screen
int get_physical_y(int row) {
    return MARGIN_TOP + (row * GLYPH_H);
}

// Helper function. Clear logic buffer
void clear_buffer() {
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            text_buffer[r][c] = ' ';
        }
    }
}

// Helper function: Redraw complete buffer to screen (Used after Jump)
void redraw_buffer(UDOUBLE target_addr) {
    // TODO: Här behövs ett anrop till EPD_IT8951_Clear_Refresh() göras för
    // att rensa skärmen med DU- (eller INIT-läge) för att städa ghosting
    
    for (int r = 0; r < MAX_ROWS; r++) {
        for (int c = 0; c < MAX_COLS; c++) {
            if (text_buffer[r][c] != ' ') {
                render_char(text_buffer[r][c], get_physical_x(c), get_physical_y(r), target_addr);
            }
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
                    // Save to buffer and render over SPI
                    text_buffer[cursor_row][cursor_col] = c;
                    printf("Ritar tecken '%c' på col: %d, row: %d (X: %d, Y: %d)\n",
                            c, cursor_col, cursor_row, get_physical_x(cursor_col), get_physical_y(cursor_row));
                    // render_char(c, get_physical_x(cursor_col), get_physical_y(cursor_row), target_addr);

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
                    // 1. Move 5 bottom rows to top 5 rows
                    for (int r = 0; r < 5; r++) {
                        for (int col = 0; col < MAX_COLS; col++) {
                            text_buffer[r][col] = text_buffer[MAX_ROWS - 5 + r][col];                       
                        }
                    }

                    // 2. Empty rest of buffer (row 6 - 15)
                    for (int r = 5; r < MAX_ROWS; r++) {
                        for (int col = 0; col < MAX_COLS; col++) {
                            text_buffer[r][col] = ' ';
                        }
                    }

                    // 3. Move pointer to row 6
                    cursor_row = 5;
                    cursor_col = 0;

                    // 4. Redraw screen with new buffer
                    redraw_buffer(target_addr);
                }
            }
        }
    }

    keyboard_close(kb_fd);
    cleanup_display();
    return 0;
}
