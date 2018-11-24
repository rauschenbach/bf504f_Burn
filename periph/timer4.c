/***************************************************************************
 *  ТАЙМЕР 4 считает число импульсов частоты SCLK между 2-мя импульсами PPS
 **************************************************************************/
#include <string.h>
#include "timer4.h"
#include "timer1.h"
#include "gps.h"
#include "irq.h"


/************************************************************************
 * 	Статические переменные
 ************************************************************************/

/* Флаги  на синхронизацию */
static struct {
    u32 sec;			/* Секунды */
    u32 count;			/* Счетчик количества PPS */
    u32 tick_buf[TIM4_BUF_SIZE];	/* Будет набирать тики таймера 4 */
    u64 t1;			/* Время прихода PPS по первому таймеру */
    u64 t4;			/* Время прихода PPS по себе */
} timer_sync_struct;


/* 4-й таймер - счетчик импульсов 1PPS */
#pragma section("FLASH_code")
void TIMER4_config(void)
{
    /* Таймерный порт на вход для PG2 */
    *pPORTG_FER |= PG2;
    *pPORTGIO_DIR &= ~PG2;

    /* Включаем 2 функцию для TMR4 на PG2 */
    *pPORTG_MUX &= ~(1 << 5);	/* 0 на 5 */
    *pPORTG_MUX |= (1 << 4);	/* 1 на 4 */

    memset(&timer_sync_struct, 0, sizeof(timer_sync_struct));

    /* считать период следования импульсов на входе PG2 и выдавать прерывания */
    *pTIMER4_CONFIG = WDTH_CAP | PULSE_HI | PERIOD_CNT | IRQ_ENA;
    ssync();

    *pTIMER_ENABLE = TIMEN4;
    ssync();
}


/* Таймер4 запущен?  */
bool TIMER4_is_run(void)
{
    return (*pTIMER_STATUS & TRUN4) ? true : false;
}


/* Установить функцию и зарегистрировать вектор */
#pragma section("FLASH_code")
void TIMER4_init_vector(void)
{
    IRQ_register_vector(TIM4_VECTOR_NUM);	/* Прерывание от 4 - го таймера  на IVG12 */
    TIMER4_enable_irq();
}

/** 
 *  Отключть прерывания таймера4 и зануляем функцию - Убрать callback функцию 
 */
#pragma section("FLASH_code")
void TIMER4_del_vector(void)
{
    IRQ_unregister_vector(TIM4_VECTOR_NUM);
}

/**
 * Считает количество импульсов SCLK между двумя фронтами 1PPS 
 * Будем мерять дрифт здесь относительно прихода импулься 1PPS.
 */
section("L1_code")
void TIMER4_ISR(void)
{
    register u32 cnt, per;

    *pTIMER_STATUS = TIMIL4;	/* Подтвердим прерывания */
    ssync();

    cnt = *pTIMER4_COUNTER;
    per = *pTIMER4_PERIOD;
    ssync();

    /* Время прихода PPS - еще более честно (с счетчиком, хотя он очень мал)! */
    timer_sync_struct.t1 = TIMER1_get_long_time();


    timer_sync_struct.sec = gps_get_nmea_time() + 1;	/* Время PPS + 1 c */
    timer_sync_struct.t4 = (u64) timer_sync_struct.sec * TIMER_NS_DIVIDER + 
                           (u64) cnt * TIMER_NS_DIVIDER / ((per == 0) ? TIMER_PERIOD : per);

    /* Собираем буфер. считает длительность м/у 2-мя импульсами */
    timer_sync_struct.tick_buf[timer_sync_struct.count++ % TIM4_BUF_SIZE] = per;
}

/**
 * Получить буфер с данными. Не проверяем, если превышение
 */
#pragma section("FLASH_code")
void TIMER4_get_tick_buf(u32 * buf, int num)
{
    int i;

    for (i = 0; i < num; i++) {
	buf[i] = timer_sync_struct.tick_buf[i];	/* Скопируем данные */
	if (i >= TIM4_BUF_SIZE)
	    break;
    }
}


/**
 * Получить сумму значений буфера
 */
#pragma section("FLASH_code")
u32 TIMER4_get_ticks_buf_sum(void)
{
    int i;
    u32 sum = 0;

    for (i = 0; i < TIM4_BUF_SIZE; i++) {
	sum += timer_sync_struct.tick_buf[i];	/* Скопируем данные */
    }
  return sum;
}


/**
 * Получить длинное время от таймера в наносекундах 
 * return: s64.  PS: нам может не хватить 32 разрядов на число
 * Если время получаем в "нехороший" момент - вернуть 0
 */
section("L1_code")
s64 TIMER4_get_long_time(void)
{
    register u32 cnt0, sec, cnt1, per;

    cnt0 = *pTIMER4_COUNTER;
    ssync();

    sec = timer_sync_struct.sec;
    cnt1 = *pTIMER4_COUNTER;
    per = *pTIMER4_PERIOD;
    ssync();

    /* Если берем время в самый момент переключения */
    if ((int) cnt1 < (int) cnt0) {
	sec = timer_sync_struct.sec;	// Еще раз прочитать!
    }

    if (per)
	return ((u64) sec * TIMER_NS_DIVIDER + (u64) cnt1 * TIMER_NS_DIVIDER / per);
    else
	return ((u64) sec * TIMER_NS_DIVIDER + (u64) cnt1 * TIMER_MS_DIVIDER / TIMER_FREQ_MHZ);
}



/**
 * Время прихода PPS
 */
#pragma section("FLASH_code")
void TIMER4_time_pps_signal(u64 * t1, u64 * t4)
{
    if (t1 != NULL)
	*t1 = timer_sync_struct.t1;
    if (t4 != NULL)
	*t4 = timer_sync_struct.t4;
}
