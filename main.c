#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/input.h>
#include <sys/epoll.h> // Krävs för asynkron inmatning
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <bcm2835.h>
#include <signal.h>

#include "firmware/display.h"
#include "firmware/keyboard.h"
#include "software/editor.h"
#include "software/model.h"

#define SHUTDOWN_BTN_PIN 26 // Ändra till den BCM-pinne du valt
#define KEYBOARD_DEVICE "/dev/input/event0"
#define MAX_EVENTS 5

// Globala referenser för säker nedstängning
static int global_kb_fd = -1;
static int global_epoll_fd = -1;
static UDOUBLE global_target_addr;

// Allokera en fast minnesyta som rymmer den minsta tänkbara fonten
char text_buffer[ABSOLUTE_MAX_ROWS * ABSOLUTE_MAX_COLS];

void handle_sigint(int sig) {
    printf("\nAvslutar programmet säkert. Frigör SPI och GPIO...\n");
    if (global_epoll_fd != -1) close(global_epoll_fd);
    if (global_kb_fd != -1) keyboard_close(global_kb_fd);
    cleanup_display();
    exit(0);
}

void handle_shutdown(int sig) {
    // Kör editorns spara- och synksekvens
    editor_shutdown(global_target_addr);

    // Städa upp gränssnitt och hårdvara
    if (global_epoll_fd != -1) close(global_epoll_fd);
    if (global_kb_fd != -1) keyboard_close(global_kb_fd);
    cleanup_display();

    // Stäng av operativsystemet mjukt
    system("sudo poweroff");
    exit(0);
}

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

void dump_glyph_to_terminal(unsigned char uc) {
    printf("Visualiserar tecken: %c (0x%02X)\n", uc, uc);

    // Hämta arrayen för tecknet (justera variabelnamnet utifrån din fontfil)
    const uint8_t *glyph = wim_font_24x43[uc];

    // Iterera över rutans höjd och bredd
    for (int y = 0; y < 43; y++) {
        for (int x = 0; x < 24; x++) {
            uint8_t pixel = glyph[y * 24 + x];

            // 0x00 är svart pigment. Allt annat tolkar vi som vitt/bakgrund.
            if (pixel == 0x00) {
                printf("##");
            } else {
                printf("  ");
            }
        }
        printf("\n");
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    UDOUBLE target_addr;

    // Stäng av maskinen med tryckknappen
    bcm2835_gpio_fsel(SHUTDOWN_BTN_PIN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(SHUTDOWN_BTN_PIN, BCM2835_GPIO_PUD_UP);

    printf("Initierar IT8951-display via SPI...\n");
    init_display(&target_addr);

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
    model_init();

    // Startkoordinater anpassade efter jump-logiken
    int cursor_col = 0;
    int cursor_row = JUMP_LINES;

    refresh_display_full(text_buffer, target_addr);

    render_status_bar("Öppnar senaste fil...", target_addr);
    printf("Öppnar senaste fil...\n");
    open_latest_file(text_buffer, &cursor_row, &cursor_col, target_addr);

    // Initiera epoll
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev_epoll, events[MAX_EVENTS];
        ev_epoll.events = EPOLLIN;
        ev_epoll.data.fd = global_kb_fd;
        epoll_ctl(global_epoll_fd, EPOLL_CTL_ADD, global_kb_fd, &ev_epoll);

    bool prompt_visible = false;
    int prompt_px = 0, prompt_py = 0;
    int epoll_timeout_ms = 300; // Kort timeout för catch-up
    int prompt_delay_ticks = 3; // 3 * 300 ms = 900 ms inaktivitet innan prompt
    int current_idle_ticks = 0;

    render_status_bar("WimWriter redo!", target_addr);
    printf("WimWriter redo!\n");

    // Main loop - CPU rests in read() until interrupt from keyboard
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, epoll_timeout_ms);

        if (bcm2835_gpio_lev(SHUTDOWN_BTN_PIN) == LOW) {
            printf("\nPåbörjar säker nedstängning...\n");
            handle_sigint(0); // Återanvänder städ-funktionen
        }

        if (nfds == 0) {
            // TIMEOUT: inaktivitet har passerat
            // Spola kön!
            editor_flush_queue(text_buffer, cursor_row, cursor_col, target_addr);

            // 1. UTSTÄDAD KONTROLL FÖR STATUSRADEN
            if (status_bar_visible) {
                time_t current_time = time(NULL);
                if (difftime(current_time, status_bar_timestamp) >= 8.0) {
                    hide_status_bar_and_redraw(target_addr);
                }
            }

            // 2. HANTERA PROMPTEN OBEROENDE AV STATUSRADEN
            current_idle_ticks++;
            if (current_idle_ticks >= prompt_delay_ticks && !prompt_visible) {
                prompt_px = get_physical_x(cursor_col);
                prompt_py = get_physical_y(cursor_row);

                // Rita ut understrecket i A2-läget
                render_char('_', prompt_px, prompt_py, target_addr);
                prompt_visible = true;
            }
        }
        else if (nfds > 0) {
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
