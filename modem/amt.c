/*****************************************************************************  
 *  Модем teledyne benthos
 *****************************************************************************/
#include <string.h>
#include <stdio.h>
#include "com_cmd.h"
#include "comport.h"
#include "amt.h"
#include "log.h"
#include "led.h"
#include "uart1.h"
#include "utils.h"
#include "ports.h"


#define 	BENTHOS_BUF_LEN			256		/* Вполне достаточно */
#define		UART_BENTHOS_MODEM_SPEED	UART_SPEED_9600	/* Скорость порта для модема  */

/************************************************************************
 * 	Статические переменные
 ************************************************************************/
/* Обмен с модемам - указатель */
static struct BENTHOS_XCHG_STRUCT {
    c8 rx_buf[BENTHOS_BUF_LEN];
    u8 rx_beg;			/* Начало пакета */
    u8 rx_cnt;			/* Счетчик принятого */
    u8 rx_fin;			/* Конец приема */
    u8 tx_len;			/* Сколько передавать  */
} *benthos_xchg_buf;


static void benthos_modem_read_ISR(u8);
static int amt_write_data(char *, int);
static int amt_wait_for(const char *, char *, int, int);


/**
 * Инициализация с проверкой
 */
#pragma section("FLASH_code")
int amt_init(void)
{
    DEV_UART_STRUCT com_par;
    int res = -1;

    select_modem_module();	/* Переключаем селектор на аналоговый модем  */

    /* Для обслуживания обмена создаем буфер на прием */
    if (benthos_xchg_buf == NULL) {
	benthos_xchg_buf = calloc(1, sizeof(struct BENTHOS_XCHG_STRUCT));
	if (benthos_xchg_buf == NULL) {
	    log_write_log_file("Error: can't alloc buf for amt\n");
	    return res;
	}
    } else {
	log_write_log_file("WARN: amt buf dev already exists\n");
    }


    /* Вызываем UART1 init */
    com_par.baud = UART_BENTHOS_MODEM_SPEED;
    com_par.rx_call_back_func = benthos_modem_read_ISR;
    com_par.tx_call_back_func = NULL;	/* Нет  */

    if (UART1_init(&com_par) == true)
	res = 0;

    return res;
}


/* Закрыть UART  */
#pragma section("FLASH_code")
void amt_close(void)
{
    UART1_close();

    if (benthos_xchg_buf) {
	free(benthos_xchg_buf);	/* Освобождаем буфер  */
	benthos_xchg_buf = NULL;
    }
}


/**
 * Программирование модема с проверкой
 */
#pragma section("FLASH_code")
int amt_prg_modem(void *p)
{
    int t0, t1, i, res = -1;
    char str[BENTHOS_BUF_LEN];
    u8 h0, m0, h1, m1;
    TIME_DATE td;
    GNS110_PARAM_STRUCT *params;


    if (p == NULL)
	return -1;

    params = (GNS110_PARAM_STRUCT *) p;	/* Переданное время */


    /* Номер модема */
    log_write_log_file("INFO: modem: %d\n", params->gns110_modem_num);

    /* Позиция установки */
    log_write_log_file("INFO: release code: %d\n", params->gns110_pos);


    /* аварийное время всплытия от модема   */
    sec_to_str(params->gns110_modem_alarm_time, str);
    log_write_log_file("INFO: modem alarm time: %s\n", str);


    select_analog_power();
    delay_ms(50);

    /* Включаем реле модема! */
    modem_on();


    do {
	/* Установка акустического модема benthos - только подстройка времени */
	if (amt_init() < 0) {
	    break;
	}

	old_modem_reset();	/* Сброс старого модема, ну знаю, нужно ли это делать для бентоса */
	log_write_log_file("INFO: reset modem and wait\n");
	res = amt_reset_modem();

	log_write_log_file("INFO: set modem parameters\n");
	res = amt_set_modem_params(params);

	amt_close();

	/* Возвращаем назад порт отладки */
	if (comport_init() != 0) {
	    break;
	}

    }
    while (0);
    log_write_log_file("INFO: end program modem\n");


    return res;
}



/**
 * Включить радио и лампочки
 */
#pragma section("FLASH_code")
int amt_set_radio(void *p)
{
    int t0, t1, i, res = -1;
    int h, m;
    char str[BENTHOS_BUF_LEN];
    TIME_DATE td;
    GNS110_PARAM_STRUCT *params;


    if (p == NULL)
	return -1;

    params = (GNS110_PARAM_STRUCT *) p;	/* Переданное время */

    select_analog_power();
    delay_ms(50);

    /* Включаем реле модема! */
    modem_on();

    do {
	/* Установка акустического модема benthos - только подстройка времени */
	if (amt_init() < 0) {
	    break;
	}

	old_modem_reset();	/* Сброс старого модема, ну знаю, нужно ли это делать для бентоса */
	log_write_log_file("INFO: reset modem and wait\n");
	i = amt_reset_modem();
	if (i < 0)
	    break;


	// Включаем радиомодуль
	sprintf(str, "%s", "ATTO0,1\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for(">", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	// определим светлое время переданное
	// Включаем лампочку, если темное время
	memset(&td, 0, sizeof(td));
	td.hour = params->gns110_modem_h0_time;
	td.min = params->gns110_modem_m0_time;
	t0 = td_to_sec(&td);


	td.hour = params->gns110_modem_h1_time;
	td.min = params->gns110_modem_m1_time;
	t1 = td_to_sec(&td);


	i = get_sec_ticks();
	sec_to_td(i, &td);
	h = td.hour;
	m = td.min;
	memset(&td, 0, sizeof(td));
	td.hour = h;
	td.min = m;
	i = td_to_sec(&td);


	// 2 варианта соответсвия
	if ((t1 > t0 && (i < t0 || i > t1)) || (t0 > t1 && (i > t1 || i < t0))) {
	    sprintf(str, "%s", "ATTO1,1\n");
	    amt_write_data(str, strlen(str));
	    log_write_log_file("send: %s\n", str);
	    i = amt_wait_for(">", str, BENTHOS_BUF_LEN, 1500);
	    log_write_debug_str("recv: %s\n", str);
	    if (i <= 0) {
		break;
	    }
	}
	res = 0;
    } while (0);
//vvvvv: надо порт выключать?
    return res;
}

/**
 * Программирование модема с проверкой
 */
#pragma section("FLASH_code")
int amt_check_modem(void)
{
    int t0, t1, i = 5, res = -1;
    char str[BENTHOS_BUF_LEN];
    u8 h0, m0, h1, m1;
    TIME_DATE td;

    select_analog_power();
    delay_ms(50);

    do {
	/* Установка акустического модема benthos - только подстройка времени */
	if (amt_init() < 0) {
	    break;
	}

	old_modem_reset();	/* Сброс старого модема, ну знаю, нужно ли это делать для бентоса */
	log_write_debug_str("INFO: reset modem and wait\n");

	/* Ожидаем строк CONNECT  */
	res = amt_wait_for("CONNECT", str, BENTHOS_BUF_LEN, 20000);
	log_write_debug_str("INFO: %s\n", str);

	amt_write_data("+++", 3);
	res = amt_wait_for(">", str, BENTHOS_BUF_LEN, 1000);
	log_write_debug_str("INFO: %s\n", str);

    } while (0);
    log_write_debug_str("INFO: end check modem\n");

    /* Возвращаем назад порт отладки */
    if (comport_init() == 0) {
	res = 0;
    }


    return res;
}


/* Ожидать ответа ms миллисекунд */
#pragma section("FLASH_code")
static void amt_wait_reply(int ms)
{
    s64 t0;

    t0 = get_msec_ticks() + ms;
    while (get_msec_ticks() < t0)
	LED_blink();
}


/**
 * Посылаем команду в модем
 */
#pragma section("FLASH_code")
static int amt_write_data(char *buf, int size)
{
    int res = 0;

    memset(benthos_xchg_buf, 0, sizeof(struct BENTHOS_XCHG_STRUCT));
    res = UART1_write_str(buf, size);
    return res;
}



/**
 * Обслуживаение прерываний от бентос
 */
#pragma section("FLASH_code")
static void benthos_modem_read_ISR(u8 rx_byte)
{
    benthos_xchg_buf->rx_buf[benthos_xchg_buf->rx_cnt % BENTHOS_BUF_LEN] = rx_byte;
    benthos_xchg_buf->rx_cnt++;
}



/* Ожидать подстроку с начала: ms миллисекунд */
#pragma section("FLASH_code")
static int amt_wait_for(const char *substr, char *str, int size, int ms)
{
    s64 t0;
    char *p, *p1;
    int len /* = -1 */ ;

    t0 = get_msec_ticks() + ms;

    do {
	/* Если строк нету - просто ждем  */
	if (substr != NULL && size != 0 && str != NULL) {
	    p = strstr(benthos_xchg_buf->rx_buf, substr);
	    p1 = strstr(benthos_xchg_buf->rx_buf, "Error");

	    memset(str, 0, size);

	    if (p != NULL || p1 != NULL) {
		delay_ms(50);
		len = (size > benthos_xchg_buf->rx_cnt) ? benthos_xchg_buf->rx_cnt : size;
/*
                log_write_debug_str("INFO: %s, len(%d)\n", p, len); 
                log_write_debug_str("INFO: %s, len(%d)\n", p1, len); 
*/
		str[len] = 0;
		benthos_xchg_buf->rx_cnt = 0;

		if (p == NULL)
		    len = -1;
		break;
	    }
	}
	LED_blink();
    } while (get_msec_ticks() < t0);
    return len;
}


/**
 * Сброс модема
 */
#pragma section("FLASH_code")
int amt_reset_modem(void)
{
    char str[BENTHOS_BUF_LEN];
    int res = -1;
    int i = 5;

    /* Ожидаем начала диалога */
    amt_write_data("+++", 3);
    log_write_debug_str("send: %s\n", "+++");
    if (amt_wait_for(">", str, BENTHOS_BUF_LEN, 1000) > 0)
	log_write_debug_str("recv: %s\n", str);


    /* Ожидаем строк CONNECT  */
    do {
	amt_write_data("ATES\n", 5);
	log_write_debug_str("send: %s\n", "ATES");
	if (amt_wait_for("CONNECT", str, BENTHOS_BUF_LEN, 15000) > 0) {
	    log_write_debug_str("recv: %s\n", str);
	    break;
	}
    } while (i--);

    if (i <= 1) {
	log_write_log_file("ERROR: reset modem\n");
	return -1;
    }


    /* Ожидаем начала приглашения */
    delay_ms(1000);
    amt_write_data("+++\n", 4);
    log_write_debug_str("send: %s\n", "+++");
    if (amt_wait_for("user", str, BENTHOS_BUF_LEN, 1500) > 0) {
	log_write_debug_str("recv0: %s\n", str);
	res = 0;
    } else if (amt_wait_for("factory", str, BENTHOS_BUF_LEN, 1500) > 0) {
	log_write_debug_str("recv1: %s\n", str);
	res = 0;
    } else {
	res = -1;
	log_write_debug_str("recv2: %s\n", benthos_xchg_buf->rx_buf);
    }

    return res;
}


/**
 * Установить время пережига
 * Считаем что в командном режиме
 */
#pragma section("FLASH_code")
int amt_set_modem_params(void *p)
{
    char str[BENTHOS_BUF_LEN];
    GNS110_PARAM_STRUCT *params;
    int t0, time;
    TIME_DATE td;
    int res = -1, i;


    do {

	if (p == NULL)
	    break;

	params = (GNS110_PARAM_STRUCT *) p;	/* Переданное время */

	// установим привилегии
	sprintf(str, "%s", "setpriv factory\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("password", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	// Пошлем пароль
	sprintf(str, "%s", "nobska\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("factory", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	//Номер модема
	sprintf(str, "@LocalAddr=%d\n", params->gns110_modem_num);
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("LocalAddr", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}


	//Код всплытия (позиция)
	sprintf(str, "@RlsCode=%d\n", params->gns110_pos);
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("RlsCode", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	//Пережиг 40 сек
	sprintf(str, "%s", "@RlsType=1\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("RlsType", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	// Время
	time = get_sec_ticks();
	if (sec_to_td(time, &td) < 0)
	    break;
	sprintf(str, "date -t%02d:%02d:%02d -d%02d/%02d/%04d -24\n", td.hour, td.min, td.sec, td.month, td.day, td.year);
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("Ok", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}
	// Время пережыга (через сколько?)
	t0 = params->gns110_modem_alarm_time - time;
	t0 /= 3600;		// в часах
	if (t0 == 0)
	    t0 = 1;
	sprintf(str, "@TimedRelease=%d\n", t0);
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("TimedRelease", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	// Сохранить
	sprintf(str, "%s", "cfg store\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("stored", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}
	// user
	sprintf(str, "%s", "setpriv user\n");
	amt_write_data(str, strlen(str));
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("user", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	// Низкое потребление
	amt_write_data("ATL\n", 4);
	log_write_log_file("send: %s\n", str);
	i = amt_wait_for("Lowpower", str, BENTHOS_BUF_LEN, 1500);
	log_write_debug_str("recv: %s\n", str);
	if (i <= 0) {
	    break;
	}

	res = 0;

    } while (0);
    return res;
}
