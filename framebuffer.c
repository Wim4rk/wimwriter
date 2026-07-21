#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Skärmens upplösning (IT8951 6-tum HD i native-orientering)
#define EPD_WIDTH 1448
#define EPD_HEIGHT 1072

// 1-bpp innebär att varje byte rymmer 8 pixlar.
// Bredd i bytes blir uppåt-avrundat om det inte är jämnt delbart med 8.
#define EPD_WIDTH_BYTES (EPD_WIDTH / 8)
#define FRAMEBUFFER_SIZE (EPD_WIDTH_BYTES * EPD_HEIGHT)

// Vår globala framebuffer i RAM
uint8_t *framebuffer;

// Utkast till initiering
int init_framebuffer() {
    // calloc nollställer minnet (ofta lika med vit skärm, beroende på skärmens logik)
    framebuffer = (uint8_t *)calloc(FRAMEBUFFER_SIZE, sizeof(uint8_t));
    if (framebuffer == NULL) {
        return -1; // Minnesallokeringsfel
    }
    return 0;
}

// Förslag på funktion för att sätta en enskild pixel
// Origo (0,0) antas vara övre vänstra hörnet.
void set_pixel(int x, int y, int black) {
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
        return; // Förhindrar skrivning utanför skärmens gränser
    }

    int byte_index = (y * EPD_WIDTH_BYTES) + (x / 8);
    int bit_offset = 7 - (x % 8); // Mest signifikanta biten (MSB) till vänster

    if (black) {
        // Sätt biten till 1
        framebuffer[byte_index] |= (1 << bit_offset);
    } else {
        // Sätt biten till 0
        framebuffer[byte_index] &= ~(1 << bit_offset);
    }
}

// Frigör minnet vid "Safe Shutdown"
void free_framebuffer() {
    if (framebuffer != NULL) {
        free(framebuffer);
    }
}
