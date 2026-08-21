#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>

// En fast gräns för hur stort ett dokument kan vara i RAM (t.ex. 1 MB = ~1 miljon tecken)
#define MAX_DOC_SIZE (1024 * 1024)

typedef struct {
    char data[MAX_DOC_SIZE];
    int gap_start;  // Index där luckan börjar
    int gap_end;    // Index där luckan slutar
} GapBuffer;

// Global instans av vår modell
extern GapBuffer document_model;

// Initierar en tom modell
void model_init(void);

// Textmanipulation
void model_insert_char(char c);
void model_delete_char(void);
void model_overwrite_char(char c);

// Navigering (flyttar luckan i minnet)
void model_move_cursor_left(void);
void model_move_cursor_right(void);

// Hjälpfunktioner för vyn
int model_get_text_length(void);
char model_char_at(int index);

#endif // MODEL_H
