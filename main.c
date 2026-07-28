#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include <poll.h> // Krävs för asynkron inmatning
#include "firmware/display.h"
#include "firmware/keyboard.h"
#include "software/editor.h" // Inkludera vår nya tillståndsmaskin

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

    // Startkoordinater anpassade efter jump-logiken
    int cursor_col = 0;
    int cursor_row = JUMP_LINES;

    // Tillstånd för prompten
    bool prompt_visible = false;
    int prompt_timeout_ms = 500; // Halv sekund innan prompten ritas ut

    // Konfigurera poll
    struct pollfd fds[1];
    fds[0].fd = kb_fd;
    fds[0].events = POLLIN;

    printf("WimWriter redo.\n");

    struct input_event ev;

    // Main loop - CPU rests in read() until interrupt from keyboard
    while (1) {
        int ret = poll(fds, 1, prompt_timeout_ms);

        if (ret == 0) {
            // TIMEOUT: 500 ms av inaktivitet har passerat
            if (!prompt_visible) {
                int px = get_physical_x(cursor_col);
                int py = get_physical_y(cursor_row);

                // Rita ut understrecket i snabba A2-läget
                render_char('_', px, py, target_addr);
                prompt_visible = true;
            }
        } else if (ret > 0) {
            // HÄNDELSE: Data finns att läsa från tangentbordet
            if (fds[0].revents & POLLIN) {
                if (read(kb_fd, &ev, sizeof(ev)) > 0) {
                    if (ev.type == EV_KEY) {

                        // Städa bort prompten om den är synlig innan nästa tecken hanteras
                        if (prompt_visible) {
                            int px = get_physical_x(cursor_col);
                            int py = get_physical_y(cursor_row);

                            render_char(' ', px, py, target_addr);
                            prompt_visible = false;
                        }

                        // Skicka tangenttryckningen och nuvarande state till editor.c
                        handle_input(&ev, target_addr, text_buffer, &cursor_row, &cursor_col);
                    }
                }
            }
        }
    }

    keyboard_close(kb_fd);
    cleanup_display();
    return 0;
}
