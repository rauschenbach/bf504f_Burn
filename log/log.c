/* Сервисные функции: ведение лога, подстройка таймера, запись на SD карту */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "main.h"
#include "timer1.h"
#include "timer2.h"
#include "ads1282.h"
#include "ports.h"
#include "gps.h"
#include "log.h"
#include "dac.h"
#include "irq.h"
#include "led.h"
#include "rsi.h"
#include "uart1.h"		/* Вывод или на SD карту или в UART */
#include "pll.h"
#include "eeprom.h"
#include "ff.h"
#include "version.h"

#define   	MAX_FILE_SIZE		1024
#define   	MAX_START_NUMBER	100	/* Максимальное число запусков */
#define	  	MAX_TIME_STRLEN		26	/* Длина строки со временем  */
#define   	MAX_LOG_FILE_LEN	134217728	/* 128 Мбайт */
#define   	MAX_FILE_NAME_LEN	31	/* Длина файла включая имя директории с '\0' */
#define		LOCK_FILE_NAME		"lock.fil"
#define		PARAM_FILE_NAME		"recparam.cfg"
#define 	ERROR_LOG_NAME		"error.log"
/*************************************************************************************
 *     Эти переменные не видимы в других файлах 
 *************************************************************************************/
static FATFS fatfs;		/* File system object - можно убрать из global? нет! */
static DIR dir;			/* Директория где храница все файло - можно убрать из global? */
static FIL log_file;		/* File object */
static FIL adc_file;		/* File object для АЦП */
static FIL env_file;		/* File object для параметров среды: температура и пр */
static ADC_HEADER adc_hdr;	/* Заголовок перед данными АЦП */
static int num_log = 0;		/* Номер лог файла, если режем по длине  */

/**
 * Для записи времени в лог - получить время, если таймер1 не запущен, то от RTC
 */
#pragma section("FLASH_code")
static void time_to_str(char *str)
{
	TIME_DATE t;
	u64 msec;
	char sym;
	int stat;

	msec = get_msec_ticks();	/* Получаем время от таймера 1 */

	stat = get_clock_status();
	if (stat == CLOCK_RTC_TIME)
		sym = 'R';
	else if (stat == CLOCK_NO_GPS_TIME)
		sym = 'G';
	else if (stat == CLOCK_PREC_TIME)
		sym = 'P';
	else
		sym = 'N';

	/* Записываем дату в формате: P: 09-07-2013 - 13:11:39.871  */
	if (sec_to_td(msec / TIMER_MS_DIVIDER, &t) != -1) {
		sprintf(str, "%c %02d-%02d-%04d %02d:%02d:%02d.%03d ", sym, t.day, t.mon, t.year, t.hour, t.min, t.sec,
			(u32) (msec % TIMER_MS_DIVIDER));
	} else {
		sprintf(str, "set time error ");
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Только для LOG файлов 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Инициализация файловой системы
 * При монтировании читаем данные с FLASH
 */
#pragma section("FLASH_code")
int log_mount_fs(void)
{
	int status;
	FRESULT res;		/* Result code */

	/* SD карту подключаем к BF */
	select_sdcard_to_bf();
	ssync();

	/* Монтируем ФС */
	asm("nop;");
	if ((res = f_mount(&fatfs, "", 1))) {
		return RES_MOUNT_ERR;
	}

	return RES_NO_ERROR;
}

/**
 * Карта монтирована или нет?
 */
#pragma section("FLASH_code")
bool log_check_mounted(void)
{
	return fatfs.fs_type;
}


/**
 * Проверка наличия лок-файла на SD - для определения-стоит ли ждать WUSB 2 минуты
 */
#pragma section("FLASH_code")
int log_check_lock_file(void)
{
	FIL lock_file;
	FRESULT res;		/* Result code */

	/* Открываем существующий файл */
	if (f_open(&lock_file, LOCK_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
		return RES_NO_LOCK_FILE;	/* Если файл не существует! */
	}

	/* Если файл существует - удалим его */
	res = f_unlink(LOCK_FILE_NAME);
	if (res)
		return RES_DEL_LOCK_ERR;	/* Не удалось */

	return RES_NO_ERROR;	/* Если файл существует */
}



/**
 * Проверка наличия файла регистрации на SD
 */
#pragma section("FLASH_code")
int log_check_reg_file(void)
{
	FIL file;
	FRESULT res;		/* Result code */

	/* Открываем существующий файл */
	if (f_open(&file, PARAM_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
		return RES_OPEN_PARAM_ERR;
	}

	/* Закроем файл */
	if (f_close(&file)) {
		return RES_CLOSE_PARAM_ERR;
	}

	return RES_NO_ERROR;	/* Если файл существует */
}



/**
 * Печатает всегда-может быть ошибка если UART не открыт
 */
#pragma section("FLASH_code")
int log_write_log_to_uart(char *fmt, ...)
{
	int r;
	char str[256];
	va_list p_vargs;	/* return value from vsnprintf  */

	va_start(p_vargs, fmt);
	r = vsnprintf(str, sizeof(str), fmt, p_vargs);
	va_end(p_vargs);
	if (r < 0)		/* formatting error?            */
		return RES_FORMAT_ERR;

	if (UART1_write_str(str, strlen(str)) == strlen(str))
		return RES_NO_ERROR;
	else
		return RES_WRITE_UART_ERR;
}



/**
 * Запись строки параметров в файл
 * Режем по 10 мБайт?
 */
#pragma section("FLASH_code")
int log_write_env_data_to_file(void *p)
{
	DEV_STATUS_STRUCT *status;
	unsigned bw;		/* Прочитано или записано байт  */
	FRESULT res;		/* Result code */
	int t0;
	char str0[256];
	char str1[32];

	if (p == NULL)
		return RES_WRITE_LOG_ERR;;

	status = (DEV_STATUS_STRUCT *) p;

	t0 = get_sec_ticks();
	sec_to_str(t0, str1);

	sprintf(str0, "%s \t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%3.1f\t%3.1f\t%d\t%c%3.1f\t%c%3.1f\t%c%3.1f\n",
		str1, status->regpwr_volt, status->ireg_sense, status->burn_ext_volt, status->burn_volt,
		status->iburn_sense, status->am_power_volt, status->iam_sense,
		status->temper0 / 10.0, status->temper1 / 10.0, status->press,
		status->pitch > 0 ? ' ' : '-', abs(status->pitch) / 10.0,
		status->roll > 0 ? ' ' : '-', abs(status->roll) / 10.0, status->head > 0 ? ' ' : '-',
		abs(status->head) / 10.0);


	*pIMASK &= ~EVT_IVG14;	/* Отключать handler 14, когда мы пишем в log */
	ssync();

	res = f_write(&env_file, str0, strlen(str0), &bw);
	if (res) {
		return RES_WRITE_LOG_ERR;
	}

	/* Обязательно запишем! */
	res = f_sync(&env_file);
	if (res) {
		return RES_SYNC_LOG_ERR;
	}

	/* Включать handler 14  */
	*pIMASK |= EVT_IVG14;
	ssync();

	return 0;
}



/**
 * Запись строки в лог файл, возвращаем сколько записали. С временем ВСЕГДА!
 * Режем по 10 мБайт?
 */
#pragma section("FLASH_code")
int log_write_log_file(char *fmt, ...)
{
	char str[256];
	FRESULT res;		/* Result code */
	unsigned bw;		/* Прочитано или записано байт  */
	int i, ret = 0;
	va_list p_vargs;	/* return value from vsnprintf  */

	/* Не монтировано (нет фс - работаем от PC), или с карточкой проблемы */
	if (log_file.obj.fs == NULL) {
		return RES_MOUNT_ERR;
	}

	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	time_to_str(str);	// Получить время всегда
	va_start(p_vargs, fmt);
	i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);

	va_end(p_vargs);
	if (i < 0)		/* formatting error?            */
		return RES_FORMAT_ERR;


	// Заменим переносы строки на UNIX (не с начала!)
	for (i = MAX_TIME_STRLEN + 4; i < sizeof(str) - 3; i++) {
		if (str[i] == 0x0d || str[i] == 0x0a) {
			str[i] = 0x0d;	// перевод строки
			str[i + 1] = 0x0a;	// Windows
			str[i + 2] = 0;
			break;
		}
	}

	*pIMASK &= ~EVT_IVG14;	/* Отключать handler 14, когда мы пишем в log */
	ssync();

	res = f_write(&log_file, str, strlen(str), &bw);
	if (res) {
		return RES_WRITE_LOG_ERR;
	}

	/* Обязательно запишем! */
	res = f_sync(&log_file);
	if (res) {
		return RES_SYNC_LOG_ERR;
	}
	/* Если больше 128 мБайт - режем и создаем след. файл. */
	if (f_size(&log_file) > MAX_LOG_FILE_LEN) {
		GNS110_PARAM_STRUCT gns110_param;
		get_gns110_start_params(&gns110_param);	/* Получили параметры  */

		res = f_close(&log_file);
		if (res) {
			return RES_CLOSE_LOG_ERR;
		}

		/* Открыть файл лога в этой директории всегда, создать если нет!  */
		sprintf(str, "%s/gns110.l%02d", gns110_param.gns110_dir_name, num_log++ % 100);
		if ((res = f_open(&log_file, str, FA_WRITE | FA_OPEN_ALWAYS))) {
			ret = RES_CREATE_LOG_ERR;
		}
	}

	/* Включать handler 14  */
	*pIMASK |= EVT_IVG14;
	ssync();

	return ret;
}

/**
 * Запись строки ошибки в лог файл
 */
#pragma section("FLASH_code")
int log_write_error_file(char *fmt, ...)
{
	char str[256];
	FIL error_log;		/* File object */
	FRESULT res;		/* Result code */
	unsigned bw;		/* Прочитано или записано байт  */
	int i, ret = 0;
	va_list p_vargs;	/* return value from vsnprintf  */

	/* Не монтировано */
	if (fatfs.fs_type == 0) {
		return RES_MOUNT_ERR;
	}

	va_start(p_vargs, fmt);
	i = vsnprintf(str, sizeof(str), fmt, p_vargs);
	va_end(p_vargs);
	if (i < 0)		/* formatting error?            */
		return RES_FORMAT_ERR;

	*pIMASK &= ~EVT_IVG14;	/* Отключать handler 14, когда мы пишем в log */
	ssync();

	/* Если все ОК - открыли error_log */
	res = f_open(&error_log, ERROR_LOG_NAME, FA_WRITE | FA_READ | FA_OPEN_ALWAYS);
	if (res) {
		return RES_WRITE_LOG_ERR;
	}

	/* Определим размер и будем дописывать */
	i = f_size(&error_log);
	if (i > 0x7F000000) {
		f_truncate(&error_log);
		i = 0;
	}
	/* Переставим указатель файла */
	f_lseek(&error_log, i);


	res = f_write(&error_log, str, strlen(str), &bw);
	if (res) {
		return RES_WRITE_LOG_ERR;
	}

	/* Обязательно запишем! */
	res = f_close(&error_log);
	if (res) {
		return RES_CLOSE_LOG_ERR;
	}

	/* Включать handler 14  */
	*pIMASK |= EVT_IVG14;
	ssync();

	return res;
}



/**
 * Запись строки в лог файл, без времени и переносов
 */
#pragma section("FLASH_code")
int log_write_debug_str(char *fmt, ...)
{
	char str[256];
	FRESULT res;		/* Result code */
	unsigned bw;		/* Прочитано или записано байт  */
	int i, ret = 0;
	va_list p_vargs;	/* return value from vsnprintf  */

	/* Не монтировано (нет фс - работаем от PC), или с карточкой проблемы */
	if (log_file.obj.fs == NULL) {
		return RES_MOUNT_ERR;
	}

	/* Получаем текущее время - MAX_TIME_STRLEN символов с пробелом - всегда пишем */
	time_to_str(str);	// Получить время всегда
	va_start(p_vargs, fmt);
	i = vsnprintf(str + MAX_TIME_STRLEN, sizeof(str), fmt, p_vargs);

	va_end(p_vargs);
	if (i < 0)		/* formatting error?            */
		return RES_FORMAT_ERR;


	*pIMASK &= ~EVT_IVG14;	/* Отключать handler 14, когда мы пишем в log */
	ssync();

	res = f_write(&log_file, str, strlen(str), &bw);
	if (res) {
		return RES_WRITE_LOG_ERR;
	}

	/* Обязательно запишем! */
	res = f_sync(&log_file);
	if (res) {
		return RES_SYNC_LOG_ERR;
	}

	/* Включать handler 14  */
	*pIMASK |= EVT_IVG14;
	ssync();

	return ret;
}

/**
 * Закрыть лог-файл
 */
#pragma section("FLASH_code")
int log_close_log_file(void)
{
	FRESULT res;		/* Result code */

	res = f_close(&log_file);
	if (res) {
		return RES_CLOSE_LOG_ERR;
	}

	/* Здесь же закроем и файл для среды и модема  */
	res = f_close(&env_file);
	if (res) {
		return RES_CLOSE_LOG_ERR;
	}


	rsi_power_off();	/* Выключаем RSI */
	return RES_NO_ERROR;
}


/**
 * Создаем заголовок - он будет использоваться для файла данных 
 * Вызывается из файла main
 * структуру будем переделывать!!!
 */
#pragma section("FLASH_code")
void log_create_adc_header(s64 gps_time, s64 drift, s32 lat, s32 lon)
{
	char str[MAX_TIME_STRLEN];	// 26 байт
#if defined		ENABLE_NEW_SIVY
	strncpy(adc_hdr.DataHeader, "SeismicDat1\0", 12);	/* Заголовок данных SeismicDat1\0 - новый Sivy */
	adc_hdr.HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */
	adc_hdr.GPSTime = gps_time;	/* Время синхронизации: наносекунды */
	adc_hdr.Drift = drift;	/* Дрифт от точных часов GPS: наносекунды  */
	adc_hdr.lat = lat;	/* широта: 55417872 = 5541.7872N */
	adc_hdr.lon = lon;	/* долгота:37213760 = 3721.3760E */
#else
	TIME_DATE data;
	long t0;
	u8 ind;

	t0 = gps_time / TIMER_NS_DIVIDER;
	sec_to_td(t0, &data);	/* время синхронизации сюда */
	strncpy(adc_hdr.DataHeader, "SeismicData\0", 12);	/* Дата ID  */
	adc_hdr.HeaderSize = sizeof(ADC_HEADER);	/* Размер заголовка     */
	adc_hdr.Drift = drift * 32768;	/* Дрифт настоящий - выведем в миллисекундах по формуле */

	memcpy(&adc_hdr.GPSTime, &data, sizeof(TIME_DATE));	/* Время получения дрифта по GPS */
	adc_hdr.NumberSV = 3;	/* Число спутников: пусть будет 3 */
	adc_hdr.params.coord.comp = true;	/* Строка компаса  */

	/* Координаты чудес - где мы находимся */
	if (lat == lon == 0)
		ind = 90;
	else
		ind = 12;	// +554177+03721340000009

	/* Исправлено странное копирование */
	snprintf(str, sizeof(str), "%c%06d%c%07d%06d%02d", (lat >= 0) ? '+' : '-', abs(lat / 100),
		 (lon >= 0) ? '+' : '-', abs(lon / 100), 0, ind);
	memcpy(adc_hdr.params.coord.pos, str, sizeof(adc_hdr.params.coord.pos));

#endif
	log_write_log_file("INFO: Create ADS1282 header OK\n");
}



/**
 * Изменим заголовок. Вызывается из файла ADS1282
 */
#pragma section("FLASH_code")
void log_fill_adc_header(char sps, u8 bitmap, u8 size)
{
	u32 block;

	/* Конфигурация и сколько самплов будет за 1 минуту */
	adc_hdr.ConfigWord = sps;
	switch (sps) {
	case SPS4K:
		block = SPS4K_PER_MIN;
		break;

	case SPS2K:
		block = SPS2K_PER_MIN;
		break;

	case SPS1K:
		block = SPS1K_PER_MIN;
		break;

	case SPS500:
		block = SPS500_PER_MIN;
		break;

	case SPS250:
		block = SPS250_PER_MIN;
		break;

	case SPS125:
		block = SPS125_PER_MIN;
		break;

	default:
		block = SPS62_PER_MIN;
		break;
	}

	adc_hdr.ChannelBitMap = bitmap;	/* Будет 1.2.3.4 канала */
	adc_hdr.SampleBytes = size;	/* Размер 1 сампла со всех работающих АЦП сразу */


#if defined		ENABLE_NEW_SIVY
	/* В новом Sivy размер блока  - 4 байт */
	adc_hdr.BlockSamples = block;
#else

	adc_hdr.BlockSamples = block & 0xffff;
	adc_hdr.params.coord.rsvd0 = (block >> 16) & 0xff;
	adc_hdr.Board = read_mod_id_from_eeprom();	/* Номер платы ставим свой */
	adc_hdr.Rev = 2;	/* Номер ревизии д.б. 2 */
#endif
}


/**
 * Для изменения заголовка. Подсчитать координаты и напряжения.
 * Заполняется раз в минуту
 */
#pragma section("FLASH_code")
void log_change_adc_header(void *p)
{
	DEV_STATUS_STRUCT *status;
	int temp;

	if (p != NULL) {
		status = (DEV_STATUS_STRUCT *) p;

#if defined		ENABLE_NEW_SIVY
		adc_hdr.u_pow = status->regpwr_volt;	/* Напряжение питания, U mv */
		adc_hdr.i_pow = status->ireg_sense;	/* Ток питания, U ma */
		adc_hdr.u_mod = status->am_power_volt;	/* Напряжение модема, U mv */
		adc_hdr.i_mod = status->iam_sense;	/* Ток модема, U ma */

		adc_hdr.t_reg = status->temper0;	/* Температура регистратора, десятые доли градуса */
		adc_hdr.t_sp = status->temper1;	/* Температура внешней платы, десятые доли градуса */
		adc_hdr.p_reg = status->press;	/* Давление внутри сферы */
		adc_hdr.pitch = status->pitch;
		adc_hdr.roll = status->roll;
		adc_hdr.head = status->head;

#else
		/* Пересчитываем по формулам DataFormatProposal */
		adc_hdr.Bat = (int) status->regpwr_volt * 1024 / 50000;	/* Будем считать что батарея 12 вольт */

		/* Если нету датчика */
		if (status->temper1 == 0 && status->press == 0) {
			temp = status->temper0;
		} else {
			temp = status->temper1;
		}
		adc_hdr.Temp = ((temp + 600) * 1024 / 5000);	/* Будем считать что температура * 10 */

#endif
	}
}


/**
 * Создавать заголовок каждый час, далее - в нем будет изменяться только время
 * Здесь же открыть файл для записи значений полученный с АЦП
 *
 * Профилировать эту функцию!!!!! 
 */
section("L1_code")
int log_create_hour_data_file(u64 ns)
{
	char name[32];		/* Имя файла */
	TIME_DATE date;
	FRESULT res;		/* Result code */
	u32 sec = ns / TIMER_NS_DIVIDER;

	/* Если уже есть окрытый файл - закроем его и создадим новый  */
	if (adc_file.obj.fs) {
		if ((res = f_close(&adc_file)) != 0) {
			return RES_CLOSE_DATA_ERR;
		}
	}

	/* Получили время в нашем формате - это нужно для названия */
	if (sec_to_td(sec, &date) != -1) {
		GNS110_PARAM_STRUCT gns110_param;
		get_gns110_start_params(&gns110_param);	/* Получили параметры  */


		/* Название файла по времени: год-месяц-день-часы.минуты */
		snprintf(name, MAX_FILE_NAME_LEN, "%s/%02d%02d%02d%02d.%02d",
			 &gns110_param.gns110_dir_name[0], ((date.year - 2000) > 0) ? (date.year - 2000) : 0,
			 date.month, date.day, date.hour, date.min);

		/* Откроем новый */
		res = f_open(&adc_file, name, FA_WRITE | FA_CREATE_ALWAYS);
		if (res) {
			return RES_OPEN_DATA_ERR;
		}
		return RES_NO_ERROR;	/* Все OK */
	} else {
		return RES_FORMAT_TIME_ERR;
	}
}


/**
 * Подготовить и сбросывать каждую минуту заголовок на SD карту и  делать F_SYNC!
 * В заголовке меняем только время 
 * Профилировать эту функцию!!!!! 
 */
section("L1_code")
int log_write_adc_header_to_file(u64 ns)
{
	TIME_DATE date;
	unsigned bw;		/* Прочитано или записано байт  */
	FRESULT res;		/* Result code */

#if defined		ENABLE_NEW_SIVY
	adc_hdr.SampleTime = ns;	/* Время сампла, наносекунды */
#else
	u64 msec = ns / TIMER_US_DIVIDER;
	memcpy(&adc_hdr.SedisTime, &msec, 8);	/* Пишем миллисекунды в резервированные места */
	sec_to_td(msec / TIMER_MS_DIVIDER, &date);	/* Получили секунды и время в нашем формате */
	memcpy(&adc_hdr.SampleTime, &date, sizeof(TIME_DATE));	/* Записали время начала записи данных */
#endif

	/* Скинем в файл заголовок */
	res = f_write(&adc_file, &adc_hdr, sizeof(ADC_HEADER), &bw);
	if (res) {
		return RES_WRITE_HEADER_ERR;
	}

	/* Обязательно запишем! не потеряем файл если выдернем SD карту */
	res = f_sync(&adc_file);
	if (res) {
		return RES_SYNC_HEADER_ERR;
	}

	return RES_NO_ERROR;
}

/**
 * Запись в файл данных АЦП: данные и размер в байтах
 * Sync делаем раз в минуту!
 * Профилировать эту функцию!!!!! 
 */
section("L1_code")
int log_write_adc_data_to_file(void *data, int len)
{
	unsigned bw;		/* Прочитано или записано байт  */
	FRESULT res;		/* Result code */

	res = f_write(&adc_file, (char *) data, len, &bw);
	if (res) {
		return RES_WRITE_DATA_ERR;
	}
	return RES_NO_ERROR;	/* Записали OK */
}

/**
 * Закрыть файл АЦП-перед этим сбросим буферы на диск 
 */
section("L1_code")
int log_close_data_file(void)
{
	FRESULT res;		/* Result code */

	if (adc_file.obj.fs == NULL)	// нет файла еще
		return RES_CLOSE_DATA_ERR;

	/* Обязательно запишем */
	res = f_sync(&adc_file);
	if (res) {
		return RES_CLOSE_DATA_ERR;
	}

	res = f_close(&adc_file);
	if (res) {
		return RES_CLOSE_DATA_ERR;
	}
	return FR_OK;		/* Все нормально! */
}


/***************************************************************************
 * Открыть файл регистрации-пока примитивный разбор,
 * в файле должны быть строки вида:
 ***************************************************************************************************
// Файл параметров для суточной записи Не редактировать!
19.03.13 15:10:00  			// Время начала регистрации в UTC
20.03.13 05:50:00  			// Время окончания регистрации в UTC
20.03.13 05:55:00  			// Время начала пережига в UTC
30   			// Длительность пережига проволоки в секундах
2   			// Расчетная длительность всплытия в минутах
-1   			// Номер модема, (0)-не меняю, (-1) не опрашиваю
20.03.13 15:11:00  			// Время аварийного всплытия от модема в UTC
06.00-18.00  			// Светлое время суток (часы-минуты)
500  			// Частота оцифровки АЦП
Hi  			// Энергопотребление  АЦП (High или Low)
2  			// PGA АЦП
3  			// Число байт в слове данных
1111  			// Включенные каналы ->(1-й, 2-й, 3-й, 4-й) слева направо
4  			// Размер файла данных (1 час, 4 часа или сутки)
 ***************************************************************************************************
 * параметр: указатель на даные
 * возврат:  успех (0) или нет (-1)
 * Возвращаем заполненную структуру наверх
 ****************************************************************************/
#pragma section("FLASH_code")
int log_read_reg_file(void *param)
{
	u32 sec;
	int j, ret = RES_NO_ERROR, x, y, m, d;	/* ret - результат выполнения */
	unsigned bw;		/* Прочитано или записано байт  */
	FIL reg_file;		/* File object для времени начала регистрации */
	FRESULT res;		/* Result code */
	char home_dir[8];	/* Название домашней директории */
	char buf[32];
	char *str = NULL;	/* Создается в куче */
	TIME_DATE time, dir_time;	/* название для директории */
	GNS110_PARAM_STRUCT *reg;	/* Параметры запуска  */


	do {
		/* Параметр не передали */
		if (param == NULL) {
			ret = RES_REG_PARAM_ERR;
			break;
		}

		reg = (GNS110_PARAM_STRUCT *) param;	/* Вот так хочу */



		/* Выделяем память для хранения файла */
		str = (char *) malloc(MAX_FILE_SIZE);
		if (str == NULL) {
			ret = RES_MALLOC_PARAM_ERR;
			break;
		}


		/* Открываем файл с параметрами */
		if (f_open(&reg_file, PARAM_FILE_NAME, FA_READ | FA_OPEN_EXISTING)) {
			ret = RES_OPEN_PARAM_ERR;
			break;
		}


		/* Читаем строки со всеми временами */
		if (f_read(&reg_file, str, MAX_FILE_SIZE - 1, &bw)) {
			ret = RES_READ_PARAM_ERR;
			break;
		}
		str[bw] = 0;

		/* Закроем файл */
		if (f_close(&reg_file)) {
			ret = RES_CLOSE_PARAM_ERR;
			break;
		}

///////////////////////////////////////////////////////////////////////////////////////////////
// 0...Читаем самую первую строку, она должна начинаться с "//"
///////////////////////////////////////////////////////////////////////////////////////////////

		/* Разбираем - найдем первый перевод строки */
		if (str[0] == '/' && str[1] == '/') {
			for (j = 2; j < 80; j++) {
				if (str[j] == '\n') {	// перевод строки DOS
					break;
				}
			}
		}
///////////////////////////////////////////////////////////////////////////////////////////////
// 1...Разбираем строку - Позиция установки
///////////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	// перевод строки DOS
				x++;
				if (x == POS_NUM_STR) {
					break;
				}
			}
		}


		/* 5 цифры на число */
		strncpy(buf, str + j + 1, 6);
		buf[5] = 0;
		reg->gns110_pos = atoi(buf);

///////////////////////////////////////////////////////////////////////////////////////////////
// 2...Разбираем строку времени начала регистрации после 1-го перевода
///////////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	// перевод строки DOS
				x++;
				if (x == BEGIN_REG_NUM_STR) {
					break;
				}
			}
		}

		buf[2] = 0;

		/* сначала 2 цифры на число */
		strncpy(buf, str + j + 1, 2);
		dir_time.day = atoi(buf);

		/* месяц */
		strncpy(buf, str + j + 4, 2);
		dir_time.mon = atoi(buf);

		/* год */
		strncpy(buf, str + j + 7, 2);
		dir_time.year = atoi(buf) + 2000;

		/* часы */
		strncpy(buf, str + j + 10, 2);
		dir_time.hour = atoi(buf);

		/* минуты */
		strncpy(buf, str + j + 13, 2);
		dir_time.min = atoi(buf);

		/* секунды */
		strncpy(buf, str + j + 16, 2);
		dir_time.sec = atoi(buf);

		/* Проверим на ошибки? */


		/* 1. перевести во время начала регистрации */
		reg->gns110_start_time = td_to_sec(&dir_time);

		/* Время начала подстройки (просыпания) будет ЗА 10 минут до этого  */
		reg->gns110_wakeup_time = (int) (reg->gns110_start_time) - TIME_START_AFTER_WAKEUP;


///////////////////////////////////////////////////////////////////////////////////////////////
// ищем 3-й перевод строки -  время окончания регистрации 
///////////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	// перевод строки DOS
				x++;
				if (x == END_REG_NUM_STR) {
					break;
				}
			}
		}

		/* 3...Разбираем строку времени окончания регистрации */
		buf[2] = 0;

		/* сначала 2 цифры на число */
		strncpy(buf, str + j + 1, 2);
		time.day = atoi(buf);

		/* месяц */
		strncpy(buf, str + j + 4, 2);
		time.mon = atoi(buf);

		/* год */
		strncpy(buf, str + j + 7, 2);
		time.year = atoi(buf) + 2000;

		/* часы */
		strncpy(buf, str + j + 10, 2);
		time.hour = atoi(buf);

		/* минуты */
		strncpy(buf, str + j + 13, 2);
		time.min = atoi(buf);

		/* секунды */
		strncpy(buf, str + j + 16, 2);
		time.sec = atoi(buf);


		/* 3. Время окончания регистрации */
		reg->gns110_finish_time = td_to_sec(&time);


//////////////////////////////////////////////////////////////////////////////////////////
//ищем 4-й перевод строки 
//////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == BEGIN_BURN_NUM_STR) {
					break;
				}
			}
		}

		/* 3... Разбираем строку времени всплытия (начало пережига) */
		buf[2] = 0;

		/* сначала 2 цифры на число */
		strncpy(buf, str + j + 1, 2);
		time.day = atoi(buf);

		/* месяц */
		strncpy(buf, str + j + 4, 2);
		time.mon = atoi(buf);

		/* год */
		strncpy(buf, str + j + 7, 2);
		time.year = atoi(buf) + 2000;

		/* часы */
		strncpy(buf, str + j + 10, 2);
		time.hour = atoi(buf);

		/* минуты */
		strncpy(buf, str + j + 13, 2);
		time.min = atoi(buf);

		/* секунды */
		strncpy(buf, str + j + 16, 2);
		time.sec = atoi(buf);

		/* 3. перевести в секунды времени начала пережига */
		reg->gns110_burn_on_time = td_to_sec(&time);

/////////////////////////////////////////////////////////////////////////////////////////
// ищем 5-й перевод строки длительность пережига
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == BURN_LEN_NUM_STR) {
					break;
				}
			}
		}

		strncpy(buf, str + j + 1, 4);	/* Длительность пережига - 3 цифры */
		reg->gns110_modem_burn_len_sec = atoi(buf);	/* Момент начала всплытия суммирует время пережига и +10 секунд до начала */
		reg->gns110_burn_off_time = reg->gns110_burn_on_time + reg->gns110_modem_burn_len_sec;

/////////////////////////////////////////////////////////////////////////////////////////
// ищем 6-й перевод строки длительность всплытия
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == POPUP_LEN_NUM_STR) {
					break;
				}
			}
		}

		strncpy(buf, str + j + 1, 4);	/* Длительность всплытия  в минутах - 3 цыфры */
		reg->gns110_gps_time = reg->gns110_burn_off_time + atoi(buf) * 60;	/* Время включения GPS = начало всплытия + длительность всплытия */


/////////////////////////////////////////////////////////////////////////////////////////
// ищем 7-й перевод строки. Номер акустического модема 
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == MODEM_NUM_NUM_STR) {
					break;
				}
			}
		}

		/* Номер акустического модема - 5 цыфр на это число! */
		strncpy(buf, str + j + 1, 6);
		reg->gns110_modem_num = atoi(buf);

/////////////////////////////////////////////////////////////////////////////////////////
// ищем 8-й перевод строки. Время аварийного всплытия от модема
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == ALARM_TIME_NUM_STR) {
					break;
				}
			}
		}

		/* Время аварийного всплытия от модема */

		/* 6... Разбираем строку времени всплытия */
		buf[2] = 0;

		/* сначала 2 цифры на число */
		strncpy(buf, str + j + 1, 2);
		time.day = atoi(buf);

		/* месяц */
		strncpy(buf, str + j + 4, 2);
		time.mon = atoi(buf);

		/* год */
		strncpy(buf, str + j + 7, 2);
		time.year = atoi(buf) + 2000;

		/* часы */
		strncpy(buf, str + j + 10, 2);
		time.hour = atoi(buf);

		/* минуты */
		strncpy(buf, str + j + 13, 2);
		time.min = atoi(buf);

		/* секунды */
		strncpy(buf, str + j + 16, 2);
		time.sec = atoi(buf);

		/* 6. перевести в секунды UTC */
		reg->gns110_modem_alarm_time = td_to_sec(&time);



/////////////////////////////////////////////////////////////////////////////////////////
// ищем 9-й перевод строки. Время светлого времени суток
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == DAY_TIME_NUM_STR) {
					break;
				}
			}
		}

		/* Время светлого времени суток */
		buf[2] = 0;

		/* сначала 2 цифры на час */
		strncpy(buf, str + j + 1, 2);
		reg->gns110_modem_h0_time = atoi(buf);

		/* 2 цифры на минуты */
		strncpy(buf, str + j + 4, 2);
		reg->gns110_modem_m0_time = atoi(buf);

		/* еще 2 цифры на час */
		strncpy(buf, str + j + 7, 2);
		reg->gns110_modem_h1_time = atoi(buf);

		/* 2 цифры на минуты */
		strncpy(buf, str + j + 10, 2);
		reg->gns110_modem_m1_time = atoi(buf);


/////////////////////////////////////////////////////////////////////////////////////////
// ищем 10-й перевод строки. Частота оцифровки.
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == ADC_FREQ_NUM_STR) {
					break;
				}
			}
		}
		/* Добавляем число 62 - это частота 62.5 */
		/* 4 цифры на число 62, 125, 250, 500, 1000, 2000, 4000 */
		strncpy(buf, str + j + 1, 5);
		reg->gns110_adc_freq = atoi(buf);

		// Ошибка в задании частоты
		if ((reg->gns110_adc_freq != 62) && (reg->gns110_adc_freq != 125) &&
		    (reg->gns110_adc_freq != 250) && (reg->gns110_adc_freq != 500) &&
		    (reg->gns110_adc_freq != 1000) && (reg->gns110_adc_freq != 2000) &&
		    (reg->gns110_adc_freq != 4000)) {
			ret = RES_FREQ_PARAM_ERR;
			break;
		}
/////////////////////////////////////////////////////////////////////////////////////////
// ищем 11-й перевод строки. Энергопотребление АЦП
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == ADC_CONSUM_NUM_STR) {
					break;
				}
			}
		}

		/* 3 символа hi или lo */
		strncpy(buf, str + j + 1, 3);

		/* В верхний регистр */
		str_to_cap(buf, 2);

		/* Что у нас записано? */
		if (strncmp(buf, "HI", 2) == 0) {
			reg->gns110_adc_consum = 1;
		} else if (strncmp(buf, "LO", 2) == 0) {
			reg->gns110_adc_consum = 0;
		} else {
			ret = RES_CONSUMP_PARAM_ERR;
			break;
		}


/////////////////////////////////////////////////////////////////////////////////////////
// ищем 12-й перевод строки. Усиление PGA
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == ADC_PGA_NUM_STR) {
					break;
				}
			}
		}

		/* 2 цифры на число PGA */
		strncpy(buf, str + j + 1, 3);
		reg->gns110_adc_pga = atoi(buf);

		// Ошибка в задании усиления
		if ((reg->gns110_adc_pga != 1) && (reg->gns110_adc_pga != 2) && (reg->gns110_adc_pga != 4)
		    && (reg->gns110_adc_pga != 8)
		    && (reg->gns110_adc_pga != 16) && (reg->gns110_adc_pga != 32) && (reg->gns110_adc_pga != 64)) {
			ret = RES_PGA_PARAM_ERR;
			break;
		}
/////////////////////////////////////////////////////////////////////////////////////////
// ищем 13-й перевод строки. Тип модема
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == MODEM_TYPE_NUM_STR) {
					break;
				}
			}
		}

		/* 2 цифры на число: 0, 1, 2, 3 */
		strncpy(buf, str + j + 1, 2);
		x = atoi(buf);
		if (x > 3 || x < 0) {
			ret = RES_MODEM_TYPE_ERR;
			break;
		}
		reg->gns110_modem_type = (GNS110_MODEM_TYPE) x;
/////////////////////////////////////////////////////////////////////////////////////////
// ищем 14-й перевод строки. Используемые каналы АЦП. Справа налево
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == ADC_BITMAP_NUM_STR) {
					break;
				}
			}
		}

		/* 4 цифры на число */
		reg->gns110_adc_bitmap = 0;
		strncpy(buf, str + j + 1, 5);

		// Смотрим ПО выключеию 
		if (buf[0] != '0')
			reg->gns110_adc_bitmap |= 8;
		if (buf[1] != '0')
			reg->gns110_adc_bitmap |= 4;
		if (buf[2] != '0')
			reg->gns110_adc_bitmap |= 2;
		if (buf[3] != '0')
			reg->gns110_adc_bitmap |= 1;


/////////////////////////////////////////////////////////////////////////////////////////
// ищем 15-й перевод строки. Сколько часов писать файл данных
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == FILE_LEN_NUM_STR) {
					break;
				}
			}
		}

		/* 2 цифры на число */
		strncpy(buf, str + j + 1, 3);
		j = atoi(buf);

		/* В этих пределах - от часа до суток */
		reg->gns110_file_len = (j > 0 && j < 25) ? j : 1;

/////////////////////////////////////////////////////////////////////////////////////////
// ищем 16-й перевод строки. Частота среза фильтра
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == FLT_FREQ_NUM_STR) {
					break;
				}
			}
		}

		/* 7 цифр на число и дес. точку */
		strncpy(buf, str + j + 1, 7);
		reg->gns110_adc_flt_freq = atof(buf);	/* В этих пределах - от 0 до ??? */


/////////////////////////////////////////////////////////////////////////////////////////
// ищем 17-й перевод строки. Постоянная регистрация
/////////////////////////////////////////////////////////////////////////////////////////
		x = 0;
		for (j = 0; j < bw - 1; j++) {
			if (str[j] == '\n') {	/* перевод строки (DOS или UNIX ?) */
				x++;
				if (x == CONST_REG_NUM_STR) {
					break;
				}
			}
		}

		/* 2 цыфры на число */
		strncpy(buf, str + j + 1, 2);
		reg->gns110_const_reg_flag = ((atoi(buf) == 0) ? false : true);	/* В этих пределах - от 0 до ??? */


/////////////////////////////////////////////////////////////////////////////////////////
// Прочитаем версию нашей платы с EEPROM
/////////////////////////////////////////////////////////////////////////////////////////
		x = read_mod_id_from_eeprom();
		sprintf(home_dir, "GNS%04d", x % 9999);	/* Сначала читаем директории, если она открывается - значит существует */

		/* Open the directory */
		if (f_opendir(&dir, home_dir)) {

			/* Не удалось создать! */
			if (f_mkdir(home_dir)) {
				ret = RES_MKDIR_PARAM_ERR;
				break;
			}
		}

/////////////////////////////////////////////////////////////////////////////////////////
// Создаем папку с названием времени запуска
/////////////////////////////////////////////////////////////////////////////////////////
		y = dir_time.year - 2000;
		m = dir_time.mon;
		d = dir_time.day;

		if (read_reset_cause_from_eeprom() == CAUSE_WDT_RESET) {	// Причина ресета - WDT
			/* Сначала читаем директории вниз, если она открывается - значит существует */
			for (x = MAX_START_NUMBER - 1; x >= 0; x--) {
				sprintf(reg->gns110_dir_name, "%s/%02d%02d%02d%02d",
					home_dir, (y > 0) ? y % MAX_START_NUMBER : 0, m % MAX_START_NUMBER,
					d % MAX_START_NUMBER, x % MAX_START_NUMBER);

				/* Open the directory - самую последнюю */
				res = f_opendir(&dir, reg->gns110_dir_name);
				if (res == FR_OK) {
					break;
				}
			}
		} else {

			/* Сначала читаем директории вниз с 99 до 0,
			 * если она открывается - значит существует */
			for (x = MAX_START_NUMBER - 1; x >= 0; x--) {
				sprintf(reg->gns110_dir_name, "%s/%02d%02d%02d%02d",
					home_dir, (y > 0) ? y % MAX_START_NUMBER : 0, m % MAX_START_NUMBER,
					d % MAX_START_NUMBER, x % MAX_START_NUMBER);

				/* Попытаться открыть */
				res = f_opendir(&dir, reg->gns110_dir_name);

				/* Если папка существует - наш номер следущий */
				if (res == FR_OK && x < (MAX_START_NUMBER - 1)) {
					x += 1;

					/* Пишем название следущей папки, которую откроем */
					sprintf(reg->gns110_dir_name, "%s/%02d%02d%02d%02d",
						home_dir, (y > 0) ? y % MAX_START_NUMBER : 0, m % MAX_START_NUMBER,
						d % MAX_START_NUMBER, x % MAX_START_NUMBER);
					break;
				}
			}

			/* Такое возможо никогда не наступит, но на всякий случай */
			if (x > (MAX_START_NUMBER - 1)) {
				ret = RES_MAX_RUN_ERR;
				break;
			}

			/* Создать папку с названием: число и номер запуска */
			if ((res = f_mkdir(reg->gns110_dir_name))) {
				ret = RES_DIR_ALREADY_EXIST;
				break;
			}
		}


		/* Открыть файл лога в этой директории всегда, создать если нет!  */
		sprintf(str, "%s/gns110.log", reg->gns110_dir_name);
		if ((res = f_open(&log_file, str, FA_WRITE | FA_READ | FA_OPEN_ALWAYS))) {
			ret = RES_CREATE_LOG_ERR;
			break;
		}

		/* Определим размер     */
		x = f_size(&log_file);
		if (x >= MAX_LOG_FILE_LEN)
			f_truncate(&log_file);

		/* Переставим указатель файла */
		f_lseek(&log_file, x);


		/*vvvvv: добавить и файл модема  */

		/* Здесь же откроем файл, куда будем скидывать поминутные параметры среды  */
		sprintf(str, "%s/gns110.env", reg->gns110_dir_name);
		if ((res = f_open(&env_file, str, FA_WRITE | FA_READ | FA_OPEN_ALWAYS))) {
			ret = RES_CREATE_ENV_ERR;
			break;
		}

		/* Определим размер */
		x = f_size(&env_file);
		if (x == 0) {
			/* Запишем названия столбцов */
			strcpy(str,
			       "Time\tREGpwr\tREGcur\tBURNext\tBURNpwr\tBURNcur\tMODEMpwr\tMODEMcur\tTemp0\tTemp1\tPress\tPitch\tRoll\tHead\n");
			res = f_write(&env_file, str, strlen(str), &bw);
			if (res) {
				return RES_WRITE_LOG_ERR;
			}
		} else {
			/* Переставим указатель файла */
			f_lseek(&env_file, x);
		}

	} while (0);

	/* Удаляем выделенный буфер */
	if (str != NULL) {
		free(str);
		str = NULL;
	}

	return ret;		/* Все получили - успех или неудача? */
}


/* Выводить свободное место на разделе */
#pragma section("FLASH_code")
void log_get_free_space(void)
{
    FRESULT res;		/* Result code */
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *fs = &fatfs;

/* Get volume information and free clusters of drive 1 */
	res = f_getfree("1:", &fre_clust, &fs);
	if (res) {
		/* Get total sectors and free sectors */
		tot_sect = (fs->n_fatent - 2) * fs->csize;
		fre_sect = fre_clust * fs->csize;

		/* Print the free space (assuming 512 bytes/sector) */
		log_write_log_file("%10lu KiB total drive space.\n%10lu KiB available.\n", tot_sect / 2, fre_sect / 2);
	} else {
		log_write_log_file("can't get free space\r\n");
	}
}
