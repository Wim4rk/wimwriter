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

// Allokera en fast minnesyta som rymmer den minsta tänkbara fonten
char text_buffer[ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS];

// Helper function. Clear logic buffer
void clear_buffer() {
    memset(text_buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
}

int main() {
    UDOUBLE target_addr;

    printf("Initierar IT8951-display via SPI...\n");
    init_display(&target_addr);

    printf("Initierar font...\n");
    set_active_font(2);
    calculate_layout_points(current_font.width, current_font.height);

    printf("Kopplar upp tangentbord...\n");
    int kb_fd = keyboard_init(KEYBOARD_DEVICE);
    if (kb_fd < 0) {
        printf("Kunde inte öppna tangentbordet (sudo?).\n");
        cleanup_display();
        return 1;
    }

    printf("Rensar buffer...\n");

    clear_buffer();

    // Startkoordinater anpassade efter jump-logiken
    int cursor_col = 0;
    int cursor_row = JUMP_LINES;

    // Konfigurera poll
    struct pollfd fds[1];
    fds[0].fd = kb_fd;
    fds[0].events = POLLIN;

    printf("WimWriter redo!\n");

    struct input_event ev;

    bool prompt_visible = false;
    int prompt_px = 0;
    int prompt_py = 0;
    int prompt_timeout_ms = 800;

    // Main loop - CPU rests in read() until interrupt from keyboard
    while (1) {
        int ret = poll(fds, 1, prompt_timeout_ms);

        if (ret == 0) {
            // TIMEOUT: inaktivitet har passerat
            if (!prompt_visible) {
                prompt_px = get_physical_x(cursor_col);
                prompt_py = get_physical_y(cursor_row);

                // Rita ut understrecket i snabba A2-läget
                render_char('_', prompt_px, prompt_py, target_addr);
                prompt_visible = true;
            }
        } else if (ret > 0) {
            // HÄNDELSE: Data finns att läsa från tangentbordet
            if (fds[0].revents & POLLIN) {
                if (read(kb_fd, &ev, sizeof(ev)) > 0) {
                    if (ev.type == EV_KEY) {

                        // Städa bort prompten om den är synlig

                        if (prompt_visible) {
                            render_char(' ', prompt_px, prompt_py, target_addr);
                            prompt_visible = false;
                        }

                        //Kika om det finns fler tecken i kön?
                        int peek = poll(fds, 1, 0);
                        bool more_keys_waiting = (peek > 0);

                        // Skicka tangenttryckningen och nuvarande state till editor.c
                        handle_input(&ev, target_addr, text_buffer, &cursor_row, &cursor_col, more_keys_waiting);
                    }
                }
            }
        }
    }

    keyboard_close(kb_fd);
    cleanup_display();
    return 0;
}
