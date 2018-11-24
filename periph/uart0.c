/*******************************************************************
 * UART0 прием строки NMEA из GPS, используем два буфера
 * Убрать лишние сообщения!!!
 * Реагируем только на строку $GPRMC!!!
*******************************************************************/
#include <string.h>
#include "uart0.h"
#include "utils.h"
#include "irq.h"
#include "log.h"
#include "pll.h"

/************************************************************************
 * 	Статические переменные
 ************************************************************************/
static DEV_UART_STRUCT uart0_xchg_struct;



/**
 * Function:    UART_init - запускается сразу же 
 * Убрал делитель EDBO
 */
#pragma section("FLASH_code")
bool UART0_init(void *dev)
{
    volatile int temp, i;
    DEV_UART_STRUCT *comm;
    u16 divider;
    bool res = false;

    do {
	if (dev == NULL)
	    break;

	comm = (DEV_UART_STRUCT *) dev;
	if (comm->baud <= 0) {
	    log_write_log_file("ERROR: UART0 baud can't be zero\n");
	    break;
	}

	*pPORTG_FER |= (PG12 | PG13);	/* Разрешаем функции на портах Rx и Tx - они находятся на PG12 и PG13 */
	*pPORTG_MUX &= ~(PG12 | PG13);	/* Включаем 3-ю функцию 00 на 12:13 */
	ssync();

	/* часы UART clock + делитель-прескалер = 1 + rerout на обычное прерывание - без EDBO */
	*pUART0_GCTL = EGLSI | UCEN | EDBO;
	ssync();


	divider = (SCLK_VALUE / comm->baud);
	*pUART0_DLL = divider;
	*pUART0_DLH = (divider >> 8);


	*pUART0_LCR = WLS_8;	/* 8N1 */
	temp = *pUART0_RBR;
	temp = *pUART0_LSR;

	*pUART0_IER_CLEAR = 0xff;
	*pUART0_IER_SET = ERBFI | ELSI;	/* приемный буфер полон + Ошибки */
	ssync();

	uart0_xchg_struct.rx_call_back_func = (void (*)(u8)) comm->rx_call_back_func;	/* Указатель на функцыю чтения */
	uart0_xchg_struct.tx_call_back_func = (void (*)(void)) comm->tx_call_back_func;	/* Указатель на функцыю записи */
	IRQ_register_vector(NMEA_VECTOR_NUM);	/* Регистрируем прерывание UART0 */

	*pSIC_IMASK0 |= IRQ_UART0_ERR;	/* чтение по статусу для UART0 */
	*pSIC_IAR0 &= 0xFF0FFFFF;	/* 5-й полубайт  (с нуля) */
	*pSIC_IAR0 |= 0x00200000;	/* STATUS IRQ: IVG09 */
	ssync();

	log_write_log_file("INFO: UART0 init on %d baud OK\n", comm->baud);
	uart0_xchg_struct.is_open = true;
	res = true;
    } while (0);
    return res;
}

/**
 * Выключаем UART0, освободим буферы 
 */
#pragma section("FLASH_code")
void UART0_close(void)
{
    *pUART0_GCTL = 0;
    ssync();
    uart0_xchg_struct.is_open = false;
    log_write_log_file("INFO: UART0 closed OK\n");
}

/**
 * Посылка строки через UART0
 * Когда можно послать. Можно передавать или в отладке или в модеме 
 */
#pragma section("FLASH_code")
int UART0_write_str(char *str, int len)
{
    int i = -1;

    /* Если открыт */
    if (uart0_xchg_struct.is_open) {
////	IRQ_unregister_vector(NMEA_VECTOR_NUM);	
	for (i = 0; i < len; i++) {
	    while (!(*pUART0_LSR & THRE));
	    *pUART0_THR = str[i];
	    ssync();
	}
////	IRQ_register_vector(NMEA_VECTOR_NUM);	/* Регистрируем прерывание UART0 */
    }
    return i;
}

/**
 * Причина прерывания - приемный буфер готов - реагируем только на $GPRMC!
 */
#pragma section("FLASH_code")
//#pragma section("L1_code")
void UART0_STATUS_ISR(void)
{
    volatile c8 byte;

    register volatile u16 stat = *pUART0_LSR;	/* Приняли... */
    ssync();

    /* Сначала проверим ошибки - биты 1..4 */
    if (stat & 0x1E) {
	*pUART0_LSR = stat & 0x1E;
	byte = *pUART0_RBR;	// Читаем из буфера
	ssync();
    }


    if (stat & DR) {
	byte = *pUART0_RBR;	/* Принятый байт */
	ssync();

	/* Вызываем функцию callback */
	if (uart0_xchg_struct.rx_call_back_func != NULL) {
	    uart0_xchg_struct.rx_call_back_func(byte);
	}

    } else if (stat & THRE) {	/*  Передаем буфер */
	if (uart0_xchg_struct.tx_call_back_func != NULL) {
	    uart0_xchg_struct.tx_call_back_func();
	}
    }
}
