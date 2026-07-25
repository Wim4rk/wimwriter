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


/* 
void render_char(char c, int x, int y, UDOUBLE target_addr) {
    if (c < 0 || c > 127) return;

    // 1. Räkna om koordinaterna för 180 graders rotation.
    // Vi utgår från skärmens fasta paneldimensioner (1448x1072).
    int rot_x = 1448 - GLYPH_W - x;
    int rot_y = 1072 - GLYPH_H - y;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Vi skapar en temporär buffert för att vända tecknet uppochner och bakofram (180 grader)
    static UBYTE rotated_glyph[GLYPH_SIZE_BYTES];
    const uint8_t *src = wim_font_32x64[(int)c];

    for (int i = 0; i < GLYPH_SIZE_BYTES; i++) {
        rotated_glyph[GLYPH_SIZE_BYTES - 1 - i] = src[i];
    }

    load_info.Source_Buffer_Addr = (UBYTE*)rotated_glyph;
    load_info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    load_info.Pixel_Format = IT8951_8BPP;
    load_info.Rotate = IT8951_ROTATE_180; // Hårdvarutrotation
    load_info.Target_Memory_Addr = target_addr;

    area_info.Area_X = rot_x;
    area_info.Area_Y = rot_y;
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

    for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
        DEV_SPI_WriteByte(rotated_glyph[i]);
    }

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // Kör A2-läge för maximal hastighet
    EPD_IT8951_Display_Area(x, y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
} */

void render_char(char c, int x, int y, UDOUBLE target_addr) {
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info load_info;
    IT8951_Area_Img_Info area_info;

    // Vänd tecknet rätt genom att läsa font-arrayen baklänges (uppochner och spegelvänt)
    static UBYTE flipped_glyph[GLYPH_SIZE_BYTES];
    const uint8_t *src = wim_font_32x64[(int)c];
    for (int i = 0; i < GLYPH_SIZE_BYTES; i++) {
        flipped_glyph[GLYPH_SIZE_BYTES - 1 - i] = src[i];
    }

    load_info.Source_Buffer_Addr = (UBYTE*)flipped_glyph;
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

    for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
        DEV_SPI_WriteByte(flipped_glyph[i]);
    }

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    EPD_IT8951_Display_Area(x, y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
}


void cleanup_display(void) {
     DEV_Module_Exit();
}
