#ifndef _COM_CMD_H
#define _COM_CMD_H

#include "globdefs.h"


/*******************************************************************
 *  Входящие команды
 *******************************************************************/
#define		UART_CMD_NONE			0x00	/* Нет команды */	

/* Однобайтные команды. Первый полубайт означал что данных нет-сейчас это не учитывается */
#define 	UART_CMD_COMMAND_PC		0x01	/* Перейти в режим работы с PC или ответить Who Are you? */
#define 	UART_CMD_GET_COUNTS		0x02    /* Выдай счетчики обменов */
#define		UART_CMD_GET_MODEM_REPLY	0x03	/* Получить ответ модема - из буфера модема */
#define		UART_CMD_DSP_RESET		0x04	/* Сброс DSP */
#define 	UART_CMD_DEV_STOP	        0x05	/* Стоп измерения  */
#define		UART_CMD_INIT_TEST		0x06	/* Запуск тестирования */
#define		UART_CMD_CLR_BUF		0x07	/* Занулить буфер с данными */
#define		UART_CMD_GET_RTC_CLOCK		0x08	/* Получить время прибора по RTC */
#define 	UART_CMD_GET_WORK_TIME		0x09	/* Получить время работы DSP */
#define		UART_CMD_ZERO_ALL_EEPROM	0x0A	/* Занулить eeprom */

#define		UART_CMD_GET_DSP_STATUS		0x0C	/* Получить статус: данные с батареи и температуру с давлением */
#define 	UART_CMD_GET_DATA	        0x10	/* Выдать пачку данных */

#define		UART_CMD_POWER_OFF		0x12    /* Управление питанием: выключение */
#define		UART_CMD_GET_ADC_CONST		0x13	/* Получить Константы */
#define		UART_CMD_MODEM_ON		0x14	/* Управление реле: модем ON */
#define		UART_CMD_MODEM_OFF		0x15	/* Управление реле: модем OFF */
#define 	UART_CMD_BURN_ON		0x16    /* Управление реле: burn ON */
#define 	UART_CMD_BURN_OFF		0x17    /* Управление реле: burn OFF */

#define		UART_CMD_GPS_ON			0x18	/* Управление GPS вкл */
#define		UART_CMD_GPS_OFF		0x19	/* Управление GPS выкл */
#define		UART_CMD_NMEA_GET		0x1a	/* Выдать строку GPS NMEA */

#define		UART_CMD_MODEM_REQUEST		0x23    /* Послать команду модему  */
#define		UART_CMD_SET_DSP_ADDR		0x24	/* Установить адрес станции */
#define		UART_CMD_SYNC_RTC_CLOCK		0x48	/* Установить время прибора. Синхронизация RTC */
#define		UART_CMD_SET_WORK_TIME		0xC9    /* Установить или стереть время работы */
#define 	UART_CMD_DEV_START	        0x85	/* Старт измерения */
#define		UART_CMD_SET_ADC_CONST		0x93	/* Установить / Записать в EEPROM константы */

#endif /* _COM_CMD_H */
