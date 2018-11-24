/* Таймер 1 теперь на самом высоком пользовательском приоритете!
 * Ведет календарь, этим тамером пользуеца служба времени GNS
 */
#include "timer1.h"
#include "gps.h"
#include "irq.h"

#define	TIM1_MAX_ERROR		(1000)

/************************************************************************
 * 	Статические переменные
 ************************************************************************/

/* Ведение календаря + флаги синхронизации */
static volatile struct {
    u32 sec;			/* Секунды */
    void (*call_back_func) (u32);	/* Указатель на функцыю, которую будем вызывать из прерывания таймера  */
    u16  check_err;		/* Нехорощые моменты переключения */
    bool sync_ok;		/* Таймер подстроен OK */
    bool run_ok;		/* период установлен OK */
} timer_tick_struct;


/**
 * Первый секундный таймер ведет календарь, период не передаем а задаем сразу, 
 * в качестве параметра передаем указатель на наш календарь
 * таймер будет запущен функцией ENABLE!!!
 */
#pragma section("FLASH_code")
int TIMER1_config(void)
{
    /* Выход ШИМ, считать до PERIOD, выход высокий, прерывания раз в секунду + Моргнуть лампой */
    *pTIMER1_CONFIG = PWM_OUT | PERIOD_CNT | IRQ_ENA /*| PULSE_HI */ ;

    /* Затираем структуры */
    timer_tick_struct.sec = 0;
    timer_tick_struct.check_err = 0;
    timer_tick_struct.call_back_func = NULL;	/* Ее не вызываем, пока не установим */
    timer_tick_struct.sync_ok = false;
    timer_tick_struct.run_ok = false;

    /* Считаем время:
     * 1 микросекунда - это SCLK_VALUE / TIMER_US_DIVIDER! 
     * 1 миллисекунда SCLK_VALUE / TIMER_MS_DIVIDER
     * 1 секунда это SCLK_VALUE
     */
    *pTIMER1_PERIOD = TIMER_PERIOD;	/* 1 секунда */
    *pTIMER1_WIDTH = 202;	/* т.ж. TIMER_PERIOD / TIMER_US_DIVIDER * 4.225 --- 202 */

    IRQ_register_vector(TIM1_TIM3_VECTOR_NUM);
    TIMER1_enable_irq();	/* Сдесь же все запустим */

    return 0;			/* Все ОК  */
}

/* Запуск без подстройки! */
#pragma section("FLASH_code")
void TIMER1_quick_start(long t)
{
    TIMER1_config();		/* Настриваем секундный таймер  */
    TIMER1_set_sec(t);
    timer_tick_struct.sync_ok = true;	/* Без синхронизации */
    TIMER1_enable();		/* и запускаем */
}

#pragma section("FLASH_code")
u32 TIMER1_get_error(void)
{
    return (u32)timer_tick_struct.check_err;
}


/**
 * Timer1 ISR приходит каждую секунду!
 * в этом таймере изменяем календарь и делаем все другие изменения
 * чтобы не было всяких разных наложений IRQ и пр.
 * Таймер TIMER4 считает быстрее!
 * Ничего сюда не ставить, а дрифт мерять в таймере 4 - это относится только к нему!!!
 */
section("L1_code")
void TIMER1_ISR(void)
{
    /* Подтвердим прерывание - после этого счет начинается заново */
    *pTIMER_STATUS = TIMIL1;
    ssync();

    /* Больше суда не заходить! */
    if (!timer_tick_struct.sync_ok) {
	timer_tick_struct.sync_ok = true;
	timer_tick_struct.run_ok = false;
	*pTIMER1_PERIOD = TIMER_PERIOD;	/* 1 секунда */
	ssync();
    }

    timer_tick_struct.sec++;	/* Счетчик секунд */

    /* Вызываем callback, если он установлен */
    if (timer_tick_struct.call_back_func != NULL)
	timer_tick_struct.call_back_func(timer_tick_struct.sec);
}



/**
 * Установить секундный счетчик, вызывающая функция сама должна следить,
 * чтобы это было во вне прерывания 
 * это так же означает что таймер 1 запущен!
 */
section("L1_code")
void TIMER1_set_sec(u32 sec)
{
    timer_tick_struct.sec = sec;
    timer_tick_struct.run_ok = true;
}


/**
 *  Получить счетчик секунд от нашего таймера 
 */
section("L1_code")
u32 TIMER1_get_sec(void)
{
    return timer_tick_struct.sec;
}

/**
 * Получить длинное время от таймера в наносекундах 
 * return: s64.  PS: нам может не хватить 32 разрядов на число
 * Если время получаем в "нехороший" момент - вернуть 0
 */
section("L1_code")
s64 TIMER1_get_long_time(void)
{
    register u32 cnt0, sec, cnt1;

    cnt0 = *pTIMER1_COUNTER;
    ssync();

    sec = timer_tick_struct.sec;
    cnt1 = *pTIMER1_COUNTER;
    ssync();

    /* Если берем время в самый момент переключения */
    if ((int) cnt1 < (int) cnt0) {
	timer_tick_struct.check_err++;	// Отметим, что это не очень хороший момент
	sec = timer_tick_struct.sec;	// Еще раз прочитать!
	cnt1 = *pTIMER1_COUNTER;
	ssync();
    }

    return ((u64) sec * TIMER_NS_DIVIDER + (u64) cnt1 * TIMER_MS_DIVIDER / TIMER_FREQ_MHZ);
}



/**
 * Установить функцию, это так же означает что таймер 1 запущен! 
 */
section("L1_code")
void TIMER1_set_callback(void *ptr)
{
    timer_tick_struct.call_back_func = (void (*)(u32)) ptr;
}


/**
 * Убрать callback функцию 
 */
#pragma section("FLASH_code")
void TIMER1_del_callback(void)
{
    timer_tick_struct.call_back_func = NULL;
}

/**
 * Таймер идет, запущен и синхронизирован? 
 */
section("L1_code")
bool TIMER1_is_run(void)
{
    bool res;
    res = (*pTIMER_STATUS & TRUN1) ? true : false;
    res &= (timer_tick_struct.sync_ok);
    res &= (timer_tick_struct.run_ok);
    return res;
}


/**
 * Если стоит callback
 */
section("L1_code")
bool TIMER1_is_callback(void)
{
    return ((timer_tick_struct.call_back_func == NULL) ? false : true);
}


/**
 * Выдать внутренний счетчик 1-го таймера
 */
section("L1_code")
u32 TIMER1_get_counter(void)
{
    register u32 cnt;

    cnt = *pTIMER1_COUNTER;
    ssync();
    return cnt;
}


/**
 * Разница импульсов PPS в тиках таймера 1. Для перевода в нс = ticks * 20.8333333
 * ОШИБКА! Таймер 4 считает не до SCLK_VALUE, а до чего угодно (SCLK_VALUE +- 50)!
 * величина чего он насчитал будет храниться TIMER4_PERIOD
 * в t1 и t4 запишутся таймеры
 */
section("L1_code")
int TIMER1_get_drift(u32 * pT1, u32 * pT4)
{
    register long t1, t4;
    long drift0, drift1;
    s64 temp;

    /* Если таймер не запущен  */
    if (!(*pTIMER_STATUS & TRUN4))
	return -1;

    t1 = *pTIMER1_COUNTER;
    t4 = *pTIMER4_COUNTER - 7;

    /* Записать в таймеры */
    if (pT1 != NULL && pT4 != NULL) {
	*pT1 = t1;
	*pT4 = t4;
    }

    /* Не попасть в момент переключения - таймеры на краяах */
    drift0 = t1 - t4;
    if (t1 < t4)
	t1 += TIMER_PERIOD;
    else
	t4 += TIMER_PERIOD;

    drift1 = t1 - t4;

    /* Число тиков переводим в наносекунды */
    temp = abs(drift0) < abs(drift1) ? drift0 : drift1;
    temp = (temp * TIMER_MS_DIVIDER) / TIMER_FREQ_MHZ;	/* Перевести в нс */

    return temp;
}

/**
 * Подождем пока таймер 1 находится дальше середины цикла.
 * PPS->|__xxxxxxxxxx__NMEA__________________________________|<-PPS
 * Должен быть ^ здесь (где xxx - перед приходом NMEA) чтобы взять нормальное время
 */
section("L1_code")
int TIMER1_sync_timer(void)
{
    int sec;

    while (TIMER1_get_counter() > TIM1_MAX_ERROR) {
	ssync();
    }

    sec = gps_get_nmea_time() + 1;	/* Наш таймер запущен - поставим в него время из NMEA + секунда */
    TIMER1_set_sec(sec);	/* время синхронизации сразу в таймер */

    return sec;
}
