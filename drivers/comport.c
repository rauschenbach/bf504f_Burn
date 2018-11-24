#include <string.h>
#include <stdio.h>
#include "comport.h"
#include "com_cmd.h"
#include "main.h"
#include "crc16.h"
#include "log.h"
#include "led.h"
#include "uart1.h"
#include "gps.h"
#include "led.h"
#include "utils.h"
#include "ports.h"


//#define 	UART_DEBUG_SPEED		115200
//#define 	UART_DEBUG_SPEED		230400	/* Скорость порта для обмена с PC  */
#define 	UART_DEBUG_SPEED		460800
#define 	RX_BUF_LEN			255
#define 	TX_BUF_LEN			255


/************************************************************************
 * 	Статические переменные
 ************************************************************************/
/* Указатель на нашу структуру - пакет на прием */
static struct RX_DATA_STRUCT {
    u8 rx_buf[RX_BUF_LEN];	/* На прием */
    u8 tx_buf[TX_BUF_LEN];	/* И на передачу */

    u8 rx_beg;			/* Начало пакета */
    u8 rx_cmd;			/* Команда  */
    u8 rx_cnt;			/* Счетчик принятого */
    u8 rx_len;			/* Всего принято */

    u8 rx_fin;			/* Конец приема */
    u8 rx_ind;
    u8 tx_cnt;
    u8 tx_len;

    u8 tx_ind;
    u8 tx_fin;			/* Конец приема */
    u16 crc16;			/* Контрольная сумма CRC16 */
} *xchg_buf;

/************************************************************************
 * 	Статические функции
 ************************************************************************/
static void comport_debug_write_ISR(void);
static void comport_debug_read_ISR(u8);
static void comport_rx_end(int);


/**
 * Description: Инициализирует UART на прием обмен данными
 */
#pragma section("FLASH_code")
int comport_init(void)
{
    DEV_UART_STRUCT com_par;
    int res = -1;

#if 1
    select_debug_module();	/* Переключаем селектор на отладочный порт  */
#else

    select_modem_module(); /* ПОРТ ДЛЯ модема */
#endif

    /* Для обслуживания обмена создаем буфер на прием */
    if (xchg_buf == NULL) {
	xchg_buf = calloc(1, sizeof(struct RX_DATA_STRUCT));
	if (xchg_buf == NULL) {
	    log_write_log_file("Error: can't alloc buf for comport\n");
	    return -2;
	}
    } else {
	log_write_log_file("WARN: comport buf dev already exists\n");
    }

    /* Вызываем UART1 init */
    com_par.baud = UART_DEBUG_SPEED;
    com_par.rx_call_back_func = comport_debug_read_ISR;
    com_par.tx_call_back_func = comport_debug_write_ISR;

    if (UART1_init(&com_par) == true) {
	res = 0;
    }

    xchg_buf->crc16 = 0xAA;
    return res;			// все ок
}

/* Закрыть UART  */
#pragma section("FLASH_code")
void comport_close(void)
{
    UART1_close();

    if (xchg_buf) {
	free(xchg_buf);		/* Освобождаем буфер  */
	xchg_buf = NULL;
    }
}



/* Выдаем полученную комунду или 0 */
#pragma section("FLASH_code")
u8 comport_get_command(void)
{
    if (xchg_buf->rx_fin)
	return xchg_buf->rx_cmd;
    else
	return 0;
}



/* Обслуживание командного режима */
section("L1_code")
static void comport_debug_read_ISR(u8 rx_byte)
{
    u8 cPar;
    u16 wPar;
    u32 lPar0, lPar1, lPar2;
    DEV_UART_COUNTS cnt;
    DEV_UART_CMD uart_cmd;	/* Команда пришла с UART */

    /* Вычисляем CRC16. При правильном приеме последние числа д.б. = 0 */
    xchg_buf->rx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
    xchg_buf->crc16 = (u16) (((xchg_buf->crc16 & 0xff) << 8) | (rx_byte & 0xff));
    xchg_buf->crc16 ^= get_crc16_table(xchg_buf->rx_ind);


    /* Пришел самый первый байт посылки и наш адрес */
    if (xchg_buf->rx_beg == 0 && rx_byte == 0xFF) {	/* Первый байт приняли: СТАРТ - FF */
	xchg_buf->rx_beg = 1;	/* Один байт приняли */
	xchg_buf->crc16 = rx_byte;	/* Контрольная сумма равна первому байту */
	xchg_buf->rx_cnt = 1;	/* Счетчик пакетов - первый байт приняли */
    } else {			/* Пришли последующие байты */
	if (xchg_buf->rx_cnt == 1) {
	    if (rx_byte != 0)	// рвем передачу
		comport_rx_end(3);
	} else if (xchg_buf->rx_cnt == 2) {
	    if (rx_byte == 0)	// рвем передачу-нуля быть не может
		comport_rx_end(3);
	    xchg_buf->rx_len = rx_byte;	/* Второй байт это длина всей следующей посылки - не может быть 0! */

	    /* 3...все остальное считается как единая посылка если она есть. Или как команда модему и пр. */
	} else if (xchg_buf->rx_cnt > 2 && xchg_buf->rx_cnt < (xchg_buf->rx_len + 3)) {	/* Данные начинаются с 4-го байта */
	    xchg_buf->rx_buf[xchg_buf->rx_cnt - 3] = rx_byte;	/* В приемный буфер принятую посылку */

	} else if (xchg_buf->rx_cnt == (xchg_buf->rx_len + 3)) {	/* Ст. байт контрольной суммы  */
	    asm("nop;");	/* ничего не делаем - нужно чтобы не входить в посл. условие */
	} else if (xchg_buf->rx_cnt == (xchg_buf->rx_len + 4)) {	/* Мл. байт контрольной суммы  */

	    /*  Crc16 правильная ? */
	    if (xchg_buf->crc16 == 0) {
		comport_rx_end(0);

		// какая под-команда?
		xchg_buf->rx_cmd = xchg_buf->rx_buf[0];

		/* Разбор команд  */
		switch (xchg_buf->rx_cmd) {

/************************************************************************************************
 * Однобайтные команды 
 ************************************************************************************************/
		    /* Передаем свое имя - будет в буфере передачи */
		case UART_CMD_COMMAND_PC:
		    cmd_get_dsp_addr(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Выдать статус устройтства - будет в буфере передачи */
		case UART_CMD_GET_DSP_STATUS:
		    cmd_get_dsp_status(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Получить время GNS110 - будет в буфере передачи */
		case UART_CMD_GET_RTC_CLOCK:
		    cmd_get_gns110_rtc(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Выдать счетчики обменов - длина нашей структуры (там нет CRC и начальной len, поэтому -3 не делаем  */
		case UART_CMD_GET_COUNTS:
		    UART1_get_count(&cnt);	/* Получить счетчики обмена */
		    xchg_buf->tx_buf[0] = sizeof(DEV_UART_COUNTS);
		    memcpy(&xchg_buf->tx_buf[1], &cnt, sizeof(DEV_UART_COUNTS));
		    UART1_start_tx();
		    break;


		    /* Очистить буфер данных */
		case UART_CMD_CLR_BUF:
		    cmd_clear_adc_buf(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Запуск тестирования */
		case UART_CMD_INIT_TEST:
		    uart_cmd.cmd = UART_CMD_INIT_TEST;	// команда
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Получить все времена работы DSP */
		case UART_CMD_GET_WORK_TIME:
		    cmd_get_work_time(xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Обнулить EEPROM */
		case UART_CMD_ZERO_ALL_EEPROM:
		    uart_cmd.cmd = UART_CMD_ZERO_ALL_EEPROM;	// команда
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* Управление reset DSP */
		case UART_CMD_DSP_RESET:
		    uart_cmd.cmd = UART_CMD_DSP_RESET;	// команда
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* Управление питание DSP: poweroff */
		case UART_CMD_POWER_OFF:
		    uart_cmd.cmd = UART_CMD_POWER_OFF;	// команда
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

		    /* команда включить реле модема */
		case UART_CMD_MODEM_ON:
		    uart_cmd.cmd = UART_CMD_MODEM_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* команда вЫключить реле модема */
		case UART_CMD_MODEM_OFF:
		    uart_cmd.cmd = UART_CMD_MODEM_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* Пережыг включить */
		case UART_CMD_BURN_ON:
		    uart_cmd.cmd = UART_CMD_BURN_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* Пережыг вЫключить */
		case UART_CMD_BURN_OFF:
		    uart_cmd.cmd = UART_CMD_BURN_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* команда включить GPS */
		case UART_CMD_GPS_ON:
		    uart_cmd.cmd = UART_CMD_GPS_ON;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* команда вЫключить GPS */
		case UART_CMD_GPS_OFF:
		    uart_cmd.cmd = UART_CMD_GPS_OFF;
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* команда выдать строку NMEA */
		case UART_CMD_NMEA_GET:
		    if (gps_get_nmea_string((c8 *) xchg_buf->tx_buf + 1, NMEA_GPRMC_STRING_SIZE) > 0) {
			xchg_buf->tx_buf[0] = NMEA_GPRMC_STRING_SIZE;
		    } else {
			cmd_get_dsp_status(xchg_buf->tx_buf);
			xchg_buf->tx_buf[0] = 2;	// 2 байта - не готово еще!
		    }
		    UART1_start_tx();	/* Передача */
		    break;


		    /*  Выдать данные */
		case UART_CMD_GET_DATA:
		    cmd_get_adc_data(xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;


		    /* Передаем свое имя и адрес. параметр u16 в приемном буфере с адреса + 1 */
		case UART_CMD_SET_DSP_ADDR:
		    wPar = get_short_from_buf(xchg_buf->rx_buf, 1);
		    cmd_set_dsp_addr(wPar, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;

/***********************************************************************************************
 * 	Пятибайтные команды  
 ************************************************************************************************/
		    /* Синхронизация RTC, параметр в приемном буфере + 1 байт */
		case UART_CMD_SYNC_RTC_CLOCK:
		    lPar0 = get_long_from_buf(xchg_buf->rx_buf, 1);
		    cmd_set_gns110_rtc(lPar0, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

/***********************************************************************************************
 * 	12 - байтные команды  
 ************************************************************************************************/
		    /* Установить / сбросить все времена работы DSP */
		case UART_CMD_SET_WORK_TIME:
		    lPar0 = get_long_from_buf(xchg_buf->rx_buf, 1);
		    lPar1 = get_long_from_buf(xchg_buf->rx_buf, 5);
		    lPar2 = get_long_from_buf(xchg_buf->rx_buf, 9);
		    cmd_set_work_time(lPar0, lPar1, lPar2, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

/***********************************************************************************************
 * 	многобайтные команды  
 ************************************************************************************************/
		    /* Установить / Записать в EEPROM константы */
		case UART_CMD_SET_ADC_CONST:
		    cmd_set_adc_const(&xchg_buf->rx_buf[1], xchg_buf->tx_buf);	/* Копируем буфер в настройки. первый байт будет команда - со смещением! */
		    UART1_start_tx();	/* Передача */
		    break;

		    /* Получить Константы всех каналов. Первый байт будет длина посылки  */
		case UART_CMD_GET_ADC_CONST:
		    cmd_get_adc_const(xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;


		    /* Старт измерений. Первый байт будет команда - со смещением! */
		case UART_CMD_DEV_START:
		    uart_cmd.cmd = UART_CMD_DEV_START;	/* сколько байт пришло для этой команды? 24 байта */
		    memcpy(uart_cmd.u.cPar, &xchg_buf->rx_buf[1], 24);
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();	/* Передача */
		    break;

		    /* команда cтоп измерения */
		case UART_CMD_DEV_STOP:
		    uart_cmd.cmd = UART_CMD_DEV_STOP;	/* Стоп, параметров нет */
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);
		    UART1_start_tx();
		    break;


/***********************************************************************************************
 * 	многобайтные команды - команды модему
 ************************************************************************************************/

		    /* Перетранслируем команду модему - в буфере приема с 2 го байт находится команда модему. 
		     * С переводом строки, CRC16.
		     * после посылки этой команды - порт должен отключиться на нек. время */
		case UART_CMD_MODEM_REQUEST:
		    uart_cmd.cmd = UART_CMD_MODEM_REQUEST;	// Команду поставили
		    cPar = xchg_buf->rx_buf[0] - 1;	// длина строки БЕЗ команды
		    uart_cmd.len = cPar % MODEM_BUF_LEN;	// запишем в это место длину
		    memcpy(&uart_cmd.u.cPar, xchg_buf->rx_buf + 1, cPar % MODEM_BUF_LEN);	// Скопировали для main.c
		    make_extern_uart_cmd(&uart_cmd, xchg_buf->tx_buf);	// в tx_buf лежит ответ
		    UART1_start_tx();	/* Передача */
		    break;

		    /* Получить ответ модема - из буфера модема */
		case UART_CMD_GET_MODEM_REPLY:
		    get_uart_cmd_buf(&uart_cmd);	// ответ
		    cPar = uart_cmd.len % MODEM_BUF_LEN;	// длина сообщения (не более 64)
		    if (cPar > 0) {
			memcpy(&xchg_buf->tx_buf[1], uart_cmd.u.cPar, cPar);	// Копируем в буфер UART
			xchg_buf->tx_buf[0] = cPar;	// Длина сообщения
			memset(&uart_cmd, 0, sizeof(uart_cmd));
		    } else {
			cmd_get_dsp_status(xchg_buf->tx_buf);
			xchg_buf->tx_buf[0] = 2;
			xchg_buf->tx_buf[1] |= 0x08;	// данные не готовы еще
		    }
		    UART1_start_tx();	/* Передача */
		    break;


		default:
		    comport_rx_end(2);
		    break;
		}

	    } else {		/* Контрольная сумма неверна-начинаем прием заново */
		comport_rx_end(1);
	    }

	} else {		/* Если что-то пошло не так */
	    comport_rx_end(2);
	}
	xchg_buf->rx_cnt++;	/* Счетчик пакетов */
    }
}


/**
 * Конец приема
 */
section("L1_code")
static void comport_rx_end(int err)
{
    xchg_buf->rx_beg = 0;
    xchg_buf->rx_cnt = 0;

    if (err == 0) {
	xchg_buf->rx_fin = 1;	/* Посылка принята OK */
	xchg_buf->tx_cnt = 0;	/* можно передавать */
	xchg_buf->tx_fin = 0;	/* Начало передачи */
    } else if (err == 1) {	/* Контрольная сумма неверна-начинаем прием заново */
	xchg_buf->rx_fin = 0;	/* Посылка приянята Error */
    } else {			/* Если что-то пошло не так */
    }
}


/* Обслуживание командного режима (запись) */
section("L1_code")
static void comport_debug_write_ISR(void)
{
    register u8 tx_byte;

    if (xchg_buf->tx_cnt == 0) {
	xchg_buf->tx_len = xchg_buf->tx_buf[0];	// Длина ответной посылки

	if (xchg_buf->tx_len == 0)
	    return;

	tx_byte = xchg_buf->tx_len;
	xchg_buf->crc16 = tx_byte;	/* Первый байт и crc - длина */
	xchg_buf->tx_ind = 0;
    } else if (xchg_buf->tx_cnt <= xchg_buf->tx_len) {
	tx_byte = xchg_buf->tx_buf[xchg_buf->tx_cnt];
	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) (((xchg_buf->crc16 & 0xff) << 8) | (tx_byte & 0xff));
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);
    } else if (xchg_buf->tx_cnt == xchg_buf->tx_len + 1) {	/* Считаем контрольную сумму передачи 2 раза! */
	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) ((xchg_buf->crc16 & 0xff) << 8);
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);

	xchg_buf->tx_ind = (u8) ((xchg_buf->crc16 >> 8) & 0xff);
	xchg_buf->crc16 = (u16) ((xchg_buf->crc16 & 0xff) << 8);
	xchg_buf->crc16 ^= get_crc16_table(xchg_buf->tx_ind);

	tx_byte = (xchg_buf->crc16 >> 8) & 0xff;	/* Ст. байт CRC16 */
    } else if (xchg_buf->tx_cnt == xchg_buf->tx_len + 2) {
	tx_byte = xchg_buf->crc16 & 0xff;	/* Мл. байт CRC16 */
	xchg_buf->tx_fin = 1;
	UART1_stop_tx();	/* Выключаем передатчик */
    } else {
	UART1_stop_tx();	/* Выключаем передатчик. На всякий... */
    }
    UART1_tx_byte(tx_byte);
    xchg_buf->tx_cnt++;
}
