#ifndef FAST_SPI_H
#define FAST_SPI_H

#include "DEV_Config.h"

// Funtion för blocköverföring via SPI
void fast_spi_write_nbyte(UBYTE *pData, UDOUBLE len);

#endif
