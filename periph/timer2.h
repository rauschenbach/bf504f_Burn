#ifndef _TIMER2_H
#define _TIMER2_H

#include "globdefs.h"
#include "pll.h"



bool TIMER2_init(void);

/* запустить таймер 2 */
#pragma always_inline
IDEF void TIMER2_enable(void)
{
   *pTIMER_ENABLE = TIMEN2;
    ssync();  
}


/* Запретить таймер 2  */
#pragma always_inline
IDEF void TIMER2_disable(void)
{
    *pTIMER_DISABLE = TIMEN2;
     ssync();  
}


void TIMER2_set_callback(void *);
void TIMER2_del_callback(void);

void TIMER2_ISR(void);
void TIMER2_set_sec(u32);
u32  TIMER2_get_sec(void);
u64  TIMER2_get_msec(void);
u32  TIMER2_get_counter(void);
u32  TIMER2_get_work_time(void);
s64  TIMER2_get_long_time(void);

#endif	/* timer2.h */
