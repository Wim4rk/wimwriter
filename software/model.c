#include "model.h"

GapBuffer document_model;

void model_init(void) {
    document_model.gap_start = 0;
    document_model.gap_end = MAX_DOC_SIZE;
}

void model_insert_char(char c) {
    // Om luckan är full har vi nått dokumentets maxstorlek
    if (document_model.gap_start == document_model.gap_end) {
        return;
    }

    // Skriv tecknet och flytta luckans start framåt
    document_model.data[document_model.gap_start] = c;
    document_model.gap_start++;
}

void model_delete_char(void) {
    // Backspace: Flytta luckans start bakåt om vi inte är i början
    if (document_model.gap_start > 0) {
        document_model.gap_start--;
    }
}

void model_move_cursor_left(void) {
    if (document_model.gap_start > 0) {
        document_model.gap_start--;
        document_model.gap_end--;
        // Flytta tecknet före luckan till luckans slut
        document_model.data[document_model.gap_end] = document_model.data[document_model.gap_start];
    }
}

void model_move_cursor_right(void) {
    if (document_model.gap_end < MAX_DOC_SIZE) {
        // Flytta tecknet efter luckan till luckans start
        document_model.data[document_model.gap_start] = document_model.data[document_model.gap_end];
        document_model.gap_start++;
        document_model.gap_end++;
    }
}

void model_overwrite_char(char c) {
    // Om luckan är full har vi nått dokumentets maxstorlek
    if (document_model.gap_start == document_model.gap_end) {
        return;
    }

    // Skriv tecknet och flytta luckans start framåt
    document_model.data[document_model.gap_start] = c;
    document_model.gap_start++;

    // Ät upp det gamla tecknet framför luckan, förutsatt att vi
    // inte har nått slutet av dokumentet eller en radbrytning.
    if (document_model.gap_end < MAX_DOC_SIZE &&
        document_model.data[document_model.gap_end] != '\n') {
        document_model.gap_end++;
    }
}

int model_get_text_length(void) {
    return document_model.gap_start + (MAX_DOC_SIZE - document_model.gap_end);
}

char model_char_at(int index) {
    if (index < document_model.gap_start) {
        return document_model.data[index];
    } else {
        // Hoppa över luckan vid läsning
        return document_model.data[index + (document_model.gap_end - document_model.gap_start)];
    }
}
