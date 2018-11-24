/************************************************************************************** 
 * Для процессора BF504F с внутренней флешкой 
 * CLKIN    = 19.2 МГц
 * умножение частоты с шагом для:
 * CCLK     =  288, 230.4, 192, ->96 МГц
 * SCLK     =  72, 64, 48, 32, ->24  МГц
 * потом можно будет добавить и дробные частоты, но частота SCLK должна быть целой!
 * Будут добавлены функцыи для управления энергопотреблением и сном
 * Выбрать частоты клоков из дефиницый
 * Пробуем такие значения!
 * Для частоты генератора 19.2 МГц берем частоту ядра в 60 МГц и частоту периферии в 24 МГц
 * CCLK = 192MHZ SCLK = 48MHZ
 **************************************************************************************/
#ifndef	__PLL_H__
#define __PLL_H__

#include "globdefs.h"
#include "config.h"




/* Причины сброса прибора - перечислены здесь. Из запишем в eeprom  */
#define		CAUSE_POWER_OFF		0x12345678	/* Выключение питания */
#define		CAUSE_EXT_RESET		0xabcdef90	/* Внешний ресет */
#define		CAUSE_BROWN_OUT		0xaa55aa55	/* Снижение питания */
#define		CAUSE_WDT_RESET		0x07070707	/* WDT reset (во время регистрации) */
#define		CAUSE_NO_LINK		0xE7E7E7E7	/* Нет связи - самовыключение прибора */
#define		CAUSE_UNKNOWN_RESET	0xFFFFFFFF	/* Неизвестная причина-выдернули питание */


/* Для генератора 19.2 МГц  */
#if QUARTZ_CLK_FREQ==(19200000)
/* Частота периферии проца = 24 МГц
 * 19.2 делим на 2 = 9.6 МГц - на вход PLL
 * MSEL[5:0] = 25 - получили VCO = 240 МГц из 9.6 
 * CSEL[1:0] = 4  - получили CCLK = VSO / 4 = 60МГц, 
 * SSEL[3:0] = 10 - получили SCLK = VSO / 10 = 24МГц */
#define 	SCLK_VALUE 		24000000
#define 	PLLCTL_VALUE        	((25 << 9) | 1)
#define 	PLLDIV_VALUE        	0x002A

/*
#elif QUARTZ_CLK_FREQ==(8192000)
// Частота периферии проца = 237568 МГц -- ОШЫБКА!!!
// MSEL[5:0] = 29 - получили VCO = 237.568 МГц из 8.192 
// CSEL[1:0] = b10  - получили CCLK = VSO / 4 = 59.392МГц, 
// SSEL[3:0] =   - получили SCLK = VSO / 10 = 24.576МГц 
#define 	SCLK_VALUE 	     23756800UL	
#define 	PLLCTL_VALUE         (29 << 9)
#define 	PLLDIV_VALUE         0x002A
*/
#elif QUARTZ_CLK_FREQ==(8192000)
/* Частота периферии проца = 24.576 МГц
 * MSEL[5:0] = 30 - получили VCO = 245.760 МГц из 8.192 
 * CSEL[1:0] = 0  - получили CCLK = VSO / 4 = 61.440 МГц, 
 * SSEL[3:0] = 10  - получили SCLK = VSO / 10 = 24.576 МГц */
#define 	SCLK_VALUE 	     24576000UL	
#define 	PLLCTL_VALUE         (30 << 9)
#define 	PLLDIV_VALUE         0x002A



#elif QUARTZ_CLK_FREQ==(4096000)
/* Частота периферии проца = 49.152 МГц
 * MSEL[5:0] = 48 - получили VCO = 196.608МГц из 4.096 
 * CSEL[1:0] = b10  - получили CCLK = VSO / 4 = 49.152МГц 
 * SSEL[3:0] = 8  - получили SCLK = VSO / 8 = 24.576МГц  */
#define 	SCLK_VALUE 	     49152000UL
#define 	PLLCTL_VALUE        (48 << 9)
#define 	PLLDIV_VALUE        0x0008

#else
#error "SCLK Value is not defined or incorrect!"
#endif


/* Эти значения для Power Managements */
#define 	PLLSTAT_VALUE       0x0000			/* NB: Только чтение!!!  */
#define 	PLLLOCKCNT_VALUE    0x0200			/* Через 512 тактов заснуть */
#define 	PLLVRCTL_VALUE      ((1 << 9)|(1 << 11))	/* Просыпаться по прерываниям на ногах PF8 и PF9 */

/* Частоту таймера определяем в PLL  */
#define 	TIMER_PERIOD 		(SCLK_VALUE)
#define		TIMER_HALF_PERIOD	(SCLK_VALUE / 2)
#define		TIMER_TUNE_SHIFT	(SCLK_VALUE / 1000)
#define 	TIMER_FREQ_MHZ		(SCLK_VALUE / 1000000)
#define		TIMER_NS_DIVIDER	(1000000000UL)
#define		TIMER_US_DIVIDER	(1000000)
#define		TIMER_MS_DIVIDER	(1000)


/* Это значение периода SCLK */
#define		SCLK_PERIOD_NS	(TIMER_NS_DIVIDER / TIMER_PERIOD)


void PLL_init(void);
void PLL_sleep(DEV_STATE_ENUM);
void PLL_reset(void);
void PLL_fullon(void);
void PLL_hibernate(DEV_STATE_ENUM);

#endif				/* pll.h */
