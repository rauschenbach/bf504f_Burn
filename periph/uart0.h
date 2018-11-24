#ifndef _UART0_H
#define _UART0_H

#include "globdefs.h"

/**
 *  function prototypes
 */
void UART0_STATUS_ISR(void);    /* Прерывание по приемцу DMA  */
bool UART0_init(void*);		/* Инициализация */
void UART0_close(void);		
int  UART0_write_str(char*, int);

#endif /*  uart0.h */

