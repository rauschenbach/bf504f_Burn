#include "tests.h"
#include "main.h"
#include "eeprom.h"
#include "ports.h"
#include "bq32k.h"
#include "ads1282.h"
#include "xpander.h"
#include "bmp085.h"
#include "lsm303.h"
#include "timer1.h"
#include "timer2.h"
#include "timer3.h"
#include "timer4.h"
#include "utils.h"
#include "gps.h"
#include "log.h"
#include "led.h"
#include "irq.h"
#include "dac.h"
#include "pll.h"



/**
 * Тест всех АЦП
 */
#pragma section("FLASH_code")
bool test_adc(void *in)
{
    DEV_STATUS_STRUCT *status;
    ADS1282_Params par;
    ADS1282_ERROR_STRUCT err;
    bool res = false;
    int i;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (in != NULL) {
	    status = (DEV_STATUS_STRUCT *) in;
	    status->st_main |= 0x10;	// регистратор неисправен 
	    status->st_adc = 0;	// не тестировали

	    par.mode = TEST_MODE;	/* Тестовый режим */
	    par.res = 1;		/* Энергопотребление */
	    par.chop = 1;
	    par.sps = SPS500;
	    par.pga = PGA2;
	    par.hpf = 0;

	     /* тест не прошел  */
	    if (ADS1282_config(&par) == false) {
		ADS1282_get_error_count(&err);
		for (i = 0; i < ADC_CHAN; i++) {
		    if (err.adc[i].cfg0_wr != err.adc[i].cfg0_rd || err.adc[i].cfg1_wr != err.adc[i].cfg1_rd)
			status->st_adc |= (1 << i);	// В каком канале ошибка
		}
		break;
	    }

	    ADS1282_start();	/* Запускаем АЦП с PGA  */
	    ADS1282_start_irq();	/* Разрешаем IRQ  */
	    delay_ms(250);	/* задержка чтобы подсчитать IRQ*/
	    ADS1282_stop();	/* Стоп АЦП */

	    /* Считаем прерывания  */
	    i = ADS1282_get_irq_count();


/* пока не выходить на 8 МГц кварце - разобраться!!!  */
#if QUARTZ_CLK_FREQ==(19200000)
	    if (i < 50) {
		break;
	    }
#endif
	    res = true;
	    status->st_main &= ~0x10;	// регистратор исправен 
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}



/**
 * Тест EEPROM, возвращает результат и устанавливает биты в статусе
 */
#pragma section("FLASH_code")
bool eeprom_test(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);


    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 |= 0x08;	/* Ошибка данных в eeprom */

	status->eeprom = read_all_data_from_eeprom();	/* Обновили все данные с eeprom  */

	if (status->eeprom == 0) {
	    status->st_test1 &= ~0x08;	/* Нет ошибки */
	    res = true;
	}
    }
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Тест часов RTC.
 * Пишем / читаем время из RTC, если ошибка - не используем 
 * Проверяем разницу - это означает, что часы перевелись  
 * Время компиляции должно быть ДО текущего времени
 */
#pragma section("FLASH_code")
bool rtc_test(void *par)
{
    DEV_STATUS_STRUCT *status;
    int t0, t1, tc;
    int diff;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	diff = 10 * 3;		/* на 30 секунд вперед, поотм назад */
	tc = get_comp_time();	/* Время компиляции */

	/* Ждем 1.5 секунды */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	t0 = get_rtc_sec_ticks();	/* Получаем секунды */

	/* если ошибка часов */
	if (t0 < 0)
	    break;

	set_rtc_sec_ticks(t0 + diff);	/* Пишем время вперед */


	t1 = get_rtc_sec_ticks();	/* Получаем секунды снова */

	if (par != NULL) {
	    status = (DEV_STATUS_STRUCT *) par;
	    status->st_test0 |= 0x01;	/* Неисправность часов  */
	    status->temper1 = 0;	/* Потом прочитаем данные */
	    status->press = 0;

	    /* пока не выясниться с часовыми поясами */
	    if (abs(t1 - t0) < (diff + 5) && tc - 3600 * 24 < t0) {
		status->st_test0 &= ~0x01;	/* Неисправность часов - снять */
		status->st_main &= ~0x01;	/* Убираем "нет времени"  */
		res = true;
	    }
	}
    } while (0);

    if (t0 > 0)
	set_rtc_sec_ticks(t0);	/* Пишем время */
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Test Датчик температуры и давления:  Прочитаем коэффициенты из bmp085 
 */
#pragma section("FLASH_code")
bool test_bmp085(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    int p, t;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x02;	/* Ошибка датчика температуры и давления */

	if (bmp085_init() && bmp085_data_get(&t, &p)) {
	    status->st_test0 &= ~0x02;	/* Снимаем ошибку датчика */
	    res = true;
	}
    }
    LED_set_state(l1, l2, l3, l4);
    return res;
}

/**
 * Test Компаса
 */
#pragma section("FLASH_code")
bool test_cmps(void *par)
{
    DEV_STATUS_STRUCT *status;
    lsm303_data data;
    bool res = false;
    u8 l1, l2, l3, l4;
    int t0;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x08;


	/* Ждем 2.5 секунды */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 2500) {
	    LED_test();
	}


	if (lsm303_get_comp_data(&data) == true) {

	    /* Все данные компаса сразу не могут быть == 0 */
	    if ((data.x == 0 && data.y == 0 && data.z == 0) || (data.x == -1 && data.y == -1 && data.z == -1))
		break;

	    status->st_test0 &= ~0x08;
	    res = true;
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Test Акселерометра - биты одинаковые с компасом!
 */
#pragma section("FLASH_code")
bool test_acc(void *par)
{
    DEV_STATUS_STRUCT *status;
    lsm303_data data;
    bool res = false;
    u8 l1, l2, l3, l4;
    int t0;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par != NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x04;

	/* Ждем 2.5 секунды */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 2500) {
	    LED_test();
	}

	/* Все данные акселерометра сразу и температура не могут быть == 0 */
	if (lsm303_get_acc_data(&data) == true) {

	    if ( /*(data.x == 0 && data.y == 0 && data.z == 0) || */ (data.x == -1 && data.y == -1 && data.z == -1))
		break;

	    status->st_test0 &= ~0x04;
	    res = true;
	}
    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Тест GPS. В течении 5-ти секунд патаемся получить NMEA
 */
#pragma section("FLASH_code")
bool test_gps(void *par)
{
    DEV_STATUS_STRUCT *status;
    bool res = false;
    s64 t0;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x10;	// Ошибка

	/* Одидаем запуска */
	t0 = get_msec_ticks();
	do {
	    LED_test();
	} while (get_msec_ticks() - t0 < 500);
	gps_set_grmc();

	/* Ждем пока буфер не начнет набираться */
	t0 = get_msec_ticks();
	do {
	    res = gps_nmea_exist();	/* выйдет при приеме 1 GSA */
	    if (res) {
		status->st_test0 &= ~0x10;
		break;
	    }
	    LED_test();
	} while (get_msec_ticks() - t0 < 4500);

    } while (0);
    LED_set_state(l1, l2, l3, l4);
    return res;
}

/**
 * Определить причину сброса
 */
#pragma section("FLASH_code")
void test_reset(void *par)
{
    DEV_STATUS_STRUCT *status;
    int res;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;

	// Получили причину сброса
	res = read_reset_cause_from_eeprom();
	if (res == CAUSE_POWER_OFF) {
	    status->st_reset = 1;
	} else if (res == CAUSE_EXT_RESET) {
	    status->st_reset = 2;
	} else if (res == CAUSE_BROWN_OUT) {
	    status->st_reset = 4;
	} else if (res == CAUSE_WDT_RESET) {
	    status->st_reset = 8;
	} else if (res == CAUSE_NO_LINK) {
	    status->st_reset = 16;
	} else {
	    status->st_reset = 32;	// непредвиденный сброс
	}
    }
    LED_set_state(l1, l2, l3, l4);
}


/**
 * Тестировать DAC 4 и генератор
 * Должно запускаться в самом начале работы. Не позже!
 */
#pragma section("FLASH_code")
bool test_dac4(void *par)
{
    DEV_STATUS_STRUCT *status;
    int i;
    int min = 0, max = 0;
    u64 t0, t1;
    bool res = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);

#if QUARTZ_CLK_FREQ==(19200000)
    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;

	status->st_test1 |= 0x20;	// Ошибка генератора 4 МГц
	status->st_test1 |= 0x80;	// Ошибка таймера T3


	DAC_write(DAC_4MHZ, 0);	// Подаем на dac4 0

	TIMER3_init();		// Запустим 3-й таймер. Вектор на IRQ не ставим!
	IRQ_register_vector(TIM1_TIM3_VECTOR_NUM);

	/* Ждем пока буфер не начнет набираться */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	/* Ждем 2.5 секунды */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 4500) {
	    t1 = get_msec_ticks();
	    LED_test();
	    if (TIMER3_get_counter() > 2000000 && TIMER3_get_counter() < 2040000) {
		res = true;
		break;
	    }
	}
	if (res == false)
	    break;

	min = TIMER3_get_period();

	DAC_write(DAC_4MHZ, 0x3ff0);	// Подаем на dac4 максимум
	res = false;

	/* Ждем пока буфер не начнет набираться */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}

	/* Ждем 1.5 секунды */
	t0 = get_msec_ticks();
	t1 = t0;
	while (t1 - t0 < 4500) {
	    t1 = get_msec_ticks();
	    LED_test();
	    if (TIMER3_get_counter() > 2000000 && TIMER3_get_counter() < 2040000) {
		res = true;
		break;
	    }
	}
	if (res == false)
	    break;

	max = TIMER3_get_period();
	status->st_test1 &= ~0x80;	// Снимаем ошибку таймера 3

	if (min > 0 && max > 0) {
	    status->st_test1 &= ~0x20;	// Снимаем ошибку генератора 4.096 МГц
	    res = true;
	}

    } while (0);
    status->st_tim3[0] = min;
    status->st_tim3[1] = max;

    IRQ_unregister_vector(TIM1_TIM3_VECTOR_NUM);
    TIMER3_disable();

#elif QUARTZ_CLK_FREQ==(8192000)
    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 &= ~0x20;	// Ошибка генератора 4 МГц - сняли
	status->st_test1 &= ~0x80;	// Ошибка таймера T3 - сняли
	res = true;
    } while (0);

    status->st_tim3[0] = SCLK_VALUE - 100;
    status->st_tim3[1] = SCLK_VALUE + 100;
#endif
    LED_set_state(l1, l2, l3, l4);
    return res;
}


/**
 * Тестировать DAC 19 и генератор
 * Должно запускаться в самом начале работы. Не позже!
 */
#pragma section("FLASH_code")
bool test_dac19(void *par)
{
    u64 t0, t1;
    DEV_STATUS_STRUCT *status;
    int min = 0, max = 0;
    bool res = false;
    bool ch1 = false, ch0 = false;
    u8 l1, l2, l3, l4;

    LED_get_state(&l1, &l2, &l3, &l4);


    do {
	if (par == NULL)
	    break;

	status = (DEV_STATUS_STRUCT *) par;
	status->st_test1 |= 0x10;	// Ошибка генератора 19 МГц
	status->st_test1 |= 0x40;	// Ошибка таймера T4

	DAC_write(DAC_19MHZ, DAC19_MIN_DATA);	// Подаем на dac 0

	TIMER4_config();	// Заводим счетный таймер
	TIMER4_enable_irq();
	TIMER4_del_vector();	// Удалим вектор - иначе не сможем опросить флажок


	status->st_test1 |= 0x01;	// GPS включен

	/* Ждем пока буфер не начнет набираться */
	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 1500) {
	    LED_test();
	}


	t0 = get_msec_ticks();
	while (get_msec_ticks() - t0 < 4500) {
	    ch0 = TIMER4_wait_for_irq();
	    if (get_msec_ticks() - t0 > 2500 && ch0 == true) {
		delay_ms(20);
		min = TIMER4_get_period();
		break;
	    }
	    LED_test();
	}


	status->st_test1 &= ~0x40;	// Снимаем ошибку таймера T4 - нет PPS


	t0 = get_msec_ticks();
	DAC_write(DAC_19MHZ, DAC19_MAX_DATA);	// Подаем на dac max
	do {
	    t1 = get_msec_ticks();
	    ch1 = TIMER4_wait_for_irq();
	    if (t1 - t0 > 2500 && ch1 == true) {
		delay_ms(20);
		max = TIMER4_get_period();
		break;
	    }
	    LED_test();
	} while (t1 - t0 < 4500);

	/* Смотрим чтоб был максимум */
	if (min == 0 || ch0 == false || min >= SCLK_VALUE || ch1 == false || max <= SCLK_VALUE) {
	    res = false;
	    break;
	}


	/* Снимаем ошибку генератора 19.2 МГц */
	if (abs(max - min) > 20) {
	    status->st_test1 &= ~0x10;
	    res = true;
	}

    }
    while (0);

    // Что прочитали
    status->st_tim4[0] = min;
    status->st_tim4[1] = max;

    TIMER4_disable();		/* Закрываем счетный таймер */
    TIMER4_disable_irq();
    LED_set_state(l1, l2, l3, l4);
    return res;
}



/**
 * Тестировать все модули
 */
#pragma section("FLASH_code")
void test_all(void *par)
{
    DEV_STATUS_STRUCT *status;
    int t0, i;

    /* не могу открыть UART0 */
    if (par != NULL && gps_init() == 0) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_main |= 0x80;	/* Идет тестирование (с даками) */

	status->st_test1 |= 0x01;	/* GPS включен */

	adc_init(status);	/* Инициализация атмеловских датчиков и вн. по I2C */

	test_gps(status);	/* Test GPS модуля */
	DAC_init();		/* Все даки */

	eeprom_test(status);	/* Тест eeprom  */
	test_reset(status);	/* Получили причину сброса */

	/* Test RTC: Прочитаем время из RTC, если ошибка - не используем */
	for (i = 0; i < 3; i++) {
	    if (rtc_test(status) == true) {
		t0 = get_rtc_sec_ticks();	/* Если успех - поменяем время  */
		break;
	    } else {
		t0 = get_comp_time();	/* Время компиляции */
		delay_ms(50);
	    }
	}

	/* если test-dac4 здесь ... - ошибка? */
	test_bmp085(status);	/* Test Датчик температуры и давления:  Прочитаем коэффициенты из bmp085 */
	test_acc(status);	/* Test Акселерометра: */
	test_cmps(status);	/* Test Компаса: */
	test_adc(status);	/* Тест АЦП */

	for (i = 0; i < 3; i++) {
	    if (test_dac19(status))	/* Тест таймера4 и генератора 19.2 МГц */
		break;
	}
	test_dac4(status);	/* Тест таймера3 и генератора 4.096 МГц */


	t0 = get_rtc_sec_ticks();	/* Если успех - поменяем время  */
	set_sec_ticks(t0);	/* Установим часы */
    }
    status->st_main &= ~0x80;	/* Снимаем статус "тестирование" */
    status->st_test1 &= ~0x01;	/* GPS вЫключен */
    gps_close();		/* Закрываем UART0  */
}
