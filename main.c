#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>

#include "firmware/display.h"

// Temporär hantering för evdev tills vidare...
char map_keycode_to_char(int code) {
    if (code >= KEY_Q && code <= KEY_P) {
        const char qwerty[] = "0000000000000000qwertyuiop000000000asdfghjkl00000zxcvbnm";
        if (code < sizeof(qwerty)) return qwerty[code];
    }
    if (code == KEY_SPACE) return ' ';
    return -1;
}

int main() {
    int cursor_x = 100;
    int cursor_y = 100;
    UDOUBLE target_addr;

    init_display(&target_addr);

    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd == -1) {
        printf("Kunde inte öppna tangentbordet (sudo?).\n");
        cleanup_display();
        return 1;
    }

    struct input_event ev;

    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {

            char c = map_keycode_to_char(ev.code);

            if (c != -1) {
                render_char(c, cursor_x, cursor_y, target_addr);
                cursor_x += GLYPH_W;

                // Grundläggande radbrytning (vi fixar detta mer senare i software/editor.c)
                if(cursor_x > (1448 - GLYPH_W - 100)) {
                    cursor_x = 100;
                    cursor_y += GLYPH_H;
                }
            }
        }
    }

    close(fd);
    cleanup_display();
    return 0;
}
