#include <stdio.h>
#include "DEV_Config.h"
#include "EPD_IT8951.h"

int main() {
    printf("Initierar hårdvara...\n");
    if (DEV_Module_Init() != 0) return 1;

    printf("Hårdvara initierad. Försöker initiera display...\n");
    fflush(stdout);

    // EPD_IT8951_Init innehåller redan anrop till Reset och SystemRun.
    // Om detta hänger sig, ligger felet i HAT-kortets svar på SPI.
    IT8951_Dev_Info dev_info = EPD_IT8951_Init(2140);
    
    printf("Display initierad. Bredd: %d\n", dev_info.Panel_W);
    
    DEV_Module_Exit();
    return 0;
}
