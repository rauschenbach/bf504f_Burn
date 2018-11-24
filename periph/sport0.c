/******************************************************************************** 
 * SPORT0 предназначен для связи с атмегой - работает в режиме 
 * совместимости с протоколом SPI
 ********************************************************************************/
#include "sport0.h"
#include "utils.h"
#include "log.h"
#include "pll.h"

/* Частота спорта пока  для связи с атмегой - в файле config.h */
#define SPORT0_SCLK 		ATMEGA_BF_XCHG_SPEED


/* Инит. SPORT0 как в режиме SPI */
#pragma section("FLASH_code")
void SPORT0_config(void)
{
	u16 divider;

	/* 1. Настраиваем порты SPORT0: 1-я функция на порте F,
	 * PORTF_MUX на 1-й функции  нули - ~(3F)
	 * Чипселект на PF3 сигналом TFS0 
	 */
	*pPORTF_FER |= (PF0 | PF1 | PF2 | PF3 | PF4 | PF5);
	*pPORTF_MUX &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5));


	/* 2. Настраиваем регистр передачи, фаза 0 полярность 0  
	 * TCKFE = 0 
	 * ITCLK = 1 - Internal transmit clock select
	 * LTFS  = 1 - FrameSYNC (CS) Active Low
	 * DTYPE = 0 - нормальная операция
	 * TLSBIT= 0 - Transmit Bit Order, MSB первый
	 * TFSR  = 1 - требуеца импульс кадровой синхронизации для CS
	 * TCKFE = 0 - Clock Falling Edge Select типа CPOL = 0
	 * TSPEN  запишем позже! 
	 */
	*pSPORT0_TCR1 = TCKFE | LATFS | LTFS | TFSR | ITFS | ITCLK;
	*pSPORT0_TCR2 = 7;	/* длина передачи минус 1 */

	/* 3. Настраиваем регистр приёма, фаза 0 полярность 0  */
	*pSPORT0_RCR1 = RCKFE | LARFS | LRFS | RFSR;	/* RSPEN запишем позже! */
	*pSPORT0_RCR2 = 7;	/* 32 - 1; Длина приемника 32 бита - у нас такой АЦП */
	ssync();

	/* Расчитаем делитель и запишем его в регистры тактовой частоты приема и передачи */
	divider = (u16) (SCLK_VALUE / (SPORT0_SCLK * 2) - 1);

	/* 4. Делитель в оба регистра */
	*pSPORT0_TCLKDIV = divider;
	*pSPORT0_RCLKDIV = divider;

	/* 5. Делители для кадровой синхронизации ??? */
	*pSPORT0_TFSDIV = 7;
	 ssync();
}


/* Запись и чтение в одном флаконе */
#pragma section("L1_code")
u8 SPORT0_write_read(u8 data)
{
	u8 byte;
	s64 t1, t2;

	/* Разрешаем SPORT */
	SPORT0_enable();

	*pSPORT0_TX16 = (u8) data;
	ssync();	

	t1 = get_msec_ticks();

	/* Ждем данных */
	while (!(*pSPORT0_STAT & RXNE))	{
		t2 = get_msec_ticks();
		if((t2 - t1) > 100) { /* за 100 миллисекунд не прочиталось :( */
		  break; // return 0; 
		}		
	}
	
	byte = (u8) * pSPORT0_RX16;

	SPORT0_disable();	/* Выключаем SPORT */
	return byte;
}
