#include "lib/GUI/GUI_Paint.h"
#include "lib/e-Paper/EPD_IT8951.h"
#include <stdlib.h>

int init_display(IT8951_Dev_Info *dev_info, UDOUBLE *target_addr) {
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    printf("Modul initierad. Startar EPD...\n");
    fflush(stdout);

    // Init-anrop med VCOM satt till 2140 (-2.14V)
    Dev_Info = EPD_IT8951_Init(2140);
    printf("Init-anrop avklarat.\n");
    fflush(stdout);

    UDOUBLE target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    // Bygg upp font-cachen i minnet före skärmrensningen
    init_glyph_cache(&Font24);

    // Rensa skärmen vid uppstart
    EPD_IT8951_Clear_Refresh(Dev_Info, target_addr, 0);
}
