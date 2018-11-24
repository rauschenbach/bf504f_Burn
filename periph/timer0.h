#ifndef _TIMER0_H
#define _TIMER0_H

#include "globdefs.h"

typedef enum {
   DELAY_US,
   DELAY_MS,
   DELAY_SEC
} DELAY_TYPE;

void TIMER0_delay(u32, DELAY_TYPE);


/***********************************************************************************
 * Задержки на миллисекунды и на микросекунды. Дефиниции
 ***********************************************************************************/
#define delay_ms(x)	 TIMER0_delay(x, DELAY_MS)
#define delay_us(x)      TIMER0_delay(x, DELAY_US)
#define delay_sec(x)     TIMER0_delay(x, DELAY_SEC)


#endif	/* timer2.h */
