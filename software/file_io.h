#ifndef FILE_IO_H
#define FILE_IO_H

// Behövs för att file_io ska känna till MAX_ROWS, MAX_COLS och JUMP_LINES
#include "../firmware/display.h"

void save_document_to_file(const char *filename);
void append_to_temp_file(char c);
void save_buffer_to_file(const char *filename, const char *buffer, int max_rows, int max_cols);
void load_file_into_buffer(const char *filename, char *buffer, int *cursor_row, int *cursor_col, UDOUBLE target_addr);
void sanitize_string(const char *input, char *output, size_t max_len);

#endif
