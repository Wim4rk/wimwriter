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
    STATE_NAMING_FILE,
    STATE_CONFIRM_OVERWRITE
} EditorState;

extern bool is_wifi_active;
// Gör tillståndet tillgängligt vid behov
extern EditorState current_state;

extern time_t status_bar_timestamp;
extern bool status_bar_visible;

// Huvudfunktion för att ta emot inmatning från main-loopen
void handle_input(struct input_event *ev, UDOUBLE target_addr, char *text_buffer, int *cursor_row, int *cursor_col, bool more_keys_waiting);
void editor_flush_queue(char *text_buffer, int cursor_row, int cursor_col, UDOUBLE target_addr);

#endif
