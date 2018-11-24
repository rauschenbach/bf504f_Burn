/* В этом файле живут настройки */
#ifndef _CONFIG_H 
#define _CONFIG_H


/* Проект GNS110 или нет?  Может быть проект для Океанологии или для Geonode */
#define		GNS110_PROJECT	
//#undef		GNS110_PROJECT	

/* Новая плата R2a или R2b */
//#define       GNS110_R2A_BOARD
//#define       GNS110_R2B_BOARD
#define 	GNS110_R2C_BOARD


#if defined GNS110_R2A_BOARD && defined GNS110_R2B_BOARD && defined GNS110_R2C_BOARD
#error "Please choose only 1 variant GNS110_R2A_BOARD or GNS110_R2B_BOARD or GNS110_R2C_BOARD"
#endif

/* Падение напряжения при котором выключаем питание - 8.5 V */
#define 	POWER_DROP_MIN			8500

/* 
 * Формат выходной структуры будет изменен
 */
#define		ENABLE_NEW_SIVY
#undef		ENABLE_NEW_SIVY


/* Выводить принятую от Модема  в лог */
#define		ENABLE_DEBUG_MODEM
//#undef                ENABLE_DEBUG_MODEM


/* Запретить выключение таймера 2  */
#define		DISABLE_TIM2_OFF
#undef		DISABLE_TIM2_OFF


/* Сколько раз в секунду печатать данные АЦП - раз в 600 секунд  и пр. */
#define 	ADC_DATA_PRINT_COEF		60

#define		TIME_ONE_DAY			86400	

#define		TIME_ONE_WEEK			604800	

#define		TIME_ONE_MONTH			(TIME_ONE_WEEK * 8)			

/* Допустимая ошибка настройки для таймера 1  */
#define		TIM1_GRANT_ERROR  		3


/* Сколько ждать старта ATMEGA при ресете  */
#define		WAIT_START_ATMEGA	500

/* Допустимая ошибка настройки для таймера 3 - 4 мкс максимум, при которой выведем в лог! */
#define		TIM3_GRANT_ERROR  		100

/* Время ожидания подключения портов по WUSB или модема  */
#define		MODEM_POWERON_TIME_SEC			(12)


/* Число секунд, в течении которое будем ожидать входящей команды с PC - секунды */
#define 	WAIT_PC_TIME			120
#define 	WAIT_PC_COMMAND			WAIT_PC_TIME

#define 	WAIT_PC_CMD_MS			(WAIT_PC_TIME * 1000)

/* Число секунд, в течении которых мы будем ждать синхронизацию */
#define		WAIT_TIM3_SYNC			10

/* Время ожидания 3DFIX - 1 час */
#define 	WAIT_3DFIX_TIME			3600

/* Задержка старта после просыпания */
#define 	TIME_START_AFTER_WAKEUP		120

/* Минимальное время записи - 5 минут  */
#define 	RECORD_TIME			300

/* Задержка пережига после окончания  регистрации - 2 секунды */
#define 	TIME_BURN_AFTER_FINISH		2


/* Время пережигания проволки - это время будет прибавляться к  времени окончания  */
#define 	RELEBURN_TIME			1

/* Время всплытия - это время прибавится к времени пережига */
#define 	POPUP_DURATION		60


/* Время ожидания статусов в командном режиме, если нет связи - выключиться. 10 минут */
#define 	POWEROFF_NO_LINK_TIME		600



/**   
 * Конфигурация клоков для переферийных устройств
 */

/* Скорость обмена между експандером и атмегой - меньше четверти от 8 МГц   */
#define		ATMEGA_BF_XCHG_SPEED		1600000

/* Скорость SPI для работы с АЦП ADS1282 - половина от 4.096 МГц */
#define		ADS1282_SPI_SPEED		2000000

/* Скорость SPI для работы с внешними ЦАП */
#define		AD5640_SPEED			1000000

/* Уарт для приема строки NMEA */
#define		UART_SPEED_9600			9600
#define		UART_SPEED_115200		115200



/* Старый модем на 4800 */
#define		UART_OLDMODEM_SPEED		4800


/* Модем teledyne на 9600 */
#define		UART_TELEDYNE_SPEED		9600



#endif				/* config.h  */
