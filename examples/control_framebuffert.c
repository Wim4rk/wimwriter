#include "EPD_IT8951.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Naturlig upplösning för 6-tums HD e-Paper
#define WIDTH 1448
#define HEIGHT 1072

int main(void) {
    printf("[1/7] Initierar IT8951-kontrollern...\n");
    fflush(stdout);
    
    // Initiera skärmen
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(2140); 

    // Skriv ut den information vi får tillbaka från skärmen för att verifiera SPI
    printf("      -> Skärmbredd rapporterad: %d px\n", dev_info.Panel_W);
    printf("      -> Skärmhöjd rapporterad: %d px\n", dev_info.Panel_H);
    printf("      -> Minnesadress L: 0x%04X\n", dev_info.Memory_Addr_L);
    printf("      -> Minnesadress H: 0x%04X\n", dev_info.Memory_Addr_H);
    printf("      -> LUT-version: %s\n", dev_info.LUT_Version);
    fflush(stdout);

    // En snabb kontroll om vi verkar ha kontakt över SPI
    if (dev_info.Panel_W == 0 || dev_info.Panel_W == 0xFFFF) {
        printf("VARNING: Kunde inte läsa giltig information från IT8951. Kontrollera SPI och ström.\n");
        fflush(stdout);
    }

    printf("[2/7] Allokerar framebuffert i RAM...\n");
    fflush(stdout);
    uint32_t buffer_size = WIDTH * HEIGHT;
    uint8_t *frame_buffer = (uint8_t *)malloc(buffer_size);
    if (frame_buffer == NULL) {
        printf("FEL: Kunde inte allokera minne för framebufferten.\n");
        fflush(stdout);
        return -1; 
    }

    printf("[3/7] Fyller bufferten med vitt...\n");
    fflush(stdout);
    memset(frame_buffer, 0xFF, buffer_size);

    printf("[4/7] Ritar en 100x100 svart kvadrat i minnet...\n");
    fflush(stdout);
    int start_x = (WIDTH / 2) - 50;
    int start_y = (HEIGHT / 2) - 50;

    for (int y = start_y; y < start_y + 100; y++) {
        for (int x = start_x; x < start_x + 100; x++) {
            frame_buffer[(y * WIDTH) + x] = 0x00; // Svart
        }
    }

    printf("[5/7] Beräknar 32-bitars måladress...\n");
    fflush(stdout);
    UDOUBLE target_addr = dev_info.Memory_Addr_L | (dev_info.Memory_Addr_H << 16);
    printf("      -> Beräknad target_addr: 0x%08lX\n", (unsigned long)target_addr);
    fflush(stdout);

    printf("[6/7] Skickar bilddata till IT8951 och uppdaterar skärmen (detta kan ta 1-2 sekunder)...\n");
    fflush(stdout);
    EPD_IT8951_8bp_Refresh(frame_buffer, 0, 0, WIDTH, HEIGHT, false, target_addr);

    printf("[7/7] Frigör resurser och försätter skärmen i vila...\n");
    fflush(stdout);
    free(frame_buffer);
    EPD_IT8951_Sleep();

    printf("Klart!\n");
    fflush(stdout);

    return 0;
}
