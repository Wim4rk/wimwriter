#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include "../lib/Config/DEV_Config.h"
#include "../lib/e-Paper/EPD_IT8951.h"

// Inställningar
#define CHAR_W 12
#define CHAR_H 20
#define MODE 2 // DU_Mode

int main() {
    if (DEV_Module_Init() != 0) return -1;
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(2140);
    UDOUBLE target_addr = ((UDOUBLE)dev_info.Memory_Addr_H << 16) | dev_info.Memory_Addr_L;

    EPD_IT8951_Clear_Refresh(dev_info, target_addr, GC16_Mode);

    int kbd_fd = open("/dev/input/event0", O_RDONLY);
    if (kbd_fd < 0) return -1;

    int cursor_x = 0, cursor_y = 0;
    struct input_event ev;
    
    // Buffert för en bokstav (12x20 pixlar)
    int size = (CHAR_W * CHAR_H) / 8;
    UBYTE *char_buf = (UBYTE *)calloc(1, size);
    memset(char_buf, 0xFF, size); // Fyll med "svart" för test

    printf("Skrivmaskin startad. Skriver till /dev/input/event0\n");

    // Ersätt din loop i writer_test.c med detta för att testa adressering:
for (int i = 0; i < 20; i++) { // En rad, 20 pixlar
    buf[i] = 0xFF;
}
// Skicka endast en rad (h=1)
EPD_IT8951_1bp_Refresh(buf, 100, 100, 20, 1, 2, target_addr, true);

    free(char_buf);
    close(kbd_fd);
    DEV_Module_Exit();
    return 0;
}
