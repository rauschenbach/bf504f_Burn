#ifndef _TIMER1_H
#define _TIMER1_H


#include "globdefs.h"
#include "pll.h"


int  TIMER1_config(void);
void TIMER1_ISR(void);
void TIMER1_set_sec(u32);
u32  TIMER1_get_sec(void);
u32  TIMER1_get_counter(void);

bool TIMER1_is_run(void);
int  TIMER1_get_drift(u32*, u32*);
s64  TIMER1_get_long_time(void);
u32  TIMER1_get_error(void);
void TIMER1_set_callback(void*);
void TIMER1_del_callback(void);
bool TIMER1_is_callback(void);
void TIMER1_quick_start(long);
int  TIMER1_sync_timer(void);

/* Непрерываемая задержка, в отличие от delay_ms  */
#define sleep_ms(x)  TIMER2_delay_ms(x)

/**
 * Разрешить подстройку таймера
 */
#pragma always_inline
IDEF void TIMER1_shift_phase(long ticks)
{
  	*pTIMER1_PERIOD = TIMER_PERIOD - ticks;
	 ssync();
}


/**
 * Разрешить прерывания таймера 1
 */
#pragma always_inline
IDEF void TIMER1_enable_irq(void)
{
	/* перепрограммируем TIMER1 на на 7 приоритет */
	*pSIC_IMASK1 |= IRQ_TIMER1;
	*pSIC_IAR4 &= 0xFFFFFF0F;
	*pSIC_IAR4 |= 0x00000000;	/* TIMER1 IRQ: IVG7 */
	 ssync();
}

/* запустить таймер 1 */
#pragma always_inline
IDEF void TIMER1_enable(void)
{
   *pTIMER_ENABLE = TIMEN1;
    ssync();  
}


/* Запретить таймер 1  */
#pragma always_inline
IDEF void TIMER1_disable(void)
{
    *pTIMER_DISABLE = TIMEN1;
     ssync();  
}



#endif				/* timer1.h */
