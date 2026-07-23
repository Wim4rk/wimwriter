#include <stdint.h>
#include "EPD_IT8951.h"

// Typsnittsdimensioner
#define GLYPH_W 32
#define GLYPH_H 64
#define GLYPH_SIZE_BYTES (GLYPH_W * GLYPH_H) // 2048 bytes per tecken vid 8bpp
#define IT8951_A2_MODE 6
#define TARGET_MEM_ADDR 122480 // Enligt hårdvaruspecifikationen

extern uint8_t font_cache[128][GLYPH_SIZE_BYTES];

void Render_Fast_Char(char c, UWORD X, UWORD Y) {
    if (c < 0 || c > 127) return;

    IT8951_Load_Img_Info Load_Img_Info;
    IT8951_Area_Img_Info Area_Img_Info;

    // 1. Förbered minnesinformation (8bpp)
    Load_Img_Info.Source_Buffer_Addr = font_cache[(int)c];
    Load_Img_Info.Endian_Type = IT8951_LDIMG_L_ENDIAN;
    Load_Img_Info.Pixel_Format = IT8951_8BPP;
    Load_Img_Info.Rotate = IT8951_ROTATE_0;
    Load_Img_Info.Target_Memory_Addr = TARGET_MEM_ADDR;

    // 2. Definiera uppdateringsrutan ("damage box")
    Area_Img_Info.Area_X = X;
    Area_Img_Info.Area_Y = Y;
    Area_Img_Info.Area_W = GLYPH_W;
    Area_Img_Info.Area_H = GLYPH_H;

    // 3. Ställ in minnesadress och initiera överföring
    EPD_IT8951_WaitForDisplayReady();
    EPD_IT8951_SetTargetMemoryAddr(Load_Img_Info.Target_Memory_Addr);
    EPD_IT8951_LoadImgAreaStart(&Load_Img_Info, &Area_Img_Info);

    // 4. Skicka pixeldata blixtsnabbt över SPI (bypass av Waveshares UWORD-loop)
    // Eftersom vår cache är 8bpp matar vi bara bytes rakt in i kontrollern.
    UWORD Write_Preamble = 0x0000;
    EPD_IT8951_ReadBusy();
    DEV_Digital_Write(EPD_CS_PIN, LOW);

    DEV_SPI_WriteByte(Write_Preamble >> 8);
    DEV_SPI_WriteByte(Write_Preamble);
    EPD_IT8951_ReadBusy();

    uint8_t *pixel_ptr = font_cache[(int)c];
    for(uint32_t i = 0; i < GLYPH_SIZE_BYTES; i++) {
        DEV_SPI_WriteByte(*pixel_ptr);
        pixel_ptr++;
    }

    DEV_Digital_Write(EPD_CS_PIN, HIGH);
    EPD_IT8951_LoadImgEnd();

    // 5. Beordra skärmen att omedelbart rita upp rutan i A2-läge
    EPD_IT8951_Display_Area(X, Y, GLYPH_W, GLYPH_H, IT8951_A2_MODE);
}
