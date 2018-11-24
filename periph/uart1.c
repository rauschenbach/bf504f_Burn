/*******************************************************************
 * Этот UART - здесь ТОЛЬКО настройка UART1 
 * и вызов переданной функции CALLBACK
 *******************************************************************/
#include <string.h>
#include <stdio.h>
#include "uart1.h"
#include "utils.h"
#include "main.h"
#include "pll.h"		/* Частота SCLK описана здесь!  */
#include "log.h"
#include "irq.h"

#define 	WAIT_MODEM_TIME_MS			250	/* 200 мс - максимальное время ожыдания ответа по протоколу */

/************************************************************************
 * 	Статические переменные
 ************************************************************************/
static DEV_UART_STRUCT uart1_xchg_struct;
static DEV_UART_COUNTS uart1_cnt;	/* Счетчики обменов - на передачу */


/************************************************************************
 * 	Статические функции
 ************************************************************************/


/**
 * Function:    UART1_init
 * Description: Инициализирует UART на прием данных с первого порта начинает работать сразу по выходу из функции
 */
#pragma section("FLASH_code")
bool UART1_init(void *dev)
{
    volatile int temp;
    DEV_UART_STRUCT *comm;
    u16 divider;
    bool res = false;

    IRQ_unregister_vector(DEBUG_COM_VECTOR_NUM);	/* Убераем прерывание для UART!!! */

    do {
	uart1_xchg_struct.is_open = false;

	if (dev == NULL)
	    break;

	comm = (DEV_UART_STRUCT *) dev;

	uart1_xchg_struct.baud = comm->baud;
	uart1_xchg_struct.rx_call_back_func = (void (*)(u8)) comm->rx_call_back_func;	/* Указатель на функцыю чтения */
	uart1_xchg_struct.tx_call_back_func = (void (*)(void)) comm->tx_call_back_func;	/* Указатель на функцыю записи */


	if (uart1_xchg_struct.baud <= 0) {
	    log_write_log_file("ERROR: UART1 baud can't be zero\n");
	    break;
	}

	divider = SCLK_VALUE / uart1_xchg_struct.baud;

	*pPORTF_FER |= (PF6 | PF7);	/* Разрешаем порты Rx и Tx - они находятся на PF7 и PF6 */
	*pPORTFIO_DIR |= PF6; 		/* Выход */
	*pPORTFIO_INEN |= PF7;		/* Вход */
	*pPORTF_MUX &= ~(PF6 | PF7);	/* Разрешаем УАРТные порты - на 0-м MUX. Ставим два 0 на 6 и 7-м */
	ssync();

	*pUART1_GCTL = EGLSI | UCEN | EDBO;	/* часы UART clock + делитель-прескалер на 16 + rerout на обычное прерывание + EDBO */
	ssync();

	/* Делитель в зависимости от устройства */
	*pUART1_DLL = divider;
	*pUART1_DLH = divider >> 8;
	*pUART1_LCR = WLS_8;	/* 8N1 */

	/* События, на которые нужно реагировать: приемный буфер полон */
	*pUART1_IER_CLEAR = 0xff;	/* обязательно почистить - остается от других устройств */
	*pUART1_IER_SET = ERBFI | ELSI;	/* приемный буфер полон + Ошибки */
	ssync();

	temp = *pUART1_RBR;
	temp = *pUART1_LSR;
	ssync();

	/* чтение на IVG10 для UART1 */
	*pSIC_IMASK0 |= IRQ_UART1_ERR;	/* чтение по статусу */
	*pSIC_IAR0 &= 0xF0FFFFFF;
	*pSIC_IAR0 |= 0x03000000;	/* STATUS IRQ: IVG10 */
	ssync();
	uart1_xchg_struct.is_open = true;
	res = true;
    } while (0);

    IRQ_register_vector(DEBUG_COM_VECTOR_NUM);	/* Ставим прерывание для UART на десятку. */
    return res;		/* все ок */
}

/* Закрыть UART  */
#pragma section("FLASH_code")
void UART1_close(void)
{
    *pUART1_GCTL = 0;
    ssync();
    uart1_xchg_struct.is_open = false;
}


/* Посылка строки через UART1 */
#pragma section("FLASH_code")
int UART1_write_str(char *str, int len)
{
    int i = -1;

    if (uart1_xchg_struct.is_open) {

	/*  Когда можно послать. Можно передавать или в отладке или в модеме */
	for (i = 0; i < len; i++) {
	    while (!(*pUART1_LSR & THRE));
	    *pUART1_THR = str[i];
	    ssync();
	    uart1_cnt.tx_pack++;
	}
    }
    return i;
}


/* Счетчики обмена выдать */
#pragma section("FLASH_code")
void UART1_get_count(DEV_UART_COUNTS * cnt)
{
    memcpy(cnt, &uart1_cnt, sizeof(DEV_UART_COUNTS));
}


/** 
 * Обслуживание прерываний - или прием или передача, не одновременно!
 * Ошибки стирать по отдельности!
 */
section("L1_code")
void UART1_STATUS_ISR(void)
{
    u16 stat, err;
    volatile u8 byte;		/* Принятый байт */

    /* Приняли статус */
    stat = *pUART1_LSR;
    ssync();

    /* Сначала проверим ошибки - добавить обработок для всех ошыбок, биты 1..4 */
    err = stat & 0x1E;
    if (err) {
	uart1_cnt.rx_stat_err++;
	*pUART1_LSR |= err;	/* снимаем ошибки (с "или" ?????) */
	byte = *pUART1_RBR;	/* Читаем из буфера             */
	ssync();
    }

    if (stat & DR) {
	byte = *pUART1_RBR;
	ssync();

	/* Вызываем функцию callback */
	if (uart1_xchg_struct.rx_call_back_func != NULL) {
	    uart1_xchg_struct.rx_call_back_func(byte);
	    uart1_cnt.rx_pack++;
	}

    } else if (stat & THRE) {	/*  Передаем буфер */
	if (uart1_xchg_struct.tx_call_back_func != NULL) {
	    uart1_xchg_struct.tx_call_back_func();
	    uart1_cnt.tx_pack++;
	}
    }
}
