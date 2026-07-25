#include "fast_spi.h"

#ifdef BCM
#include <bcm2835.h>
#endif

void fast_spi_write_nbyte(UBYTE *pData, UBYTE len) {
#ifdef BCM
    bcm2835_spi_writenb((char *)pData, len);
#endif
}
