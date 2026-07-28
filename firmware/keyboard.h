#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <linux/input.h>
#include <stdbool.h>

// Initiera och stäng tangentbordet
int keyboard_init(const char *device_path);
void keyboard_close(int fd);

// Tolka eventet och returnera tecknet (returnerar 0 om ogiltigt/osynligt)
char keyboard_get_char(struct input_event *ev);

bool keyboard_is_ctrl_pressed(void);
bool keyboard_is_shift_pressed(void);

#endif
