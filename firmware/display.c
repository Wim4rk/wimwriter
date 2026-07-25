#include <stdio.h>
#include "display.h"
#include "wim_font_courier.h" 
#include "DEV_Config.h"

extern void EPD_IT8951_ReadBusy(void);

void init_display(UDOUBLE *target_addr) {
    IT8951_Dev_Info Dev_Info;

    if (DEV_Module_Init() != 0) {
        printf("Fel vid initiering av displayen\n");
        return;
    }

    Dev_Info = EPD_IT8951_Init(2140);
    *target_addr = ((UDOUBLE)Dev_Info.Memory_Addr_H << 16) | Dev_Info.Memory_Addr_L;

    EPD_IT8951_Clear_Refresh(Dev_Info, *target_addr, 0);
}


void render_char(char c, int x, int y, UDOUBLE target_addr) {
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    load_info.Source_Buffer_Addr = (UBYTE*)wim_font_32x64[(int)c];
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_0;
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = x;
    area_info.Area_Y = y;
    area_info.Area_W = GLYPH_W;
    area_info.Area_H = GLYPH_H;

    EPD_IT8951_WaitForDisplayReady();
    EPD_IT8951_SetTargetMemoryAddr(target_addr);
    EPD_IT8951_LoadImgAreaStart(&load_info, &area_info);

    UWORD write_preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(write_preamble >> 8);
    DEV_SPI_WriteByte(write_preamble);
    EPD_IT8951_ReadBusy();

    const uint8_t *pixel_ptr = wim_font_32x64[(int)c];
    for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
        DEV_SPI_WriteByte(*pixel_ptr++);
    }

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Kör A2-läge för maximal hastighet i enlighet med Prio 1
    EPD_IT8951_Display_Area(x, y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
}


void cleanup_display(void) {
     DEV_Module_Exit();
}
