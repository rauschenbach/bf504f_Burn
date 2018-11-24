#ifndef _SPI0_H
#define _SPI0_H

#include "globdefs.h"


void SPI0_init(void);
void SPI0_stop(void);
u8   SPI0_write_read(u8);

#endif /* spi0.h  */
