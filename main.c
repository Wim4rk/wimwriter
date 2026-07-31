#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include <sys/epoll.h> // Krävs för asynkron inmatning
#include <fcntl.h>
#include <string.h>

#include "firmware/display.h"
#include "firmware/keyboard.h"
#include "software/editor.h" // Inkludera vår nya tillståndsmaskin

#define KEYBOARD_DEVICE "/dev/input/event0"
#define MAX_EVENTS 5

// Allokera en fast minnesyta som rymmer den minsta tänkbara fonten
char text_buffer[ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS];

// Helper function. Clear logic buffer
void clear_buffer() {
    memset(text_buffer, ' ', ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS);
}

// Hjälpfunktion för att sätta filbeskrivaren i icke-blockerande läge
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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

    // Aktivera icke-blockerande I/O för tangentbordet
    set_nonblocking(kb_fd);

    printf("Rensar buffer...\n");

    clear_buffer();

    // Startkoordinater anpassade efter jump-logiken
    int cursor_col = 0;
    int cursor_row = JUMP_LINES;

    // Initiera epoll
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev_epoll, events[MAX_EVENTS];
    ev_epoll.events = EPOLLIN;
    ev_epoll.data.fd = kb_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, kb_fd, &ev_epoll);

    printf("WimWriter redo!\n");

    struct input_event ev;
    bool prompt_visible = false;
    int prompt_px = 0, prompt_py = 0;
    int epoll_timeout_ms = 300; // Kort timeout för catch-up
    int prompt_delay_ticks = 3; // 3 * 300 ms = 900 ms inaktivitet innan prompt
    int current_idle_ticks = 0;

    // Main loop - CPU rests in read() until interrupt from keyboard
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, epoll_timeout_ms);

        if (nfds == 0) {
            // TIMEOUT: inaktivitet har passerat
            // Spola kön!
            editor_flush_queue(text_buffer, cursor_row, cursor_col, target_addr);

            current_idle_ticks++;
            if (current_idle_ticks >= prompt_delay_ticks && !prompt_visible) {
                prompt_px = get_physical_x(cursor_col);
                prompt_py = get_physical_y(cursor_row);

                // Rita ut understrecket i A2-läget
                render_char('_', prompt_px, prompt_py, target_addr);
                prompt_visible = true;

                // TODO: Eventuellt trigga spolning till temp-fil här
            }
        } else if (nfds > 0) {
            current_idle_ticks = 0;
            for (int n = 0; n < nfds; n++) {
                if (events[n].data.fd == kb_fd) {
                    // Läs tömmer bufferten tills EAGAIN returneras
                    // Läs in alla väntande events i en stöt
                    struct input_event ev_queue[64];
                    int ev_count = 0;

                    while (read(kb_fd, &ev_queue[ev_count], sizeof(struct input_event)) > 0) {
                        if (ev_queue[ev_count].type == EV_KEY) {
                            ev_count++;
                            if (ev_count >= 64) break; // Säkerhet mot overflow
                        }
                    }

                    // Processa dem och låt motorn veta om fler tangenter väntar
                    for (int i = 0; i < ev_count; i++) {
                        if (prompt_visible) {
                            render_char(' ', prompt_px, prompt_py, target_addr);
                            prompt_visible = false;
                        }

                        bool more_keys = (i < ev_count - 1); // True för alla utom sista
                        handle_input(&ev_queue[i], target_addr, text_buffer, &cursor_row, &cursor_col, more_keys);
                    }
                }
            }
        }
    }

    close(epoll_fd);
    keyboard_close(kb_fd);
    cleanup_display();
    return 0;
}
