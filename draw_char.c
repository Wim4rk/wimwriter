#include <stdint.h>

// Berättar för kompilatorn att set_pixel finns i framebuffer.c
extern void set_pixel(int x, int y, int black);

// Anpassa detta efter hur din array faktiskt heter i font20.c
extern const uint8_t Font20_Table[];

// Funktionen ritar in ett tecken i framebuffern via set_pixel
void draw_char_to_fb(char c, int cursor_x, int cursor_y) {
    if (c < 32 || c > 126) c = '?';
    
    int glyph_index = c - 32;
    int bytes_per_row = 2; // Baserat på din beskrivning om 14 pixlar (2 bytes)
    int glyph_size = 20 * bytes_per_row; // 40 bytes totalt per tecken
    
    // Peka på början av det specifika tecknets data
    const uint8_t *glyph_ptr = &Font20_Table[glyph_index * glyph_size];

    for (int row = 0; row < 20; row++) {
        for (int byte_col = 0; byte_col < bytes_per_row; byte_col++) {
            
            // Hämta rätt byte för denna rad och kolumn
            uint8_t pixel_byte = glyph_ptr[(row * bytes_per_row) + byte_col];

            for (int bit = 0; bit < 8; bit++) {
                int is_black = (pixel_byte & (1 << (7 - bit))) != 0;
                int pixel_x = cursor_x + (byte_col * 8) + bit;
                int pixel_y = cursor_y + row;

                set_pixel(pixel_x, pixel_y, is_black);
            }
        }
    }
}

/*void draw_char_generic(sFont *font, char c, int x, int y){
    // Använd font-Width och font->Height
    // Detta gör att vi kan skicka in &Dont20 eller &Font16
    printf("Ritar tecken '%c' med dimensionerna %dx%d\n", c, font->Width, font->Height);
}*/
