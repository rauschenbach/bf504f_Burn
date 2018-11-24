/******************************************************************************************   
 * Включение - выключение, здесь же используется таймер,
 * который запускается только в момент прерываний
 *****************************************************************************************/
#include "utils.h"
#include "power.h"
#include "main.h"
#include "rele.h"
#include "irq.h"
#include "led.h"
#include "pll.h"
#include "eeprom.h"
#include "log.h"

#define BUTPOW_PUSHED		PG15
#define POWER_VECTOR_NUM	POW_TIM5_VECTOR_NUM
#define BOUNCE_TIMER_PERIOD	TIMER_PERIOD	/* 1 с */

/******************************************************************************************
 * Статические функции для выключателя
 *******************************************************************************************/
static inline void inner_timer_start(void);
static inline void inner_timer_stop(void);

/* Не включено изначально */
static struct {
    int  tick;
    u8   req_cnt;
    u8   power_led;
    bool init_ok;
} BOUNCE_POWER_STRUCT;


/* Включение прибора по алгоритму, таймер 2 уже должен работать!*/
#pragma section("FLASH_code")
int POWER_on(void)
{
    int rst;

    LED_on(LED_POWER);		/* Зажигаем светодиод */
    delay_ms(150);
    RELE_on(RELEPOW);		/* Замыкаем реле-в загрузчике реле уже сработало!!! */
    LED_off(LED_POWER);		/* Зажигаем светодиод */

    rst = read_reset_cause_from_eeprom();	/* Причина ресета */
    delay_ms(50);

#if !defined	JTAG_DEBUG   /* Не проверяем при подключении из студии */
    /*  Бесконечный цыкл-выключится, когда магнит уберут!   */
    if (rst != CAUSE_EXT_RESET && rst != CAUSE_WDT_RESET) {

	/* Отключаем функции для порта PG15 и делаем его на вход - пин будет читаться как 1 при перепаде в 0 */
	*pPORTG_FER &= ~BUTPOW_PUSHED;
	*pPORTGIO_DIR &= ~BUTPOW_PUSHED;
	*pPORTGIO_INEN |= BUTPOW_PUSHED;
	*pPORTGIO_POLAR |= BUTPOW_PUSHED;
	*pPORTGIO_EDGE &= ~BUTPOW_PUSHED;
	ssync();


	/* Если разомкнули геркон в течении 3-х секунд  */
	if (!(*pPORTGIO & BUTPOW_PUSHED)) {
	    POWER_off(0, false);	// не записывать причину reset
	    while (1);		/*  Бесконечный цыкл-выключится, когда магнит уберут!   */
	}
    }
#endif
    RELE_off(RELEBURN);		/* Выключить реле пережыга */

    return rst;			/* Причину сброса или выключения возвращаем */
}



/* Выключение прибора обычное */
#pragma section("FLASH_code")
void POWER_off(int cause, bool modem)
{
  if(cause != 0) {
      log_write_log_file("INFO: Power Off\n");
      write_reset_cause_to_eeprom(cause);	/* Запишем причину сброса */
      print_reset_cause(cause);
      write_work_time();			/* записать время работы */
   }

    if (modem) {
	log_write_log_file("INFO: Power Off Modem\n");
	log_close_log_file();
	RELE_off(RELEAM);	/* Модем выключить также */
    }



#if !defined	JTAG_DEBUG
    RELE_off(RELEPOW);
#endif

/*  Бесконечный цыкл - выключится, когда магнит уберут! Должно быть вижно если выключатель сломан.*/
   do {
      LED_toggle(LED_POWER);		
      delay_ms(250);
    } while (1);
}



/**
 * Инициализация для включения-выключения питания 
 * Порты PORTG - не путать! 
 */
#pragma section("FLASH_code")
void POWER_init(void)
{
    IRQ_unregister_vector(POWER_VECTOR_NUM);	/* Отрегистрируем обработчик */
    LED_off(LED_POWER);

    /* Отключаем функции для порта PG15 - делаем их на вход */
    *pPORTG_FER &= ~BUTPOW_PUSHED;
    *pPORTGIO_DIR &= ~BUTPOW_PUSHED;
    *pPORTGIO_INEN |= BUTPOW_PUSHED;
    *pPORTGIO_POLAR |= BUTPOW_PUSHED;
    *pPORTGIO_EDGE &= ~BUTPOW_PUSHED;
    ssync();

    /* Прерывание возникнет по уровню  */
    *pPORTGIO_POLAR |= BUTPOW_PUSHED;

    /* Установим прерывание по событию: маска A для этого порта */
    *pPORTGIO_MASKA_SET |= BUTPOW_PUSHED;

    /* и перепрограммируем его на на 13 приоритет */
    *pSIC_IAR5 &= 0xFFFFFFF0;
    *pSIC_IAR5 |= 0x00000006;
    *pSIC_IMASK1 |= IRQ_PFA_PORTG;
    ssync();

    /* Счетный таймер инициализируем, но не запускаем + ставим вектор 
     * Шитать до BOUNCE_TIMER_PERIOD, прерывания  */
    *pTIMER5_CONFIG = PERIOD_CNT | IRQ_ENA | PWM_OUT;
    *pTIMER5_PERIOD = BOUNCE_TIMER_PERIOD / 10;	/* по 100 ms */
    *pTIMER5_WIDTH = BOUNCE_TIMER_PERIOD / 10000;	/* 0.1 ms */
    ssync();

    BOUNCE_POWER_STRUCT.req_cnt = 0;
    BOUNCE_POWER_STRUCT.tick = 20;

    /* Регистрируем обработчик на IVG13 */
    IRQ_register_vector(POWER_VECTOR_NUM);

    /* перепрограммируем TIMER5 на приоритет IVG13 */
    *pSIC_IMASK1 |= IRQ_TIMER5;
    *pSIC_IAR4 &= 0xFF0FFFFF;
    *pSIC_IAR4 |= 0x00600000;
    ssync();
}


/* Прерывание, для включения или выключения магнитом */
#pragma section("FLASH_code")
void POWER_MAGNET_ISR(void)
{
    BOUNCE_POWER_STRUCT.power_led = LED_get_power_led_state();	/* Сохраняем состояние красной лампы */
    inner_timer_start();	/* Запускаем таймер на счет */
    *pPORTGIO_MASKA_CLEAR = BUTPOW_PUSHED;	/* Подтвердим прерывания в самом конце */
    *pPORTGIO_CLEAR = BUTPOW_PUSHED;
}


/* Читаем состояние геркона. Если геркон не замкнут - продолжаем обычную работу */
#pragma section("FLASH_code")
void BOUNCE_TIMER_ISR(void)
{
    /* Подтвердим прерывание - чтоб здесь не повиснуть  */
    *pTIMER_STATUS = TIMIL5;
    ssync();

    BOUNCE_POWER_STRUCT.tick++;	// Набираем счетчик

    /* Если не замкнут хоть миллисекунду - перезапускаем без убирания вектора */
    if (!(*pPORTGIO & BUTPOW_PUSHED)) {
	inner_timer_stop();	/* Перезапускаем систему выключения только если уберем магнит */
	*pPORTGIO_MASKA_SET = BUTPOW_PUSHED;
	ssync();
    }
    /* Через 1.5 секунды гасим лампу возвращаем в исходное - посмотреть, 
     * если будет изменение состояния, лампочка может не попасть в правильное
     ** Прочитать в каком режиме находимся
     */
    if (BOUNCE_POWER_STRUCT.tick == 15 && *pPORTGIO & BUTPOW_PUSHED) {
	LED_set_power_led_state(BOUNCE_POWER_STRUCT.power_led);
	send_magnet_request();	/* Запрос на выключение или переключение */
    }
}

/** 
 * Запускается 5-й таймер, период 100 мс (TIMER_PERIOD / 10)
*/
#pragma section("FLASH_code")
static inline void inner_timer_start(void)
{
    *pTIMER_ENABLE = TIMEN5;	/* Запустили */
    ssync();

    /* Ставим красный */
    if (BOUNCE_POWER_STRUCT.req_cnt)
	LED_set_power_led_state(LED_ON_STATE);
    BOUNCE_POWER_STRUCT.req_cnt++;
}

/**
 * Останавливает счет 
 */
#pragma section("FLASH_code")
static inline void inner_timer_stop(void)
{
    int state;

    *pTIMER_DISABLE = TIMEN5;	/* Стопим таймер */
    ssync();
    state = get_dev_state();


   /* Возвращаем лампы как было --- проверить на режимах если НЕ начало */
    if (state == DEV_ERROR_STATE || state == DEV_CHOOSE_MODE_STATE)
	LED_set_power_led_state(LED_QUICK_STATE);
    else
	LED_set_power_led_state( /*BOUNCE_POWER_STRUCT.power_led */ LED_OFF_STATE);	

    BOUNCE_POWER_STRUCT.tick = 0;	/* Обнулим структуру */
    BOUNCE_POWER_STRUCT.init_ok = false;
}
