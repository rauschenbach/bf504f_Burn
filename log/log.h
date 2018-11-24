#ifndef _LOG_H
#define _LOG_H

#include "globdefs.h"

#define 	SPS4K_PER_MIN	(4000 * 60)
#define 	SPS2K_PER_MIN	(2000 * 60)
#define 	SPS1K_PER_MIN	(1000 * 60)
#define	 	SPS500_PER_MIN	(500 * 60)
#define 	SPS250_PER_MIN	(250 * 60)
#define 	SPS125_PER_MIN	(125 * 60)
#define 	SPS62_PER_MIN	(125 * 30)




/** 
 * Здесь ощибки, которые могут быть возвражены функцией
 */
typedef enum {
	// не ошибки
	RES_NO_ERROR = 0,	// Нет ошибки
	RES_NO_LOCK_FILE,	// Нет лок файла

	RES_WRITE_UART_ERR = -4,	// Ошибка записи в порт
	RES_DEL_LOCK_ERR = -5,	// Ошибка удаления лок файла
	RES_MOUNT_ERR = -6,	// Ошибка монтирования  
	RES_FORMAT_ERR = -7,	// Ошибка формирования строки
	RES_WRITE_LOG_ERR = -8,	// Ошибка записи в лог
	RES_SYNC_LOG_ERR = -9,	// Ошибка записи в лог (sync)
	RES_CLOSE_LOG_ERR = -10,	// Ошибка закрытия лога
	RES_OPEN_DATA_ERR = -11,	// Ошибка открытия файла данных
	RES_WRITE_DATA_ERR = -12,	// Ошибка записи файла данных
	RES_CLOSE_DATA_ERR = -13,	// Ошибка закрытия файла данных
	RES_WRITE_HEADER_ERR = -14,	// Ошибка записи минутного заголовка
	RES_SYNC_HEADER_ERR = -15,	// Ошибка записи заголовка (sync)
	RES_REG_PARAM_ERR = -16,	// Ошибка в файле параметров
	RES_MALLOC_PARAM_ERR = -17,	// Ошибка выделения памяти
	RES_OPEN_PARAM_ERR = -18,	// Ошибка открытия файла параметров
	RES_READ_PARAM_ERR = -19,	// Ошибка чтения файла параметров
	RES_CLOSE_PARAM_ERR = -20,	// Ошибка закрытия файла параметров
	RES_TIME_PARAM_ERR = -21,	// Ошибка в задании времени
	RES_FREQ_PARAM_ERR = -22,	// Ошибка в задании частоты
	RES_CONSUMP_PARAM_ERR = -23,	// Ошибка в задании енергопотребления
	RES_PGA_PARAM_ERR = -24,	// Ошибка в задании усиления
	RES_MODEM_TYPE_ERR = -25,	// Ошибка в задании числа байт
	RES_MKDIR_PARAM_ERR = -26,	// Ошибка в создании папки
	RES_MAX_RUN_ERR = -27,	// Исчерпаны запуски
	RES_DIR_ALREADY_EXIST = -28,
	RES_CREATE_LOG_ERR = -29,	// Ошибка создания лога
	RES_CREATE_ENV_ERR = -30,	// Ошибка создания лога среды

	RES_READ_FLASH_ERR = -31,	// Ошибка чтения flash
	RES_FORMAT_TIME_ERR = -40,	// Ошибка форматирования времени
} ERROR_ResultEn;



/*******************************************************************
*  function prototypes
*******************************************************************/
int log_mount_fs(void);
void log_get_free_space(void);
int log_read_reg_file(void *);
int log_write_error_file(char *fmt, ...);
int log_check_reg_file(void);
int log_write_debug_str(char *, ...);
int log_check_lock_file(void);
int log_write_log_to_uart(char *, ...);
bool log_check_mounted(void);
int log_write_log_file(char *, ...);
void log_create_adc_header(s64, s64, s32, s32);
void log_fill_adc_header(char, u8, u8);
void log_change_adc_header(void *);
int log_write_env_data_to_file(void *);
int log_create_hour_data_file(u64);
int log_write_adc_header_to_file(u64);
int log_write_adc_data_to_file(void *, int);
int log_close_data_file(void);
int log_close_log_file(void);


#endif				/* log.h */
