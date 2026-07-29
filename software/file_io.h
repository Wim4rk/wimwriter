#ifndef FILE_IO_H
#define FILE_IO_H

// Behövs för att file_io ska känna till MAX_ROWS, MAX_COLS och JUMP_LINES
#include "../firmware/display.h"

void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr);

#endif
