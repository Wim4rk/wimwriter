#include "keyboard.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

// Håll koll på modifier-tangenter
static bool shift_pressed = false;
static bool caps_locked = false;

// Mappning för standardtangenter (Svensk fysisk layout -> 7-bit ASCII)
// Observera: 'å' = '}', 'ä' = '{', 'ö' = '|' i din 7-bitars standard
static const char map_default[128] = {
    [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd', [KEY_E] = 'e',
    [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h', [KEY_I] = 'i', [KEY_J] = 'j',
    [KEY_K] = 'k', [KEY_L] = 'l', [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o',
    [KEY_P] = 'p', [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
    [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x', [KEY_Y] = 'y',
    [KEY_Z] = 'z',
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',
    [KEY_SPACE] = ' ', [KEY_ENTER] = '\n', [KEY_BACKSPACE] = 127, // 127 är Delete
    [KEY_LEFTBRACE] = '}',  // å
    [KEY_APOSTROPHE] = '{', // ä
    [KEY_SEMICOLON] = '|',  // ö
    [KEY_MINUS] = '+', [KEY_SLASH] = '-', [KEY_COMMA] = ',', [KEY_DOT] = '.'
};

// Mappning för Shift/Caps Lock (Svensk fysisk layout -> 7-bit ASCII)
// 'Å' = ']', 'Ä' = '[', 'Ö' = '\'
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
    [KEY_LEFTBRACE] = ']',  // Å
    [KEY_APOSTROPHE] = '[', // Ä
    [KEY_SEMICOLON] = '\\', // Ö
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

// Publik funktion så main.c kan kontrollera shift (t.ex. Shift + F3)[cite: 2]
bool keyboard_is_shift_pressed(void) {
    return shift_pressed;
}

char keyboard_get_char(struct input_event *ev) {
    // Uppdatera modifiers oavsett om det är ned- eller uppsläpp
    if (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT) {
        shift_pressed = (ev->value == 1 || ev->value == 2);
        return 0;
    }
    if (ev->code == KEY_CAPSLOCK && ev->value == 1) {
        caps_locked = !caps_locked;
        return 0;
    }

    // Returnera bara tecken vid Key Press (1) eller Key Repeat (2)
    if (ev->value == 1 || ev->value == 2) {
        if (ev->code < 128) {
            char default_char = map_default[ev->code];

            // Kontrollera om det är en bokstav (a-z eller å, ä, ö)
            bool is_letter = (default_char >= 'a' && default_char <= 'z') ||
                                default_char == '{' || default_char == '}' || default_char == '|';

            bool use_shift = shift_pressed;

            if (use_shift) {
                return map_shift[ev->code];
            } else {
                return default_char;
            }
        }
    }
    return 0;
}
