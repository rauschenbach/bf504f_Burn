#include <string.h>
#include <stdio.h>
#include "comport.h"
#include "com_cmd.h"
#include "crc16.h"
#include "main.h"
#include "am3.h"
#include "log.h"
#include "led.h"
#include "uart1.h"
#include "utils.h"
#include "ports.h"

/* Проверить с JTAG почему это происходит!!! */
#define		UART_AM3_MODEM_SPEED		57600	/* Скорость порта для модема  */
#define 	WAIT_MODEM_TIME_MS		250	/* 200 мс - максимальное время ожыдания ответа по протоколу */

/************************************************************************
 * 	Статические переменные
 ************************************************************************/
/* Обмен с модемам - указатель */
static struct MODEM_XCHG_STRUCT{
    u8 rx_buf[MODEM_BUF_LEN];
    u8 rx_beg;			/* Начало пакета */
    u8 rx_cnt;			/* Счетчик принятого */
    u8 rx_fin;			/* Конец приема */
    u8 tx_len;			/* Сколько передавать  */
} *pModem_xchg_buf;

/************************************************************************
 * 	Статические функции
 ************************************************************************/
static void am3_modem_read_ISR(u8);	/* Чтение из модема */
static int am3_write_data(char *, int);
static int am3_get_data(char *, int);
static void am3_wait_reply(int);


/**
 * Инициализация с проверкой
 */
#pragma section("FLASH_code")
int am3_init(void)
{
    DEV_UART_STRUCT com_par;
    int res = -1;

    do {
	/* Для обслуживания обмена создаем буфер на прием */
	if (pModem_xchg_buf == NULL) {
	    pModem_xchg_buf = calloc(1, sizeof(struct MODEM_XCHG_STRUCT));
	    if (pModem_xchg_buf == NULL) {
		log_write_log_file("ERROR: can't alloc buf for am3\n");
		break;
	    }
	} else {
	    log_write_log_file("WARN: am3 buf dev already exists\n");
	}

	log_write_log_file("INFO: alloc buf for am3 OK\n");

	/* Вызываем UART1 init */
	com_par.baud = UART_AM3_MODEM_SPEED;
	com_par.rx_call_back_func = am3_modem_read_ISR;
	com_par.tx_call_back_func = NULL;	/* Нет  */

	if (UART1_init(&com_par) == false)
	    break;

	/* Переключаем селектор на аналоговый модем  */
	select_modem_module();
	res = 0;
    } while (0);

    return res;
}


/* Закрыть UART  */
#pragma section("FLASH_code")
void am3_close(void)
{
    UART1_close();


    if (pModem_xchg_buf) {
	free(pModem_xchg_buf);	/* Освобождаем буфер  */
	pModem_xchg_buf = NULL;
    }
}


/**
 * Обслуживание модема - прием ответа 
 * Ответ должен начинаться с $ASA
 * Можно из FLASH!!!
 */
section("L1_code")
static void am3_modem_read_ISR(u8 rx_byte)
{
    /*  Первый байт */
    if (0x24 == rx_byte) {	/* Начинается всегда с $ */
	pModem_xchg_buf->rx_buf[0] = 0x24;
	pModem_xchg_buf->rx_beg = 1;
	pModem_xchg_buf->rx_cnt = 1;
	pModem_xchg_buf->rx_fin = 0;
    } else if (pModem_xchg_buf->rx_beg == 1) {	// Если уже есть начало
	pModem_xchg_buf->rx_buf[pModem_xchg_buf->rx_cnt] = rx_byte;

	/* Если перевод строки - считаем что набрали буфер */
	if (rx_byte == 0x0A && pModem_xchg_buf->rx_buf[pModem_xchg_buf->rx_cnt - 1] == 0x0d) {
	    pModem_xchg_buf->rx_buf[pModem_xchg_buf->rx_cnt + 1] = 0;
	    pModem_xchg_buf->rx_beg = 0;
	    pModem_xchg_buf->rx_fin = 1;	/* FIN стоит до тех пока не начали принимать с $ */
	} else {
	    pModem_xchg_buf->rx_cnt++;
	    pModem_xchg_buf->rx_cnt %= MODEM_BUF_LEN;
	}
    }
}

/**
 * Перетранслировать команду для модема, переключив порт
 */
#pragma section("FLASH_code")
int am3_convey_buf(void *cmd, int size)
{
    int res = -1, t0, len;
    char str[MODEM_BUF_LEN];
    DEV_STATUS_STRUCT status;
    DEV_UART_CMD uart_cmd;	/* Команда пришла с UART */

    comport_close();		/* закрываем отладочный порт */

    cmd_get_dsp_status(&status);	/* получили статус */
    status.st_test0 |= 0x10;	/* ставим "GNS неисправен" */
    status.st_main |= 0x02;	/* ставим "Ошибка модема"  */

    do {

	/* Установка порта на акустический модем */
	if (am3_init() < 0) {
	    break;
	}

	status.st_test0 &= ~0x10;	/* уберем "GNS неисправен" */
	memset(&uart_cmd, 0, sizeof(uart_cmd));

	am3_write_data((char *) cmd, size);	/* Посылаем команду из буфера ДЛЯ модема  */
	am3_wait_reply(WAIT_MODEM_TIME_MS);	/* Ждем ответ  */

	len = am3_get_data(str, MODEM_BUF_LEN);
	len %= MODEM_BUF_LEN;
	if (len > 0) {
	    res = 0;		/* успех - если в этом мусоре появились данные! */
	    status.st_main &= ~0x08;	/* снимаем "не вовремя" */
	    status.st_main &= ~0x02;	/* снимаем "Ошибка модема" */
	    memcpy(uart_cmd.u.cPar, str, len);	/* Что приняли */
	    uart_cmd.len = len;
	}

	set_uart_cmd_buf(&uart_cmd);	/* Передали буфер */
	am3_close();		/* Закроем модемный порт */

    } while (0);

    cmd_set_dsp_status(&status);	/* восстановили статус */

    /* Возвращаем назад порт отладки */
    if (comport_init() < 0) {
	res = -1;
    }

    return res;
}


/**
 * Пограммирование модема с проверкой
 */
#pragma section("FLASH_code")
int am3_prg_modem(void *par)
{
    int t0, t1, i, k;
    char str[MODEM_BUF_LEN];
    u8 h0, m0, h1, m1;
    int res = -1;		/* Сразу поставим ошибка. Если все выполнится ОК-ставим 0 */
    TIME_DATE td;
    GNS110_PARAM_STRUCT *time;


    if (par == NULL)
	return -1;

    time = (GNS110_PARAM_STRUCT *) par;	/* Переданное время */

    log_write_log_file(">>>>>>>>>>>>>>>>>>>> Init modem AM3 <<<<<<<<<<<<<<<<<<<\n");

    modem_on();

    /* Ждем ~8 секунд после включения */
    t0 = get_sec_ticks();
    while (get_sec_ticks() - t0 < MODEM_POWERON_TIME_SEC)
	LED_blink();


    do {
	/* Установка акустического модема - только подстройка времени */
	if (am3_init() < 0) {
	    break;
	}

	i = 1;
	log_write_log_file("Acoustic modem AM3 init OK, number got from log: %d\n", time->gns110_modem_num);
	if (time->gns110_modem_num > 0) {	/* Если '0' - номер модема не изменяем - ставим то что у него уже записано */
	    log_write_log_file("%d)------------------------------------------------\n", i++);
	    delay_ms(WAIT_MODEM_TIME_MS);	/* Задержка перед посылкой команды */
	    LED_blink();

	    if (am3_set_dev_num(time->gns110_modem_num) == 0) {
		log_write_log_file("SUCCESS: Set modem number %d OK\n", time->gns110_modem_num);
	    } else {
		log_write_log_file("FAIL: Set modem number %d\n", time->gns110_modem_num);
		break;
	    }
	    delay_ms(WAIT_MODEM_TIME_MS);	// Задержка перед посылкой команды
	    LED_blink();
	} else {
  	     log_write_log_file("We don't have to change Acoustic modem number\n");
	    // Читаем из модема
	    t0 = am3_get_dev_num();
	    if (t0 == 99 || t0 == 0) {
		log_write_log_file("FAIL: Modem number can't be %d\n", t0);
		break;
	    }

	    time->gns110_modem_num = t0;
	    if (am3_set_dev_num(time->gns110_modem_num) == 0) {
		log_write_log_file("SUCCESS: Set modem number %d OK\n", time->gns110_modem_num);
	    } else {
		log_write_log_file("FAIL: Set modem number %d\n", time->gns110_modem_num);
		break;
	    }
	    delay_ms(WAIT_MODEM_TIME_MS);	// Задержка перед посылкой команды
	    LED_blink();
	}

	/* Установим время модема */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = get_sec_ticks();
	sec_to_td(t0, &td);
	if (am3_set_curr_time(&td) == 0) {
	    log_write_log_file("SUCCESS: Set modem time\n");
	} else {
	    log_write_log_file("FAIL: Set modem time\n");
	    break;
	}


	/* Установим аварийное время всплытия */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	sec_to_td(time->gns110_modem_alarm_time, &td);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	res = am3_set_alarm_time(&td);
	if (res == 0) {
	    log_write_log_file("SUCCESS: Set alarm popup time: %02d.%02d.%02d - %02d:%02d\n", td.day, td.mon, td.year - 2000, td.hour, td.min);
	} else {
	    log_write_log_file("FAIL: Set alarm popup time\n");
	    break;
	}
	res = -1;		/* Поставим снова */

	/* Установим светлое время суток */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_cal_time(time->gns110_modem_h0_time, time->gns110_modem_m0_time, time->gns110_modem_h1_time, time->gns110_modem_m1_time) == 0) {
	    log_write_log_file("SUCCESS: Set calendar time\n");
	} else {
	    log_write_log_file("FAIL: Set calendar time\n");
	    break;
	}

	/* Длительность всплытия: получаем в секундах, передаем в минутах */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	t0 = time->gns110_gps_time - time->gns110_burn_off_time;
	t0 /= 60;
	delay_ms(WAIT_MODEM_TIME_MS);	// Задержка перед посылкой команды
	LED_blink();
	if (am3_set_popup_len(t0) == 0) {
	    log_write_log_file("SUCCESS: Set popup len: %d min\n", t0);
	} else {
	    log_write_log_file("FAIL: Set popup len: %d min\n", t0);
	    break;
	}


	/* Длительность пережига в секундах */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);	// Задержка перед посылкой команды
	LED_blink();
	if (am3_set_burn_len(time->gns110_modem_burn_len_sec) == 0) {
	    log_write_log_file("SUCCESS: Set burn len: %d sec\n", time->gns110_modem_burn_len_sec);
	} else {
	    log_write_log_file("FAIL: Set burn len: %d sec\n", time->gns110_modem_burn_len_sec);
	    break;
	}


	/* Применить все параметры */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_all_params() == 0) {
	    log_write_log_file("SUCCESS: Set all parameters\n");
	} else {
	    log_write_log_file("FAIL: Set all parameters\n");
	    break;
	}


	/***********************************************************************************************
	 * Проверка того, что мы записали
	 ***********************************************************************************************/

	/* Получить номер устройства - если номер 0 - ошибка! */
	for (k = 0; k < 3; k++) {
	    log_write_log_file("%d)------------------------------------------------\n", i++);
	    t0 = am3_get_dev_num();
	    if (t0 != time->gns110_modem_num && time->gns110_modem_num != 0) {
		log_write_log_file("ERROR: %d read modem number: %d\n", k + 1, t0);
	    } else {
		log_write_log_file("SUCCESS: Get modem number: %d\n", t0);
		break;
	    }
	    delay_ms(WAIT_MODEM_TIME_MS);
	    LED_blink();
	}

	if (t0 == 0 || t0 == 99) {
	    log_write_log_file("FAIL: Modem number can't be: %d\n", t0);
	    break;
	}

	/* Еще раз проверим! */
	if (k >= 2 && t0 != time->gns110_modem_num && time->gns110_modem_num != 0) {
	    log_write_log_file("FAIL: Get modem number: %d\n", t0);
	    break;
	}



	/* Получить время с модема  */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = get_sec_ticks();	/* Наше время  */
	t1 = am3_get_curr_time(&td);
	if (t1 != -1 && abs(t1 - t0) < 120) {
	    log_write_log_file("SUCCESS: Get modem time: %02d.%02d.%02d %02d:%02d:%02d\n", td.day, td.mon, td.year - 2000, td.hour, td.min, td.sec);
	} else {
	    log_write_log_file("ERROR: Get modem time: %02d.%02d.%02d %02d:%02d:%02d\n", td.day, td.mon, td.year - 2000, td.hour, td.min, td.sec);
	    break;
	}

	/* Получить аварийное время всплытия - проверить что мы записали туда менее 2 минут разницы д.б.!!! */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = am3_get_alarm_time(&td);
	if (t0 == -1 || abs(t0 - time->gns110_modem_alarm_time) > 120) {
	    log_write_log_file("ERROR: Get modem alarm time: %02d.%02d.%02d - %02d:%02d:%02d\n", td.day, td.mon, td.year - 2000, td.hour, td.min, td.sec);
	    break;
	}
	log_write_log_file("SUCCESS: Get modem alarm time: %02d.%02d.%02d - %02d:%02d:%02d\n", td.day, td.mon, td.year - 2000, td.hour, td.min, td.sec);
	/* Получить светлое время суток */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	if (am3_get_cal_time(&h0, &m0, &h1, &m1) == 0) {
	    if (h0 == time->gns110_modem_h0_time && m0 == time->gns110_modem_m0_time && h1 == time->gns110_modem_h1_time && m1 == time->gns110_modem_m1_time) {
		log_write_log_file("SUCCESS: Get calendar time\n");
	    } else {
		log_write_log_file("FAIL: Get calendar time: %02d:%02d-%02d:%02d\n", h0, m0, h1, m1);
		break;
	    }
	} else {
	    log_write_log_file("FAIL: Get calendar time\n");
	}


	/* Длительность всплытия в минутах */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = am3_get_popup_len();
	t0 *= 60;		// в секунды перевели
	if (t0 == time->gns110_gps_time - time->gns110_burn_off_time) {
	    log_write_log_file("SUCCESS: Get popup len\n");
	} else {
	    log_write_log_file("FAIL: Get popup len\n");
	    break;
	}


	/* Длительность пережыга в секундах */
	log_write_log_file("%d)------------------------------------------------\n", i++);
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = am3_get_burn_len();
	if (t0 == time->gns110_modem_burn_len_sec) {
	    log_write_log_file("SUCCESS: Get burn len\n");
	} else {
	    log_write_log_file("FAIL: Get burn len\n");
	    break;
	}

	res = 0;		// Все OK
	log_write_log_file("---------------------------------------------------\n");
    } while (0);
    am3_close();
    return res;
}

/**
 * Опрос модема. Перевести время всплытия на сутки вперед от наших часов, пока он не запрограммирован.
 */
#pragma section("FLASH_code")
int am3_check_modem(void *v)
{
    TIME_DATE td;
    int res = -1, t0, t1, temp;
    GNS110_PARAM_STRUCT *run;
    if (v == NULL)
	return res;
    do {

	run = (GNS110_PARAM_STRUCT *) v;

	/* Установка акустического модема - только подстройка времени */
	if (am3_init() < 0) {
	    break;
	}

	/* Получить номер устройства */
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	temp = am3_get_dev_num();
	if (temp < 0) {
	    break;
	}
	run->gns110_modem_num = temp;

	/* Получить время с модема  */
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	run->gns110_modem_rtc_time = am3_get_curr_time(&td);
	if (run->gns110_modem_rtc_time < 0) {
	    run->gns110_modem_rtc_time = 1577836800;	// 01-01-20
	}
	//-------------------- Установим номер модема
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_dev_num(run->gns110_modem_num) < 0) {
	    break;
	}
	//-------------------- Установим время модема по часам GNS110
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 = get_sec_ticks();
	sec_to_td(t0, &td);
	if (am3_set_curr_time(&td) != 0) {
	    break;
	}
	//----------------------- Время аварийного всплытия на сутки вперед
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	t0 += 86400;
	sec_to_td(t0, &td);
#if ZAPLATKA
	td.hour = 23;
	td.min = 59;
#endif
	if (am3_set_alarm_time(&td) != 0) {
	    break;
	}
	//----------------------- Светлое время
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_cal_time(6, 0, 20, 0) < 0) {
	    break;
	}
	//-----------------------Время подъема-всплытия
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_popup_len(1) < 0) {
	    break;
	}
	//-----------------------Время пережига
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_burn_len(5) < 0) {
	    break;
	}
	// Установить все параметры
	delay_ms(WAIT_MODEM_TIME_MS);
	LED_blink();
	if (am3_set_all_params() != 0) {
	    break;
	}
	res = 0;
    } while (0);
    comport_init();		/* заново инициализируем отладочный порт */
    return res;
}


/**
 * Включить радиопередачу
 */
#pragma section("FLASH_code")
int am3_set_radio(void *v)
{
    int t0;
    GNS110_PARAM_STRUCT *par;
    if (v == NULL)
	return -1;
    par = (GNS110_PARAM_STRUCT *) v;	/* Переданное число */
    modem_on();			/* Включаем реле Акуст модема  */

    /* Установка акустического модема - только подстройка времени */
    if (am3_init() < 0) {
	return -1;
    }

    /* Включаем радиопередачу */
    if (am3_set_gps_radio() == 0) {
	log_write_log_file("SUCCESS: Set modem GPS radio \n");
    } else {
	// еще раз
	if (am3_set_gps_radio() < 0) {
	    log_write_log_file("FAIL: Set modem GPS radio \n");
	    am3_close();
	    return -1;
	}
    }

    /* Не выключать UART */
    return 0;
}

/**
 *  Установить номер устройства 
 */
#pragma section("FLASH_code")
int am3_set_dev_num(short num)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    if (num > 9999)
	num = 9999;
    snprintf(str, sizeof(str), "$RSANM,%04d,*%04x\r\n", num, 0);	// 1 шаг - Мы должны послать эту команду (без 2-х нулей)
    crc16 = check_crc16((u8 *) str + 1, 11);	// шаг 2 - Определяем контрольную сумму до звездочки 
    snprintf(str, sizeof(str), "$RSANM,%04d,*%04x\r\n", num, crc16);	// 3 шаг - Команда с контрольной суммой 
    am3_write_data(str, strlen(str));	/* Посылаем команду */
    log_write_log_file("TX: am3_set_dev_num: %s", str);
    /* Ждем ответ */
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN) > 0) {
	log_write_log_file("RX: am3_set_dev_num: %s", str);
	if (strstr(str, "$ASANM,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_dev_num: got nothing\n");
    }

    return res;
}


/* Установить время аналогового модема */
#pragma section("FLASH_code")
int am3_set_curr_time(TIME_DATE * td)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSACL,%02d.%02d.%02d,%02d:%02d,*%04x\r\n", td->day, td->mon, td->year - 2000, td->hour, td->min, 0x0000);
    crc16 = check_crc16((u8 *) str + 1, 21);	/* Определяем контрольную сумму до звездочки */
    snprintf(str, sizeof(str), "$RSACL,%02d.%02d.%02d,%02d:%02d,*%04x\r\n", td->day, td->mon, td->year - 2000, td->hour, td->min, crc16);	/* Команды с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_set_curr_time: %s", str);
    /* Ждем ответ */
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_curr_time: %s", str);
	if (strstr(str, "$ASACL,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_curr_time: got nothing\n");
    }

    return res;
}


/* Установить аварийное время всплытия */
#pragma section("FLASH_code")
int am3_set_alarm_time(TIME_DATE * td)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSAAL,%02d.%02d.%02d,%02d:%02d,*%04x\r\n", td->day, td->mon, td->year - 2000, td->hour, td->min, 0x0000);	// 1 шаг
    /* Определяем контрольную сумму до звездочки */
    crc16 = check_crc16((u8 *) str + 1, 21);	// шаг 2
    /* Команды с контрольной суммой */
    snprintf(str, sizeof(str), "$RSAAL,%02d.%02d.%02d,%02d:%02d,*%04x\r\n", td->day, td->mon, td->year - 2000, td->hour, td->min, crc16);	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_set_alarm_time: %s", str);
    /* Ждем ответ */
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_alarm_time: %s", str);
	if (strstr(str, "$ASAAL,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_alarm_time: got nothing\n");
    }

    return res;
}

/**
 * Установить локальный календарь. Светлое время суток 
 */
#pragma section("FLASH_code")
int am3_set_cal_time(u8 h0, u8 m0, u8 h1, u8 m1)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSAST,%02d:%02d-%02d:%02d,*%04x\r\n", h0, m0, h1, m1, 0x0000);	// 1 шаг
    /* Определяем контрольную сумму до звездочки */
    crc16 = check_crc16((u8 *) str + 1, 18);	// шаг 2
    /* Команды с контрольной суммой */
    snprintf(str, sizeof(str), "$RSAST,%02d:%02d-%02d:%02d,*%04x\r\n", h0, m0, h1, m1, crc16);	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_set_cal_time: %s\n", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_cal_time: %s", str);
	if (strstr(str, "$ASAST,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_cal_time: got nothing\n");
    }

    return res;
}


/* Установить продолжительность всплытия */
#pragma section("FLASH_code")
int am3_set_popup_len(u16 min)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    if (min > 999)
	min = 999;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSATZ,%03d,*%04x\r\n", min, 0x0000);	// 1 шаг
    /* Определяем контрольную сумму до звездочки */
    crc16 = check_crc16((u8 *) str + 1, 10);	// шаг 2
    /* Команды с контрольной суммой */
    snprintf(str, sizeof(str), "$RSATZ,%03d,*%04x\r\n", min, crc16);	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_set_popup_len: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_popup_len: %s", str);
	if (strstr(str, "$ASATZ,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_popup_len: got nothing\n");
    }


    return res;
}

/* Установить продолжительность пережига проволоки в секундах */
#pragma section("FLASH_code")
int am3_set_burn_len(u16 sec)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    if (sec < 1)
	sec = 1;
    if (sec > 9999)
	sec = 9999;
    snprintf(str, sizeof(str), "$RSABT,%04d,*%04x\r\n", sec, 0x0000);	/* Мы должны послать эту команду (без 2-х нулей) */
    crc16 = check_crc16((u8 *) str + 1, 11);	/* Определяем контрольную сумму до звездочки */
    snprintf(str, sizeof(str), "$RSABT,%04d,*%04x\r\n", sec, crc16);	/* Команды с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_set_burn_len: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_burn_len: %s", str);
	if (strstr(str, "$ASABT,,00") != NULL) {
	    res = 0;
	}
    } else {
	log_write_log_file("RX: am3_set_burn_len: got nothing\n");
    }

    return res;
}

/* Установить размер файла данных */
#pragma section("FLASH_code")
int am3_set_file_len(u32 num)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSASZ,%08d,*%04x\r\n", num, 0x0000);	// 1 шаг
    /* Определяем контрольную сумму до звездочки */
    crc16 = check_crc16((u8 *) str + 1, 15);	// шаг 2
    /* Команды с контрольной суммой */
    snprintf(str, sizeof(str), "$RSASZ,%03d,*%04x\r\n", num, crc16);	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_set_file_len: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_file_len: %s", str);
	if (strstr(str, "$ASASZ,,00") != NULL)
	    res = 0;
    } else {
	log_write_log_file("RX: am3_set_file_len: got nothing\n");
    }

    return res;
}


/* Включение GPS и радиопередачи */
#pragma section("FLASH_code")
int am3_set_gps_radio(void)
{
    char str[MODEM_BUF_LEN];
    int res = -1;
    u16 crc16;
    int t0;
    /* Мы должны послать эту команду (без 2-х нулей) */
    snprintf(str, sizeof(str), "$RSAGR,,*%04x\r\n", 0x0000);	// 1 шаг
    /* Определяем контрольную сумму до звездочки */
    crc16 = check_crc16((u8 *) str + 1, 7);	// шаг 2
    /* Команды с контрольной суммой */
    snprintf(str, sizeof(str), "$RSAGR,,*%04x\r\n", crc16);	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_set_gps_radio: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_gps_radio: %s", str);
	if (strstr(str, "$ASAGR,,00") != NULL)
	    res = 0;
    } else {
	log_write_log_file("RX: am3_set_gps_radio: got nothing\n");
    }

    return res;
}

/* Установить все введенные параметры */
#pragma section("FLASH_code")
int am3_set_all_params(void)
{
    char str[MODEM_BUF_LEN];
    int res = -1, pos;
    u16 crc16;
    int t0;
    snprintf(str, sizeof(str), "$RSAPR,,*\r\n");	/* 1 шаг. Мы должны послать команду RGAST */
    crc16 = check_crc16((u8 *) str + 1, 7);	/* шаг 2. Определяем контрольную сумму до звездочки */
    snprintf(str, sizeof(str), "$RSAPR,,*%04x\r\n", crc16);	/* 3 шаг     Команды с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_set_all_params: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_set_all_params: %s", str);
	if (strstr(str, "$ASAPR,,00") != NULL)
	    res = 0;
    } else {
	log_write_log_file("RX: am3_set_all_params: got nothing\n");
    }

    return res;
}

/*  Получить номер устройства */
#pragma section("FLASH_code")
s16 am3_get_dev_num(void)
{
    char str[MODEM_BUF_LEN];
    char buf[8];
    short res = -1;
    u16 crc16;
    int t0, pos = 0;
    /* Команда с контрольной суммой */
    snprintf(str, sizeof(str), "$RGANM,,*d05c\r\n");	// 3 шаг
    /* Посылаем команду  */
    am3_write_data(str, strlen(str));
    log_write_log_file("TX: am3_get_dev_num: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    /* Ждем ответ */
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_get_dev_num: %s", str);
	/* ищем 1-ю запятую: $AGANM,0021,00*6327 - не до конца буфера!!! */
	for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
	    if (str[pos] == 0x2c) {
		pos += 1;
		break;
	    }
	}

	/* копируем 4 символа после запятой */
	if (MODEM_BUF_LEN - 16) {
	    strncpy(buf, str + pos, 4);
	    buf[4] = 0;
	    res = atoi(buf);
	}
    } else {
	log_write_log_file("RX: am3_get_dev_num: got nothing\n");
    }

    return res;
}

/*  Получить часы реального времени */
#pragma section("FLASH_code")
int am3_get_curr_time(TIME_DATE * td)
{
    char str[MODEM_BUF_LEN];
    char buf[16];
    int res = -1;
    u16 crc16;
    int t0, pos = 0;
    int i;

    if (td == NULL)
	return res;
    snprintf(str, sizeof(str), "$RGACL,,*a13c\r\n");	/* 1 шаг. Мы должны послать команду RGACL */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_get_curr_time: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    /* Найдем подстроку $AGACL в этом мусоре $AGACL,21.05.13,11:07,00*7cdd - может быть не с начала */
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_get_curr_time: %s", str);
	// ищем 1-ю запятую: $AGACL,21.05.13,11:07,00*7cdd
	for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
	    if (str[pos] == 0x2c) {
		pos += 1;
		break;
	    }
	}


	do {
	    res = -1;
	    /* Разбираем буфер прям здесь! */
	    memmove(str, str + pos, 14);	// от запятой
	    // день [1,31]
	    memcpy(buf, str, 2);
	    buf[2] = 0;
	    td->day = atoi(buf);
	    if (td->day == 0 || td->day > 31)
		break;
	    // месец 
	    memcpy(buf, str + 3, 2);
	    buf[2] = 0;
	    td->mon = atoi(buf);
	    if (td->mon == 0 || td->mon > 12)
		break;
	    // Год
	    memcpy(buf, str + 6, 2);
	    buf[2] = 0;
	    td->year = atoi(buf) + 2000;
	    // Чисы      
	    memcpy(buf, str + 9, 2);
	    buf[2] = 0;
	    td->hour = atoi(buf);
	    if (td->hour > 24)
		break;
	    // минуты
	    memcpy(buf, str + 12, 2);
	    buf[2] = 0;
	    td->min = atoi(buf);
	    if (td->min > 60)
		break;
	    // Секунд нету
	    td->sec = 0;
	    // Для проверки пробуем преобразовать
	    res = td_to_sec(td);
	} while (0);
    } else {
	log_write_log_file("RX: am3_get_curr_time: got nothing\n");
    }
    return res;
}

/* Получить аварийное время всплытия */
#pragma section("FLASH_code")
int am3_get_alarm_time(TIME_DATE * td)
{
    char str[MODEM_BUF_LEN];
    char buf[16];
    int res = -1, pos;
    u16 crc16;
    int t0;
    if (td != NULL) {
	snprintf(str, sizeof(str), "$RGAAL,,*cf5c\r\n");	/* 1 шаг. Мы должны послать команду RGACL */
	am3_write_data(str, strlen(str));	/* Посылаем команду  */
	log_write_log_file("TX: am3_get_alarm_time: %s", str);
	am3_wait_reply(WAIT_MODEM_TIME_MS);
	/* Найдем подстроку $AGACL в этом мусоре  $AGAAL,02.07.13,10:20,00*583c - может быть не с начала */
	if (am3_get_data(str, MODEM_BUF_LEN)) {

	    log_write_log_file("RX: am3_get_alarm_time: %s", str);
	    for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
		if (str[pos] == 0x2c) {
		    pos += 1;
		    break;
		}
	    }

	    /* Разбираем буфер прям здесь! */
	    memmove(str, str + pos, 14);	// от запятой
	    // день [1,31]
	    memcpy(buf, str, 2);
	    buf[2] = 0;
	    td->day = atoi(buf);
	    // месец 
	    memcpy(buf, str + 3, 2);
	    buf[2] = 0;
	    td->mon = atoi(buf);
	    // Год
	    memcpy(buf, str + 6, 2);
	    buf[2] = 0;
	    td->year = atoi(buf) + 2000;
	    // Чясы      
	    memcpy(buf, str + 9, 2);
	    buf[2] = 0;
	    td->hour = atoi(buf);
	    // минуты
	    memcpy(buf, str + 12, 2);
	    buf[2] = 0;
	    td->min = atoi(buf);
	    // Секунд нету
	    td->sec = 0;
	    // Для проверки пробуем преобразовать
	    res = td_to_sec(td);
	} else {
	    log_write_log_file("RX: am3_get_alarm_time: got nothing\n");
	}
    }
    return res;
}

/* Получить светлое время суток */
#pragma section("FLASH_code")
int am3_get_cal_time(u8 * h0, u8 * m0, u8 * h1, u8 * m1)
{
    char str[MODEM_BUF_LEN];
    char buf[16];
    int res = -1, pos;
    u16 crc16;
    int t0;
    snprintf(str, sizeof(str), "$RGAST,,*\r\n");	/* 1 шаг. Мы должны послать команду RGAST */
    crc16 = check_crc16((u8 *) str + 1, 7);	/* шаг 2. Определяем контрольную сумму до звездочки */
    snprintf(str, sizeof(str), "$RGAST,,*%04x\r\n", crc16);	/* 3 шаг     Команды с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_get_cal_time: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    /* Найдем подстроку $AGACL в этом мусоре  $AGAST,06:00-18.00,00*7c2b\r\n - может быть не с начала */
    if (am3_get_data(str, MODEM_BUF_LEN)) {

	log_write_log_file("RX: am3_get_alarm_time: %s", str);
	for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
	    if (str[pos] == 0x2c) {
		pos += 1;
		break;
	    }
	}

	/* Разбираем буфер прям здесь! */
	memmove(str, str + pos, 14);	// от запятой
	// часы
	memcpy(buf, str, 2);
	buf[2] = 0;
	*h0 = atoi(buf);
	// минуты
	memcpy(buf, str + 3, 2);
	buf[2] = 0;
	*m0 = atoi(buf);
	// Чясы      
	memcpy(buf, str + 6, 2);
	buf[2] = 0;
	*h1 = atoi(buf);
	// минуты
	memcpy(buf, str + 9, 2);
	buf[2] = 0;
	*m1 = atoi(buf);
	res = 0;
    } else {
	log_write_log_file("RX: am3_get_alarm_time: got nothing\n");
    }

    return res;
}


/**
 *  Получить время на всплытие в минутах 
 */
#pragma section("FLASH_code")
u16 am3_get_popup_len(void)
{
    char str[MODEM_BUF_LEN];
    short res = -1;
    u16 crc16;
    int t0, pos = 0;
    snprintf(str, sizeof(str), "$RGATZ,,*ce1a\r\n");	/* Команда с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_get_popup_len: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    // Принято: $AGATZ,010,00*f78a
    if (am3_get_data(str, MODEM_BUF_LEN)) {

	log_write_log_file("RX: am3_get_popup_len: %s", str);
	// ищем 1-ю запятую:  $AGATZ,010,00*f78a
	for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
	    if (str[pos] == 0x2c) {
		pos += 1;
		break;
	    }
	}

	// копируем 3 символа после запятой
	if (MODEM_BUF_LEN - 16) {
	    memmove(str, str + pos, 3);
	    str[3] = 0;
	    res = atoi(str);
	}
    } else {
	log_write_log_file("RX: am3_get_popup_len: got nothing\n");
    }
    return res;
}


/**
 * Получить время пережига проволоки в секундах 
 */
#pragma section("FLASH_code")
u16 am3_get_burn_len(void)
{
    char str[MODEM_BUF_LEN];
    short res = -1;
    u16 crc16;
    int t0, pos = 0;
    snprintf(str, sizeof(str), "$RGABT,,*1cd6\r\n");	/* Команда с контрольной суммой */
    am3_write_data(str, strlen(str));	/* Посылаем команду  */
    log_write_log_file("TX: am3_get_burn_len: %s", str);
    am3_wait_reply(WAIT_MODEM_TIME_MS);
    // Принято: $AGABT,0002,00
    if (am3_get_data(str, MODEM_BUF_LEN)) {
	log_write_log_file("RX: am3_get_burn_len: %s", str);
	// ищем 1-ю запятую:  $AGABT,0002,00*
	for (pos = 0; pos < MODEM_BUF_LEN - 16; pos++) {
	    if (str[pos] == 0x2c) {
		pos += 1;
		break;
	    }
	}

	// копируем 4 символа после запятой
	if (pos < MODEM_BUF_LEN - 16) {
	    memmove(str, str + pos, 4);
	    str[4] = 0;
	    res = atoi(str);
	}
    } else {
	log_write_log_file("RX: am3_get_burn_len: got nothing\n");
    }

    return res;
}

/* Ожидать ответа ms миллисекунд */
#pragma section("FLASH_code")
static void am3_wait_reply(int ms)
{
    s64 t0;
    t0 = get_msec_ticks() + ms;
    while (get_msec_ticks() < t0) {
	LED_blink();
	if (pModem_xchg_buf->rx_fin)
	    break;
    }
}

/**
 * Перекачать из буфера приема модема в буфер функции
 */
#pragma section("FLASH_code")
static int am3_get_data(char *buf, int size)
{
    int res = 0;
    /* Найдем подстроку в этом мусоре */
    if (pModem_xchg_buf->rx_fin) {
	memset(buf, 0, size);
	res = (size > pModem_xchg_buf->rx_cnt) ? pModem_xchg_buf->rx_cnt : size;
	memcpy(buf, pModem_xchg_buf->rx_buf, res);
    }

    pModem_xchg_buf->rx_cnt = 0;
    pModem_xchg_buf->rx_fin = 0;
    return res;
}

/**
 * Посылаем команду в модем
 */
#pragma section("FLASH_code")
int am3_write_data(char *buf, int size)
{
    int res = 0;
    pModem_xchg_buf->rx_fin = 0;	/* сбросим приемный буфер */
    pModem_xchg_buf->rx_buf[0] = 0;
    res = UART1_write_str(buf, size);
    return res;
}
