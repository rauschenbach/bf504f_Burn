#ifndef _UART1_H
#define _UART1_H

#include "globdefs.h"


/*******************************************************************
 *  function prototypes
 *******************************************************************/
void UART1_STATUS_ISR(void);
bool UART1_init(void*);
void UART1_close(void);
void UART1_get_count(DEV_UART_COUNTS*);
int  UART1_write_str(char *, int);

IDEF void UART1_start_tx(void)
{
   *pUART1_IER_SET = ETBEI;
}

IDEF void UART1_stop_tx(void)
{
   *pUART1_IER_CLEAR = ETBEI;
}


IDEF void UART1_tx_byte(char byte)
{
    *pUART1_THR = byte;
}


#endif /*  uart1.h */
