/***************************************************************************
 * ТАЙМЕР 3 считает число импульсов частоты SCLK между 2-мя прерываниями
 * таймера 1 (свой PPS)
 * Этот таймер можно запустить из FLASH
 **************************************************************************/
#include <string.h>
#include "timer3.h"
#include "timer1.h"
#include "pll.h"
#include "irq.h"


#if QUARTZ_CLK_FREQ==(19200000)

/* Сколько тиков частоты 4.096 МГц будет в этом числе? 
 * делители нормально работают для SCLK 24000000 и 48000000
 */
#define QUARTZ_F4MHZ_FREQ    	(4096000)
#define	TIM3_MAX_ERROR		(50)
#define	TIM3_COUNT_MUL  	((long)QUARTZ_F4MHZ_FREQ / 64000)
#define TIM3_COUNT_DIV          ((long)TIMER_PERIOD / 64000)


/************************************************************************
 * 	Статические переменные
 ************************************************************************/
/* Считаем прерывания  */
static struct {
    s32 timer_sec;		/* Секунды */
    s32 curr_err;		/* старая и новая ошибки  */
    s32 last_err;
    s32 last_count;
    s32 curr_count;
    s32 phase_err;
    s32 timer_per;
    u8   sync_num;
    bool sync_req;
    bool sync_ok;
    bool tune_ok;		/* частота и фаза подстроена! */
} timer_sync_struct;


/************************************************************************
 * 	Статические функции - видны тока здесь
 ************************************************************************/

/**
 * Третий таймер считает частоту 4 МГц в режиме EXT_CLK 
 */
#pragma section("FLASH_code")
bool TIMER3_init(void)
{
    /* Таймерный порт PF13 на вход  */
    *pPORTF_FER |= PF13;
    *pPORTFIO_DIR &= ~PF13;


    /* Включаем 2 функцию для TMR3 на PF13  01b - вход таймера3 */
    *pPORTF_MUX |= (1 << 12);	/* 1 на 12 */
    *pPORTF_MUX &= ~(1 << 13);	/* 0 на 13 */
    ssync();

    /* Счетный режим + IRQ */
    *pTIMER3_CONFIG = IRQ_ENA | PULSE_HI | EXT_CLK;


    /* Считаем до этого значения */
    *pTIMER3_PERIOD = QUARTZ_F4MHZ_FREQ;

    /* Ее не вызываем, пока не установим */
    memset(&timer_sync_struct, 0, sizeof(timer_sync_struct));

    /* перепрограммируем TIMER3 на на 7 приоритет вместе с таймером 1. Будет прерываться когда установица таймер 1 */
    *pSIC_IMASK1 |= IRQ_TIMER3;
    *pSIC_IAR4 &= 0xFFFF0FFF;
    *pSIC_IAR4 |= 0x00000000;

    /* Сразу запускаем  */
    *pTIMER_ENABLE = TIMEN3;
    ssync();
    return true;
}

/**
 * TIMER3 ISR должен приходить при наборе 4096000 тиков
 * В момент запуска таймер должен один раз подстроиться на середину
 * периода таймера 1, потом уже проводим подстройку кварца
 */
section("L1_code")
void TIMER3_ISR(void)
{
   register volatile u32 count;

    /* Подтвердим прерывание - после этого счет начинается заново */
    *pTIMER_STATUS = TIMIL3;
    ssync();

   /* Приняли метку - сколько таймер1 уже насчитал */
    count = (TIMER1_is_run()? *pTIMER1_COUNTER : *pTIMER2_COUNTER);

    /* Этот блок должен быть внутри прерывания! 
     * нужно подстроить этот таймер на середину 1-го таймера */
    if (timer_sync_struct.sync_req) {
	int period; 

	do {
	    if (timer_sync_struct.sync_num == 0) {

		/* Не попасть в момент переключения */
		if (count > ((int)TIMER_PERIOD - TIM3_MAX_ERROR * 2))
		    break;	/* Попробуем еще раз */

		/* На сколько увеличить период счета, чтобы быть на середине таймера 1
		 * т.е. на 24000000 
		 * если diff > 0, - увеличить период таймера 3, если diff < 0 - уменьшить
		 * сделать приведение типа, или 'timer_sec' как знаковое!    
		 */
/*		period = (((long)TIMER_HALF_PERIOD - (long) count) * 32 / 375) + QUARTZ_F4MHZ_FREQ;*/ // 48МГц
/*		period = (((long)TIMER_HALF_PERIOD - (long) count) * 64 / 375) + QUARTZ_F4MHZ_FREQ;*/ // 24МГц

		period = (((long)TIMER_HALF_PERIOD - (long) count) * TIM3_COUNT_MUL / TIM3_COUNT_DIV) + QUARTZ_F4MHZ_FREQ;

		/* diff не может быть > половины периода таймера 3! */
		*pTIMER3_PERIOD = period;
		ssync();
	    }
	    timer_sync_struct.sync_num++;

	    /* Возвращаем период обратно */
	    if (timer_sync_struct.sync_num == 2) {
		*pTIMER3_PERIOD = QUARTZ_F4MHZ_FREQ;
		ssync();
	    }

	    if (timer_sync_struct.sync_num >= 4) {
		timer_sync_struct.sync_num = 0;
		timer_sync_struct.sync_req = 0;
		timer_sync_struct.sync_ok = true;	/* Подстроено */
	    }
	} while (0);
    }

    timer_sync_struct.last_count = timer_sync_struct.curr_count;
    timer_sync_struct.curr_count = count;

    /* Если ошибка error < 0 - таймер 3 идет быстрее чем таймер 1, т.е он быстрее снимает метки. если ошибка error > 0 - таймер 3 идет медленнее чем таймер 1 */
    timer_sync_struct.last_err = timer_sync_struct.curr_err;
    timer_sync_struct.curr_err = count - timer_sync_struct.last_count;
    timer_sync_struct.timer_per = SCLK_VALUE + timer_sync_struct.curr_err;	/* Период */

    /* Если фаза > 0, то таймер 3 опережает по фазе середину таймера 1. если фаза < 0, то таймер 3 отстает по фазе от середины счета таймера 1 */
    timer_sync_struct.phase_err = SCLK_VALUE / 2 - count;

    /* Ошибки частоты и фазы должна быть меньше 50 для запуска АЦП порядок в if() именно такой! */
    if (!timer_sync_struct.tune_ok && (abs(timer_sync_struct.curr_err) < TIM3_MAX_ERROR) && 
				      (abs(timer_sync_struct.last_err) < TIM3_MAX_ERROR) && 
				      (abs(timer_sync_struct.phase_err) < TIM3_MAX_ERROR)) {
	timer_sync_struct.tune_ok = true;
    }

    /* собрал и подсчитал - каждую секунду */
    timer_sync_struct.timer_sec++;
}


/* При получении OK - уже подстроили */
#pragma section("FLASH_code")
bool TIMER3_is_shift_ok(void)
{
    return timer_sync_struct.sync_ok;
}

/**
 * Таймер идет, запущен и синхронизирован? 
 */
#pragma section("FLASH_code")
bool TIMER3_is_run(void)
{
    bool res;
    res = (*pTIMER_STATUS & TRUN3) ? true : false;
    return res;
}


/* Сдвинуть фазу таймера 3 по отношению к таймеру 1 */
section("L1_code")
void TIMER3_shift_phase(int sec)
{
    /* Сдвиг импульса */
    timer_sync_struct.sync_req = true;	/* новый период */
    timer_sync_struct.sync_ok = false;	/* не подстроено еще */
    timer_sync_struct.timer_sec = sec;  /* поставим время в этот таймер */
}


/**
 * Ждать срабатывания таймера 3, если не установлен вектор!
 */
section("L1_code")
u32 TIMER3_get_counter(void)
{
    return *pTIMER3_COUNTER;
}

/**
 * Расчитанный период таймера!
 */
section("L1_code")
s32 TIMER3_get_period(void)
{
    return timer_sync_struct.timer_per;
}


section("L1_code")
s32 TIMER3_get_freq_err(void)
{
    return timer_sync_struct.curr_err;
}

section("L1_code")
s32 TIMER3_get_phase_err(void)
{
    return timer_sync_struct.phase_err;
}


section("L1_code")
bool TIMER3_is_tuned_ok(void)
{
 return timer_sync_struct.tune_ok;
}

section("L1_code")
s32 TIMER3_get_sec_ticks(void)
{
    return timer_sync_struct.timer_sec;
}


#endif /* QUARTZ_CLK_FREQ==(19200000) - для генератора 19.2 МГц */

