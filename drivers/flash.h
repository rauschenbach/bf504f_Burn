#ifndef __FLASH_H
#define __FLASH_H

#include "globdefs.h"


#define FLASH_START_ADDR 	((uint32_t)0x20000000)	/* start address - то что у нас */

int  FLASH_init(void);
int  FLASH_erase_page(u32);
int  FLASH_program_half_word(u32, u16);

#endif /* flash.h */


