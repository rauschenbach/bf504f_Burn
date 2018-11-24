#ifndef _COMPORT_H
#define _COMPORT_H

#include "globdefs.h"

int  comport_init(void);
void comport_close(void);
u8   comport_get_command(void);

#endif /* comport.h */
