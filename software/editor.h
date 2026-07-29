#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <linux/input.h>
#include "../firmware/display.h"

// Definiera tillstånden för vår State Machine
typedef enum {
    STATE_EDITING,
    STATE_HELP,
    STATE_FILE_SWITCH,
    STATE_NAMING_FILE
} EditorState;

// Gör tillståndet tillgängligt vid behov
extern EditorState current_state;

// Huvudfunktion för att ta emot inmatning från main-loopen
void handle_input(struct input_event *ev, UDOUBLE target_addr, char *text_buffer, int *cursor_row, int *cursor_col);

#endif
