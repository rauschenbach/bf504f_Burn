#ifndef _POWER_H
#define _POWER_H

#include "globdefs.h"

#define POWER_CPU_ON 		1
#define POWER_INIT_MODE         2
#define POWER_OFF_REQUEST       3
#define POWER_CPU_OFF           4
#define POWER_BURN_FLAG       	5


void POWER_init(void);
void POWER_MAGNET_ISR(void);
void BOUNCE_TIMER_ISR(void);
int  POWER_on(void);
void POWER_off(int, bool);

#endif /* power.h  */
