#ifndef _TWI_H
#define _TWI_H

#include "globdefs.h"

#pragma pack(4)
typedef struct {
  u16 lo_div_clk; /* Мл. байт делителя */
  u16 hi_div_clk;  /* ст. байт делителя */
  u32 rsvd;
} twi_clock_div;


void TWI_stop(void);
void TWI_write_byte(u16, u16, u8, const void*);
u8   TWI_read_byte(u16, u8, const void*);
bool TWI_write_pack(u16, u8, u8 *, u16, const void*);
bool TWI_read_pack(u16, u8, u8* , u16, const void*);

#endif /* twi.h   */
