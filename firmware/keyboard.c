#include "keyboard.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

// Håll koll på modifier-tangenter
static bool left_shift_pressed = false;
static bool right_shift_pressed = false;
static bool caps_locked = false;
static bool ctrl_pressed = false;
static bool altgr_pressed = false;

// Mappning för AltGr (Svensk fysisk layout)
static const char map_altgr[128] = {
    [KEY_2] = '@',
    [KEY_4] = '$',
    [KEY_7] = '{',
    [KEY_8] = '[',
    [KEY_9] = ']',
    [KEY_0] = '}',
    [KEY_MINUS] = '\\', // Ofta knappen till höger om 0 (frågetecken/plus/backslash)
    [KEY_RIGHTBRACE] = '~' // Ofta knappen till höger om Å (diaeresis/timsglas/tilde)
};

// Mappning för standardtangenter (gemener)
static const char map_default[128] = {
    [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd', [KEY_E] = 'e',
    [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h', [KEY_I] = 'i', [KEY_J] = 'j',
    [KEY_K] = 'k', [KEY_L] = 'l', [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o',
    [KEY_P] = 'p', [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
    [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x', [KEY_Y] = 'y',
    [KEY_Z] = 'z',
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',
    [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_BACKSPACE] = 127,
    [KEY_LEFTBRACE] = 0xE5,  // å (229)
    [KEY_APOSTROPHE] = 0xE4, // ä (228)
    [KEY_SEMICOLON] = 0xF6,  // ö (246)
    [KEY_MINUS] = '+', [KEY_SLASH] = '-', [KEY_COMMA] = ',', [KEY_DOT] = '.'
};

// Mappning för Shift/Caps Lock (versaler)
static const char map_shift[128] = {
    [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D', [KEY_E] = 'E',
    [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H', [KEY_I] = 'I', [KEY_J] = 'J',
    [KEY_K] = 'K', [KEY_L] = 'L', [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O',
    [KEY_P] = 'P', [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
    [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X', [KEY_Y] = 'Y',
    [KEY_Z] = 'Z',
    [KEY_1] = '!', [KEY_2] = '"', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%',
    [KEY_6] = '&', [KEY_7] = '/', [KEY_8] = '(', [KEY_9] = ')', [KEY_0] = '=',
    [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_BACKSPACE] = 127,
    [KEY_LEFTBRACE] = 0xC5,  // Å (197)
    [KEY_APOSTROPHE] = 0xC4, // Ä (196)
    [KEY_SEMICOLON] = 0xD6,  // Ö (214)
    [KEY_MINUS] = '?', [KEY_SLASH] = '_', [KEY_COMMA] = ';', [KEY_DOT] = ':'
};

int keyboard_init(const char *device_path) {
    int fd = open(device_path, O_RDONLY);
    if (fd == -1) {
        printf("Kunde inte öppna %s\n", device_path);
    }
    return fd;
}

void keyboard_close(int fd) {
    if (fd != -1) close(fd);
}

// Publik funktion så main.c kan kontrollera shift (t.ex. Shift + F3)[cite: 1, 2]
return left_shift_pressed || right_shift_pressed;

// Publik funktion så editor.c kan kontrollera ctrl (t.ex. Ctrl + Backspace)
bool keyboard_is_ctrl_pressed(void) {
    return ctrl_pressed;
}

char keyboard_get_char(struct input_event *ev) {
    // Hantera shift-tangenterna
    if (ev->code == KEY_LEFTSHIFT) {
        left_shift_pressed = (ev->value == 1 || ev->value == 2);
        return 0;
    }
    if (ev->code == KEY_RIGHTSHIFT) {
        right_shift_pressed = (ev->value == 1 || ev->value == 2);
        return 0;
    }

    // Hantera Ctrl-tangenterna
    if (ev->code == KEY_LEFTCTRL || ev->code == KEY_RIGHTCTRL) {
        ctrl_pressed = (ev->value == 1 || ev->value == 2);
        return 0;
    }

    if (ev->code == KEY_CAPSLOCK && ev->value == 1) {
        caps_locked = !caps_locked;
        return 0;
    }

    // Hantera AltGr (KEY_RIGHTALT är standard för höger Alt)
    if (ev->code == KEY_RIGHTALT) {
        altgr_pressed = (ev->value == 1 || ev->value == 2);
        return 0;
    }

    // Returnera bara tecken vid Key Press (1) eller Key Repeat (2)
    if (ev->value == 1 || ev->value == 2) {
        if (ev->code < 128) {
            char default_char = map_default[ev->code];

            bool is_letter = (default_char >= 'a' && default_char <= 'z') ||
                                default_char == (char)0xE5 || default_char == (char)0xE4 || default_char == (char)0xF6;

            bool use_shift = left_shift_pressed || right_shift_pressed;

            if (caps_locked && is_letter) {
                use_shift = !use_shift;
            }

            if (altgr_pressed && map_altgr[ev->code] != 0) {
                return map_altgr[ev->code];
            } else if (use_shift) {
                return map_shift[ev->code];
            } else {
                return default_char;
            }
        }
    }
    return 0;
}
