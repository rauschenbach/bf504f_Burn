/* Таймер 1 теперь на самом высоком пользовательском приоритете!
 * Ведет календарь, этим тамером пользуеца служба времени GNS
 */
#include "timer2.h"
#include "irq.h"


/************************************************************************
 * 	Статические переменные
 ************************************************************************/

/* Ведение календаря + флаги синхронизации */
static struct {
    u32 sec;			/* Секунды */
    u32 work_time;
} timer_tick_struct;


/**
 * 2-й секундный таймер ведет календарь, период не передаем а задаем сразу
 */
#pragma section("FLASH_code")
bool TIMER2_init(void)
{
	/* Шитать до PERIOD, прерывания  */
	*pTIMER2_CONFIG = PERIOD_CNT | IRQ_ENA | PWM_OUT;
	*pTIMER2_PERIOD = TIMER_PERIOD; /* 1 секунда */
	*pTIMER2_WIDTH =  202;	/* т.ж. TIMER_PERIOD / TIMER_US_DIVIDER * 4.225 --- 202 */


	/* Обнулим структуру */
	timer_tick_struct.sec = 0;
	timer_tick_struct.work_time = 0;

	IRQ_register_vector(TIM2_VECTOR_NUM);	/* Регистрируем обработчик на IVG11 */

	/* перепрограммируем TIMER2 на приоритет IVG11 */
	*pSIC_IMASK1 |= IRQ_TIMER2;
	*pSIC_IAR4 &= 0xFFFFF0FF;
	*pSIC_IAR4 |= 0x00000400;

	/* Запустили */
	*pTIMER_ENABLE = TIMEN2;
	ssync();
        return true;			/* Все ОК  */
}


/**
 * Timer2 ISR приходит каждую секунду!
 */
#pragma section("FLASH_code")
void TIMER2_ISR(void)
{
    /* Подтвердим прерывание - после этого счет начинается заново */
    *pTIMER_STATUS = TIMIL2;
    ssync();

    timer_tick_struct.sec++;		/* Счетчик секунд */
    timer_tick_struct.work_time++;	/* Счетчик рабочего времени - он считается с нуля */
}



/**
 * Установить секундный счетчик, вызывающая функция сама должна следить,
 * чтобы это было во вне прерывания 
 * это так же означает что таймер 1 запущен!
 */
#pragma section("FLASH_code")
void TIMER2_set_sec(u32 sec)
{
    timer_tick_struct.sec = sec;
}


/**
 *  Получить счетчик секунд от нашего таймера 
 */
#pragma section("FLASH_code")
u32 TIMER2_get_sec(void)
{
    return timer_tick_struct.sec;
}


/**
 * Получить длинное время от таймера sec + нс
 * return: u64.  PS: нам может не хватить 32 разрядов на число
 */
#pragma section("FLASH_code")
s64 TIMER2_get_long_time(void)
{
    register u32 cnt0, sec, cnt1;

    cnt0 = *pTIMER2_COUNTER;
    sec = timer_tick_struct.sec;
    cnt1 = *pTIMER2_COUNTER;
    ssync();

    if (cnt1 < cnt0)
	sec++;

    return  (u64) sec * TIMER_NS_DIVIDER + (u64) cnt1 * TIMER_MS_DIVIDER / TIMER_FREQ_MHZ;
}
