/************************************************************************
 * В этом файле описываются обработчики прерываний для каждого приоритета 
 * Мы можем использовать приоритеты от 7-го до 13 включительно.
 * IVG14 и IVG15 - программные прерывания: на IVG14 работает сигнал
 * на IVG15 наша программа 
 *********************************************************************/
#include "ads1282.h"
#include "timer1.h"
#include "timer2.h"
#include "timer3.h"
#include "timer4.h"
#include "power.h"
#include "ports.h"
#include "uart0.h"
#include "uart1.h"
#include "irq.h"
#include "spi0.h"
#include "rsi.h"



/* Обработчик прерываний для приоритета IVG7: 
 * 1 - Таймер 1 - самый высокий приоритет, PRTC! 
 * 2 - Подстройка генератора 4 МГц
 */
EX_INTERRUPT_HANDLER(section("L1_code")IVG7_ISR)
{
	/* Таймер 1 - секундное прерывание  */
	if (*pSIC_ISR1 & IRQ_TIMER1) {
		TIMER1_ISR();
	}
#if QUARTZ_CLK_FREQ==(19200000)
	/* Таймер 3 подсчет периода */
	if (*pSIC_ISR1 & IRQ_TIMER3) {
		TIMER3_ISR();
	}
/*#elif QUARTZ_CLK_FREQ==(8192000)*/

#endif
}


/* Обработчик прерывания для приоритета IVG8:
 * 1 - порт F маска A 
 * Больше ничего сюда не ставить!
 */
EX_INTERRUPT_HANDLER(section("L1_code")IVG8_ISR)
{
	if (*pSIC_ISR0 & IRQ_PFA_PORTF) {
		ADS1282_ISR();
	}
}


/* Обработчик прерываний для приоритета IVG9:
 * 1 - UART0 Rx строки  NMEA
 * можно во flash
 */
#pragma section("FLASH_code")
EX_INTERRUPT_HANDLER(IVG9_ISR)
{
	/* UART0 STATUS Rx interrupt */
	if (*pSIC_ISR0 & IRQ_UART0_ERR) {
		UART0_STATUS_ISR();
	}
}


/* Обработчик прерываний для приоритета IVG10:
 * 1 - UART1
 */
EX_INTERRUPT_HANDLER(IVG10_ISR)
{
	/* USART1 STATUS Rx interrupt */
	if (*pSIC_ISR0 & IRQ_UART1_ERR) {
		UART1_STATUS_ISR();
	}
}


/* Обработчик прерываний для приоритета IVG11:
 * 1 - таймер 2 - счет задержек
 * можно во flash
 */
#pragma section("FLASH_code")
EX_INTERRUPT_HANDLER(IVG11_ISR)
{
	/* Таймер 2 подсчет задержек */
	if (*pSIC_ISR1 & IRQ_TIMER2) {
		TIMER2_ISR();
	}
}

/* Обработчик прерываний для приоритета IVG12:
 * 1 - 1PPS на вход таймера 4 
 */
EX_INTERRUPT_HANDLER(section("L1_code")IVG12_ISR)
{
	/* Счетный вход 1PPS от Сильваны */
	if (*pSIC_ISR1 & IRQ_TIMER4) {
		TIMER4_ISR();
	}
}

/* Обработчик прерываний для приоритета IVG13: выключение
 * 1. Включение - выключение, порт G маска А  
 * можно во flash
 */
#pragma section("FLASH_code")
EX_INTERRUPT_HANDLER(IVG13_ISR)
{
	if (*pSIC_ISR1 & IRQ_PFA_PORTG) {
        	POWER_MAGNET_ISR();
	}

	if (*pSIC_ISR1 & IRQ_TIMER5) {
		BOUNCE_TIMER_ISR();
	}
}


/* Установить вектор */
#pragma section("FLASH_code")
bool IRQ_register_vector(int vec)
{
	bool result;
	switch (vec) {
	case 7:
		register_handler(ik_ivg7, IVG7_ISR);
		result = true;
		break;

	case 8:
		register_handler(ik_ivg8, IVG8_ISR);
		result = true;
		break;

	case 9:
		register_handler(ik_ivg9, IVG9_ISR);
		result = true;
		break;

	case 10:
		register_handler(ik_ivg10, IVG10_ISR);
		result = true;
		break;

	case 11:
		register_handler(ik_ivg11, IVG11_ISR);
		result = true;
		break;

	case 12:
		register_handler(ik_ivg12, IVG12_ISR);
		result = true;
		break;

	case 13:
		register_handler(ik_ivg13, IVG13_ISR);
		result = true;
		break;

	default:
		result = false;
		break;
	}

	return result;
}


/* Убрать вектор  */
#pragma section("FLASH_code")
bool IRQ_unregister_vector(int vec)
{
	bool result;
	switch (vec) {
	case 7:
		interrupt(ik_ivg7, SIG_IGN);
		result = true;
		break;

	case 8:
		interrupt(ik_ivg8, SIG_IGN);
		result = true;
		break;

	case 9:
		interrupt(ik_ivg9, SIG_IGN);
		result = true;
		break;

	case 10:
		interrupt(ik_ivg10, SIG_IGN);
		result = true;
		break;

	case 11:
		interrupt(ik_ivg11, SIG_IGN);
		result = true;
		break;

	case 12:
		interrupt(ik_ivg12, SIG_IGN);
		result = true;
		break;

	case 13:
		interrupt(ik_ivg13, SIG_IGN);
		result = true;
		break;

	default:
		result = false;
		break;
	}

	return result;
}

