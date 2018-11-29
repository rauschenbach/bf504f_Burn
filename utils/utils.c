/******************************************************************************
 * Функции перевода дат, проверка контрольной суммы т.ж. здесь 
 * Все функции считают время от начала Эпохи (01-01-1970)
 * Все функции с маленькой буквы
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "main.h"
#include "timer1.h"
#include "timer2.h"
#include "timer4.h"
#include "utils.h"
#include "eeprom.h"
#include "timer1.h"
#include "timer2.h"
#include "ads1282.h"
#include "lsm303.h"
#include "math.h"
#include "dac.h"
#include "log.h"
#include "rsi.h"


section("FLASH_data")
static const char *monthes[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

#define 	M_PI				3.141592653589
#define		DAC_TUNE_COEF		 	94

static f32 AccSensorNorm(f32 *);

/******************************************************************************* 
 * Переводит текущее время и год в секунды с начала Эпохи
 * int t_year,t_mon,t_day; - текущая дата
 * int t_hour,t_min,t_sec; - текущее время
 * Можно запускать из FLASH - не критично!
 *******************************************************************************/
#pragma section("FLASH_code")
long td_to_sec(TIME_DATE * t)
{
    long r;
    struct tm tm_time;

    tm_time.tm_sec = t->sec;
    tm_time.tm_min = t->min;
    tm_time.tm_hour = t->hour;
    tm_time.tm_mday = t->day;
    tm_time.tm_mon = t->mon - 1;	/* В TIME_DATE месяцы с 1 по 12, в tm_time с 0 по 11 */
    tm_time.tm_year = t->year - 1900;	/* В tm годы считаются с 1900 - го */
    tm_time.tm_wday = 0;	/* Не используются */
    tm_time.tm_yday = 0;
    tm_time.tm_isdst = 0;

    /* Делаем секунды */
    r = mktime(&tm_time);
    return r;			/* -1 или нормальное число  */
}


/***********************************************************************************
 * Выдать тики в минутах
 * Можно запускать из FLASH - не критично!
 ***********************************************************************************/
#pragma section("FLASH_code")
long get_min_ticks(void)
{
    return (long) ((((u64) get_long_time() / TIMER_NS_DIVIDER) + 1) / 60);	/* Не пересекаться!!!  */
}




/***********************************************************************************  
 * Получить строку с временем
 * Можно запускать из FLASH - не критично!
 **********************************************************************************/
#pragma section("FLASH_code")
void sec_to_str(u32 ls, char *str)
{
    TIME_DATE t;

    /* Записываем дату в формате: 08-11-12 - 08:57:22 */
    if (sec_to_td(ls, &t) != -1) {
	sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d", t.day, t.mon, t.year, t.hour, t.min, t.sec);
    } else {
	sprintf(str, "[set time error]");
    }
}



/***********************************************************************************  
 * Записываем дату в формате: 08-11-12 - 08:57:22
 **********************************************************************************/
#pragma section("FLASH_code")
void td_to_str(TIME_DATE * t, char *str)
{
    sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d", t->day, t->mon, t->year, t->hour, t->min, t->sec);
}


/***********************************************************************************
 * Установить время на обоих таймерах
 * Можно запускать из FLASH - не критично!
 ***********************************************************************************/
#pragma section("FLASH_code")
void set_sec_ticks(long sec)
{
    TIMER1_set_sec(sec);
    TIMER2_set_sec(sec);
}


/***********************************************************************************  
 * Наносекунды в строку
 **********************************************************************************/
#pragma section("FLASH_code")
void nsec_to_str(u64 ls, char *str)
{
    u32 sec, nsec;
    TIME_DATE t;

    sec = (u64) ls / TIMER_NS_DIVIDER;
    nsec = (u64) ls % TIMER_NS_DIVIDER;

    /* Записываем дату в формате: 08-11-12 - 08:57:22 */
    if (sec_to_td(sec, &t) != -1) {
	sprintf(str, "%02d-%02d-%04d - %02d:%02d:%02d.%09d", t.day, t.mon, t.year, t.hour, t.min, t.sec, nsec);
    } else {
	sprintf(str, "[set time error] ");
    }
}


/***********************************************************************************
 * Какой у процессора ENDIAN: 1 - Big-endian, 0 - Little-endian. В проверке наоборот 
 ***********************************************************************************/
#pragma section("FLASH_code")
int get_cpu_endian(void)
{
    int i = 1;
    if (*((u8 *) & i) == 0)	/* Big-endian */
	return 1;
    else
	return 0;		/* Little-endian */
}


/**
 * Получить сумму значений, смотреть что бы не вышло за разрядную сетку
 */
#pragma section("FLASH_code")
u32 get_buf_sum(u32 * buf, int size)
{
    int i;
    u32 sum = 0;

    if (buf != NULL & size != 0) {

	for (i = 0; i < size; i++)
	    sum += buf[i];
    }
    return sum;
}


/**
 * Получить среднюю величину буфера
 */
#pragma section("FLASH_code")
u32 get_buf_average(u32 * buf, int size)
{
    int i;
    u32 sum = 0;

    if (buf != NULL & size != 0) {
	for (i = 0; i < size; i++)
	    sum += buf[i];
	sum /= size;
    }
    return sum;
}


/**
 * Получить управляющее значение для подачи на ДАК генератора
 * По формуле прямой через 2 точки
 * для 16-ти секунд
 */
#pragma section("FLASH_code")
u16 get_dac_ctrl_value(u32 sum, u16 old_dac, u16 delta)
{
    long err;
    int new_dac;
    int coef = (delta < 10 || delta > 250) ? 98 : delta;

    err = sum - TIM4_TICKS_FOR_16_SEC;

    // Физ. смысл:    new_dac = (-err) * 16384 / DAC_TUNE_COEF / 16 + old_dac;
    new_dac = (-err) * 1024 / coef + old_dac;

    /* против переполнения, ЦАП работает на пределе разрядной величины!!! */
    if (new_dac > DAC19_MAX_DATA)
	new_dac = DAC19_MAX_DATA;
    if (new_dac < DAC19_MIN_DATA)
	new_dac = DAC19_MIN_DATA;

    return (u16) new_dac;
}


/**
 * Получить управляющее значение для подачи на ДАК генератора
 * По формуле прямой через 2 точки
 */
#pragma section("FLASH_code")
u16 get_dac_ctrl_value_old(u32 * buf, int size, u16 old_dac, u16 delta)
{
    long err;
    int new_dac;
    int coef = (delta < 10 || delta > 250) ? 98 : delta;

    err = get_buf_sum(buf, size) - SCLK_VALUE * size;

    // Физ. смысл:    new_dac = (-err) * 16384 / DAC_TUNE_COEF / 16 + old_dac;
    new_dac = (-err) * 1024 / coef + old_dac;

    /* против переполнения, ЦАП работает на пределе разрядной величины!!! */
    if (new_dac > DAC19_MAX_DATA)
	new_dac = DAC19_MAX_DATA;
    if (new_dac < DAC19_MIN_DATA)
	new_dac = DAC19_MIN_DATA;

    return (u16) new_dac;
}



/* Строку в верхний регистр   */
#pragma section("FLASH_code")
void str_to_cap(char *str, int len)
{
    int i = len;

    while (i--)
	str[i] = (str[i] > 0x60) ? str[i] - 0x20 : str[i];
}


//Mar 14 2013 13:53:31
#pragma section("FLASH_code")
bool parse_date_time(char *str_date, char *str_time, TIME_DATE * time)
{
    char buf[5];
    int i, len, x;

    len = strlen(str_date);
    str_to_cap(str_date, len);	// в верхний регистр

    for (i = 0; i < len; i++)
	if (isalpha(str_date[i]))	// найдем первую букву
	    break;

    if (i >= len)
	return false;


    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 3);	// 3 символа скопировали
    for (i = 0; i < 12; i++) {
	if (strncmp(buf, monthes[i], 3) == 0) {
	    time->mon = i + 1;	// месяц нашли
	    break;
	}
    }
    if (time->mon > 12)
	return false;


    // ищем первую цыфру
    for (i = 0; i < len; i++)
	if (isdigit(str_date[i])) {
	    x = i;
	    break;
	}

    memset(buf, 0, 5);
    strncpy(buf, str_date + i, 2);	// 2 символа
    time->day = atoi(buf);

    if (time->day > 31)
	return false;

    // ищем год, пробел после цыфр
    for (i = x; i < len; i++) {
	if (str_date[i] == 0x20) {
	    break;
	}
    }
    strncpy(buf, str_date + i + 1, 4);	// 4 символа
    time->year = atoi(buf);

    if (time->year < 2013)
	return false;


    /* 3... Разбираем строку времени */
    memset(buf, 0, 5);

    /* часы */
    strncpy(buf, str_time, 2);
    time->hour = atoi(buf);

    if (time->hour > 24)
	return false;


    /* минуты */
    strncpy(buf, str_time + 3, 2);
    time->min = atoi(buf);
    if (time->min > 60)
	return false;


    /* секунды */
    strncpy(buf, str_time + 6, 2);
    time->sec = atoi(buf);
    if (time->sec > 60)
	return false;

    return true;
}


/**
 * Сместить все времена, если время старта вышло или пережиг
 */
#pragma section("FLASH_code")
int check_start_time(void *par)
{
    char str[32];
    GNS110_PARAM_STRUCT *time;
    long t0;
    int res = -1;

    do {
	if (par == NULL) {
	    break;
	}
	time = (GNS110_PARAM_STRUCT *) par;	/* Получили параметры  */
	t0 = get_sec_ticks();

	/* Должно быть неск. секунд в запасе - округляем на целую минуту   */
	if (time->gns110_start_time <= (t0 + WAIT_TIM3_SYNC)) {
	    time->gns110_start_time = t0 - (t0 % 60) + WAIT_PC_TIME;	/* на 2 минуты позже "сейчас" */
 	    sec_to_str(t0, str);
            log_write_log_file("INFO: start time was shifted on %s\n", str);
	}

	/* Если время окончания получилось */
	if (time->gns110_finish_time <= (int) time->gns110_start_time) {
	    log_write_log_file("ERROR: start time can't be more finish time\n");
	    break;		/*  ошибка */
	}
	res = 0;
    } while (0);
    return res;
}

/**
 * Печатать данные среды
 */
#pragma section("FLASH_code")
void print_adc_data(void *par)
{
    log_write_env_data_to_file(par);
}


/**
 * Печатать статус
 */
#pragma section("FLASH_code")
void print_status(void *par)
{
    ADS1282_Regs regs;
    DEV_DAC_STRUCT dac;
    DEV_STATUS_STRUCT *status;
    char str[32];

    if (par == NULL)
	return;

    status = (DEV_STATUS_STRUCT *) par;


    log_write_log_file(">>>>>>>>>>>>>>>Status and Tests<<<<<<<<<<<<<<<\n");
    log_write_log_file("INFO: %s\n", status->st_main & 0x01 ? "No time in RTC" : "Time OK");
/*    log_write_log_file("INFO: %s\n", status->st_main & 0x02 ? "No const in EEPROM" : "Const OK"); */
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x01 ? "RTC error" : "RTC OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x02 ? "T & P error" : "T & P OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x04 ? "Accel & comp error" : "Accel & comp  OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x08 ? "Modem error" : "Modem OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x10 ? "GPS error" : "GPS OK");
    log_write_log_file("INFO: %s\n", status->st_test0 & 0x20 ? "EEPROM error" : "EEPROM OK");


#if QUARTZ_CLK_FREQ==(19200000)
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x10 ? "Quartz 19.2  MHz error" : "Quartz 19.2  MHz OK");
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x20 ? "Quartz 4.096 MHz error" : "Quartz 4.096 MHz OK");
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x80 ? "TIMER3 error" : "TIMER3  OK");
#else
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x10 ? "Quartz 8.192  MHz error" : "Quartz 8.192  MHz OK");
#endif
    log_write_log_file("INFO: %s\n", status->st_test1 & 0x40 ? "TIMER4 error" : "TIMER4 OK");



    read_dac_coefs_from_eeprom(&dac);
    read_ads1282_coefs_from_eeprom(&regs);
    log_write_log_file(">>>>>>>>>>>EEPROM status: %08X <<<<<<<<<<<<<<<\n", status->eeprom);
    log_write_log_file("INFO: EEPROM_MOD_ID:\t\t%d\n", read_mod_id_from_eeprom());
    log_write_log_file("INFO: EEPROM_RSVD0:\t\t%d\n", read_rsvd0_from_eeprom());
    log_write_log_file("INFO: EEPROM_RSVD1:\t\t%d\n", read_rsvd1_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_WORK:\t%d\n", read_time_work_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_CMD:\t%d\n", read_time_cmd_from_eeprom());
    log_write_log_file("INFO: EEPROM_TIME_MODEM:\t%d\n", read_time_modem_from_eeprom());
    log_write_log_file("INFO: EEPROM_DAC19_COEF:\t%d\n", dac.dac19_data);
    log_write_log_file("INFO: EEPROM_DAC4_COEF:\t%d\n", dac.dac4_data);
    log_write_log_file("INFO: EEPROM_RSVD1:\t\t%d\n", read_rsvd2_from_eeprom());
    log_write_log_file("INFO: EEPROM_ADC_OFS0:\t0x%08X\n", regs.chan[0].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC0:\t0x%08X\n", regs.chan[0].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS1:\t0x%08X\n", regs.chan[1].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC1:\t0x%08X\n", regs.chan[1].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS2:\t0x%08X\n", regs.chan[2].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC2:\t0x%08X\n", regs.chan[2].gain);
    log_write_log_file("INFO: EEPROM_ADC_OFS3:\t0x%08X\n", regs.chan[3].offset);
    log_write_log_file("INFO: EEPROM_ADC_FSC3:\t0x%08X\n", regs.chan[3].gain);

    // От чего произошол предыдущий reset
    if (status->st_reset == 1) {
	log_write_log_file("INFO: Last reset was:\tPOWER OFF\n");
    } else if (status->st_reset == 2) {
	log_write_log_file("INFO: Last reset was:\tEXT. RESET\n");
    } else if (status->st_reset == 4) {
	log_write_log_file("INFO: Last reset was:\tBURN OUT\n");
    } else if (status->st_reset == 8) {
	log_write_log_file("INFO: Last reset was:\tWDT RESET\n");
    } else if (status->st_reset == 16) {
	log_write_log_file("INFO: Last reset was:\tNO LINK POWER OFF\n");
    } else {
	log_write_log_file("INFO: Last reset was:\tUNKNOWN RESET\n");
    }
}

/**
 * Вывести параметры АЦП
 */
#pragma section("FLASH_code")
void print_ads1282_parms(void *v)
{
    GNS110_PARAM_STRUCT *gns110_param;
    u8 map;

    gns110_param = (GNS110_PARAM_STRUCT *) v;	/* Получили параметры  */


    log_write_log_file(">>>>>>>>>>ADS1282 parameters<<<<<<<<<\n");
    log_write_log_file("INFO: Sampling freq:  %####d\n", gns110_param->gns110_adc_freq);
    log_write_log_file("INFO: Mode:           %s\n", gns110_param->gns110_adc_consum == 1 ? "High res mode" : "Low power mode");
    log_write_log_file("INFO: PGA:            %####d\n", gns110_param->gns110_adc_pga);
    log_write_log_file("INFO: HPF freq:       %3.4f Hz\n", gns110_param->gns110_adc_flt_freq);


    /* Глюк компилятора по-видимому или мой */
    if (gns110_param->gns110_adc_bitmap == 0) {
	log_write_log_file("ERROR: bitmap param's invalid\n");
	log_write_log_file("INFO: All channels will be turned on\n");
	gns110_param->gns110_adc_bitmap = 0x0f;	///// была ошибка ==
    }

    log_write_log_file("INFO: Use channels:   4: %s, 3: %s, 2: %s, 1: %s\n",
		       (gns110_param->gns110_adc_bitmap & 0x08) ? "On" : "Off",
		       (gns110_param->gns110_adc_bitmap & 0x04) ? "On" : "Off",
		       (gns110_param->gns110_adc_bitmap & 0x02) ? "On" : "Off", (gns110_param->gns110_adc_bitmap & 0x01) ? "On" : "Off");
    log_write_log_file("INFO: File length %d hour(s)\n", gns110_param->gns110_file_len);
}

/**
 * Вывести все заданные времена
 */
#pragma section("FLASH_code")
void print_set_times(void *par)
{
    char str[32];
    long t;
    GNS110_PARAM_STRUCT *time;

    if (par != NULL) {

	time = (GNS110_PARAM_STRUCT *) par;	/* Получили параметры  */

	log_write_log_file(">>>>>>>>>>>>>>>>  Times  <<<<<<<<<<<<<<<<<\n");
	t = get_sec_ticks();
	sec_to_str(t, str);
	log_write_log_file("INFO: now time is:\t\t%s\n", str);

	/* не показывать 1970-й год */
	if (time->gns110_start_time > 1) {
	    sec_to_str(time->gns110_start_time, str);
	    log_write_log_file("INFO: registration starts at:\t%s\n", str);
	}

	/* Не показывать 1970-й год */
	if (time->gns110_finish_time > 1) {
	    sec_to_str(time->gns110_finish_time, str);
	    log_write_log_file("INFO: registration finish at:\t%s\n", str);
	}

	/* не показывать 1970-й год */
	if (time->gns110_burn_on_time > 1) {
	    sec_to_str(time->gns110_burn_on_time, str);
	    log_write_log_file("INFO: burn wire on at:\t%s\n", str);
	}
	/* не показывать 1970-й год */
	if (time->gns110_burn_off_time > 1) {
	    sec_to_str(time->gns110_burn_off_time, str);
	    log_write_log_file("INFO: burn wire off time at:\t%s\n", str);
	}
	sec_to_str(time->gns110_gps_time, str);
	log_write_log_file("INFO: turn on GPS at:\t\t%s\n", str);
    } else {
	log_write_log_file("ERROR: print times");
    }
}

/**
 * Вывести дрифт в часах минутах и секундах и прочих более мелких долях
 * Время работы и уход таймера
 * Дрифт  = Т1 - T4
 */
#pragma section("FLASH_code")
void print_drift_and_work_time(s64 tim1, s64 tim4, s32 rtc, u32 wt)
{
    char str[64];
    TIME_DATE td;
    u32 sec, nsec;
    s64 drift;
    u8 sign = 0;

    /* Печатаем время T4 */
    nsec_to_str(tim4, str);
    log_write_log_file("INFO: GPS time:  %s\n", str);


    if (tim1 != 0) {
	// Печатаем время T1
	nsec_to_str(tim1, str);
	log_write_log_file("INFO: PRTC time: %s\n", str);

	// Печатаем время RTC
	sec_to_str(rtc, str);
	log_write_log_file("INFO: RTC  time: %s\n", str);

	// И сам дрифт
	drift = tim1 - tim4;
	if (drift < 0) {
	    sign = 1;		// если отрицательное число
	    drift = 0 - drift;
	}
	// Если меньше 1-й секунды
	sec = (u64) drift / TIMER_NS_DIVIDER;

	if (abs(sec) < 1) {
	    drift = TIMER1_get_drift(NULL, NULL);
	    drift = abs(drift);
	}

	nsec = (u64) drift % TIMER_NS_DIVIDER;
	sec_to_td(sec, &td);
	if (wt > 0)
	    log_write_log_file("INFO: Drift final full: %c%02d days %02d:%02d:%02d.%09d\n", sign ? '-' : ' ', td.hour / 24, td.hour % 24, td.min, td.sec, nsec);
	else
	    log_write_log_file("INFO: Drift begin full: %c%02d days %02d:%02d:%02d.%09d\n", sign ? '-' : ' ', td.hour / 24, td.hour % 24, td.min, td.sec, nsec);
    }

    if (wt > 0) {
	float div = (float) drift / wt;
	log_write_log_file("INFO: PRTC timer walks on %4.5f ns per sec\n", div);
	log_write_log_file("INFO: GNS worked %d sec\n", wt);
    }
}



#pragma section("FLASH_code")
void print_reset_cause(u32 cause)
{
    switch (cause) {

    case CAUSE_POWER_OFF:
	log_write_log_file("INFO: Disconnection reason - Power OFF\n");
	break;

    case CAUSE_EXT_RESET:
	log_write_log_file("INFO: Disconnection reason - Extern. reset\n");
	break;

    case CAUSE_BROWN_OUT:
	log_write_log_file("INFO: Disconnection reason - Brown out\n");
	break;

    case CAUSE_WDT_RESET:
	log_write_log_file("INFO: Disconnection reason - WDT reset\n");
	break;

    case CAUSE_NO_LINK:
	log_write_log_file("INFO: Disconnection reason - No link (> 10 min)\n");
	break;

    case CAUSE_UNKNOWN_RESET:
    default:
	log_write_log_file("INFO: Disconnection reason - Unknown reset\n");
	break;
    }
}


/**
 * Печатаем ошибки если они есть
 */
#pragma section("FLASH_code")
void print_timer_and_sd_card_error(void)
{
    long sa, bs, err;
    SD_CARD_ERROR_STRUCT ts;
    ADS1282_ERROR_STRUCT ader;

    /* Таймер считывали в неподходящее время  */
    err = TIMER1_get_error();
    log_write_log_file("INFO: %d timer1 warning(s) were found for work time\n", err);

    /* Ошибки АЦП - запись на карту  */
    ADS1282_get_error_count(&ader);
    log_write_log_file("INFO: %d ADC miss(es) were found\n", ader.sample_miss);	/* отсчет пришел не вовремя */
    log_write_log_file("INFO: %d write error(s) were found on SD card\n", ader.block_timeout);	/* Блок не успел вовремя записаться */

    /* Ошибки sd карты */
    rsi_get_card_timeout(&ts);
    log_write_log_file("INFO: %d command error(s) occured on SD Card\n", ts.cmd_error);
    log_write_log_file("INFO: %d read timeout(s) occured on SD Card\n", ts.read_timeout);
    log_write_log_file("INFO: %d write timeout(s) occured on SD Card\n", ts.write_timeout);
    log_write_log_file("INFO: %d other error(s) occured on SD Card\n", ts.any_error);

    /* Для EEPROM на Flash */
    eeprom_get_status(&err, &sa, &bs);
    log_write_log_file("INFO: EEPROM was formated %d time(s)\n", err);
    log_write_log_file("INFO: PAGE0 was erased %d, PAGE1 was erased %d time(s)\n", sa, bs);
}


/**
 * Таймаут карты SD
 */
#pragma section("FLASH_code")
void get_sd_card_timeout(SD_CARD_ERROR_STRUCT * ts)
{
    if (ts != NULL)
	rsi_get_card_timeout(ts);
}



/**
 * Проверить допустимость заданных времен
 * Если постоянная регистрация - не смотрим время финиша
 */
#pragma section("FLASH_code")
int check_set_times(void *par)
{
    char str0[32], str1[32];
    long t;
    GNS110_PARAM_STRUCT *time;


    if (par == NULL)
	return -1;

    time = (GNS110_PARAM_STRUCT *) par;	/* Получили параметры  */

    t = get_sec_ticks();

    log_write_log_file("INFO: Checking times...\n");

#if 1
    /* Если время "сейчас" меньше времени старта на день - вероятно часы сбиты! */
    if (abs((int) time->gns110_start_time - t) >= TIME_ONE_DAY) {
	log_write_log_file("ERROR: Time now can't be less start time on one day! Check RTC\n");
	return -1;
    }
#endif
    if (read_reset_cause_from_eeprom() == CAUSE_WDT_RESET) {
	if ((int) time->gns110_finish_time - t < TIME_START_AFTER_WAKEUP) {
	    log_write_log_file("ERROR: It hasn't time for adjust in WDT reset!\n");
	    return -1;
	} else {
	    log_write_log_file("INFO: Finish time set OK\n");
	}
    } else {
	if ((int) time->gns110_finish_time - (int) time->gns110_start_time < RECORD_TIME) {
	    log_write_log_file("ERROR: Record time less %d seconds!\n", RECORD_TIME);
	    return -1;
	} else {
	    log_write_log_file("INFO: Finish time set OK\n");
	}

	/* Должно быть минимум 7 минут на подстройку и регистрацию  */
	if (((int) time->gns110_finish_time - t) < (RECORD_TIME + TIME_START_AFTER_WAKEUP)) {
	    sec_to_str(time->gns110_finish_time, str1);
	    log_write_log_file("INFO: Finish time: %s\n", str1);
	    log_write_log_file("INFO: Please correct finish time!\n", str1);
	    log_write_log_file("ERROR: Not enough time for tuning or time is over!\n");
	    return -1;
	} else {
	    sec_to_str(t, str0);
	    sec_to_str(time->gns110_finish_time, str1);
	    log_write_log_file("INFO: Time set OK\n");
	    log_write_log_file("INFO: Now (%s)\n", str0);
	    log_write_log_file("INFO: Fin (%s)\n", str1);
	}

	if ((int) time->gns110_burn_on_time <= (int) time->gns110_finish_time) {
	    log_write_log_file("ERROR: Burn-relay turn on time less finish time!\n");
	    return -1;
	} else {
	    log_write_log_file("INFO: Burn time set OK\n");
	}

	/* Время пережига от модема и свое не должно перекрываться */
	if (abs((int) time->gns110_burn_on_time - (int) time->gns110_modem_alarm_time) < (RELEBURN_TIME * 2)) {
	    log_write_log_file("ERROR: Burn-relay time and modem alarm time can't overlap!\n");
	} else {
	    log_write_log_file("INFO: Burn-relay time and modem alarm set OK. Times don't overlap!\n");
	}

	if ((int) time->gns110_burn_off_time - (int) time->gns110_burn_on_time < RELEBURN_TIME) {
	    log_write_log_file("ERROR: Burnout time less %d seconds!\n", RELEBURN_TIME);
	    return -1;
	} else {
	    log_write_log_file("INFO: burn wire off set OK\n");
	}

	if ((int) time->gns110_gps_time - (int) time->gns110_burn_off_time < POPUP_DURATION) {	// к 1 минуте!
	    log_write_log_file("ERROR: Turn on GPS time need to be %d minutes later burn wire off time!\n", POPUP_DURATION);
	    return -1;
	} else {
	    log_write_log_file("INFO: Turn on GPS time set OK\n");
	}
    }

    return 0;
}


/**
 * Проверить как установили время для модема
 */
#pragma section("FLASH_code")
int check_modem_times(void *v)
{
    int res = 0;
    char str[128];
    GNS110_PARAM_STRUCT *par;

    if (v == NULL)
	return -1;

    par = (GNS110_PARAM_STRUCT *) v;

    if (par->gns110_modem_type > 0) {
	long t0;

	t0 = get_sec_ticks();

	if (par->gns110_modem_alarm_time < t0) {
	    sec_to_str(par->gns110_modem_alarm_time, str);
	    log_write_log_file("ERROR: modem alarm time is over: %s!\n", str);
	    res = -1;
	} else {
	    log_write_log_file("INFO: modem alarm time set OK\n");
	}
    }

    return res;
}

/** 
 * Напечатать тип и номер модема
 */
#pragma section("FLASH_code")
void print_modem_type(void *p)
{
    if (p != NULL) {

	log_write_log_file("INFO: modem type: %s, modem num: %d\n",
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_NOT_MODEM ? "not" :
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_MODEM_OLD ? "old" :
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_type == GNS110_MODEM_AM3 ? "am3" : "benthos",
			   ((GNS110_PARAM_STRUCT *) p)->gns110_modem_num);
    }
}


/**
 * Здесь и далее математика с floating point
 * Посмотреть чем ЗДЕСЬ могут различаца FLOAT и DOUBLE (long double не поддерживается)
 */
#pragma section("FLASH_code")
void print_coordinates(s32 lat, s32 lon)
{
    int temp;

    temp = abs(lat);
    log_write_log_file("INFO: Latitude  --> %04d.%04d(%c)\n", temp / 10000, temp % 10000, lat >= 0 ? 'N' : 'S');

    temp = abs(lon);
    log_write_log_file("INFO: Longitude --> %04d.%04d(%c)\n", temp / 10000, temp % 10000, lon >= 0 ? 'E' : 'W');
}


/**
 * Расчет углов вращений и поворота
 * На входе указатель на данные компаса, акселерометра
 * На выходе указатель на расчитаные углы в углах (радианах) * 10 -> в акселерометре
 */
#pragma section("FLASH_code")
bool calc_angles_new(lsm303_data * acc, lsm303_data * mag)
{

    f32 fTemp;
    f32 fSinRoll, fCosRoll, fSinPitch, fCosPitch;
    f32 fTiltedX, fTiltedY;
    f32 fAcc[3];		/* Акселерометр */
    f32 fMag[3];		/* Компас */
    bool res = false;


    if (acc != NULL && mag != NULL) {
	fAcc[0] = (f32) acc->x / 100.0;
	fAcc[1] = (f32) acc->y / 100.0;
	fAcc[2] = (f32) acc->z / 100.0;

	fMag[0] = (f32) mag->x;
	fMag[1] = (f32) mag->y;
	fMag[2] = (f32) mag->z;

	/* Нормализовать векторы ускорений */
	fTemp = AccSensorNorm(fAcc);

	/* Вычисляем промежуточные параметры матрицы поворота */
	fSinRoll = fAcc[1] / sqrt(pow(fAcc[1], 2) + pow(fAcc[2], 2));
	fCosRoll = sqrt(1.0 - fSinRoll * fSinRoll);
	fSinPitch = -fAcc[0] * fTemp;
	fCosPitch = sqrt(1.0 - fSinPitch * fSinPitch);

	/* Матрица поворота -> к магнитному полю для компонентов X и Y */
	fTiltedX = fMag[0] * fCosPitch + fMag[2] * fSinPitch;
	fTiltedY = fMag[0] * fSinRoll * fSinPitch + fMag[1] * fCosRoll - fMag[2] * fSinRoll * fCosPitch;

	/* Получаем угол head  */
	fTemp = -atan2(fTiltedY, fTiltedX) * 180.0 / M_PI;
	acc->z = (int) (fTemp * 10.0);

	/* и углы roll и pitch */
	fTemp = atan2(fSinRoll, fCosRoll) * 180 / M_PI;
	acc->x = (int) (fTemp * 10.0);

	fTemp = atan2(fSinPitch, fCosPitch) * 180 / M_PI;
	acc->y = (int) (fTemp * 10.0);
	res = true;
    }
    return res;
}

/**
 * Нормализуем векторы ускорений, для того, чтобы считать от единиц G
 */
#pragma section("FLASH_code")
static f32 AccSensorNorm(f32 * pfAccXYZ)
{
    /* Функция обратного кв. корня */
    return inv_sqrt(pow(pfAccXYZ[0], 2) + pow(pfAccXYZ[1], 2) + pow(pfAccXYZ[2], 2));
}

/**
 * Fast inverse square-root  
 * See: http://en.wikipedia.org/wiki/Fast_inverse_square_root
 */
f32 inv_sqrt(f32 x)
{
    unsigned int i = 0x5F1F1412 - (*(unsigned int *) &x >> 1);
    f32 tmp = *(f32 *) & i;
    f32 y = tmp * (1.69000231 - 0.714158168 * x * tmp * tmp);
    return y;
}



/**
 * Углы расчитать и вывести
 * Изменить статут и там в статусе записывать расчитанные углы!!!
 */
bool calc_angles(void *par0, void *par1)
{
    lsm303_data *acc, *comp;
    f32 p, r, h;
    f32 mx, my, mz;
    f32 cx, cy, cz;
    f32 ax, ay, az;
    f32 f, f0;
    bool res = false;

    if (par0 != NULL && par1 != NULL) {
	acc = (lsm303_data *) par0;
	comp = (lsm303_data *) par1;

	// Усреднение акселерометра
	ax = acc->x / 1000.0;
	ay = acc->y / 1000.0;
	az = acc->z / 1000.0;

	// Усреднение компаса
	cx = comp->x / 1100.0;
	cy = comp->y / 1100.0;
	cz = comp->z / 980.0;

	// Считаем pitch и с ax не работаем больше
	f = -(ax);
	if (f >= 1.0)
	    f = 0.999999999;
	if (f <= -1.0)
	    f = -0.999999999;
	p = asin(f);

	// и r
	f = ay;
	if (f >= 1.0)
	    f = 0.999999999;
	if (f <= -1.0)
	    f = -0.999999999;

	f0 = cos(p);
	if (f0 == 0.0) {
	    f0 = 1.0;
	} else {
	    f0 = f / f0;	// error
	}

	if (f0 >= 1.0)
	    f0 = 0.999999999;
	if (f0 <= -1.0)
	    f0 = -0.999999999;
	r = asin(f0);

	mx = cx * cos(p) + cz * sin(p);
	my = cx * sin(r) * sin(p) + cy * cos(r) - cz * sin(r) * cos(p);
	mz = -cx * cos(r) * sin(p) + cy * sin(r) + cz * cos(r) * cos(p);


#if 0
	if (mx > 0 && my >= 0) {
	    h = atan(my / mx);
	} else if (mx < 0) {
	    h = M_PI + atan(my / mx);
	} else if (mx > 0 && my <= 0) {
	    h = 2 * M_PI + atan(my / mx);
	} else if (mx == 0 && my < 0) {
	    h = M_PI / 2;
	} else if (mx == 0 && my > 0) {
	    h = M_PI / 2 * 3;
	}
#else
	h = atan2(my, mx);
#endif

	p *= 180 / M_PI;
	r *= 180 / M_PI;
	h *= 180 / M_PI;

	/* 0...360 */
	if (h <= 0.0) {
	    h = 360 + h;
	}

	/* Расчитаннаые данные здесь  */
	acc->x = p * 10;
	acc->y = r * 10;
	acc->z = h * 10;
	res = true;
    }
    return res;
}




///////////////////////////////////////////////////////////////////////////////
// Все эти функции запускаются из L1
////////////////////////////////////////////////////////////////////////////////

/**
 * расчитать значение фильтра HPF в зависимости от частоты среза
 */
section("L1_code")
u16 get_hpf_from_freq(f32 freq, int dr)
{
    f32 rad, s;
    u16 reg = 0xFFFF;

    if (dr <= 0)
	return reg;

    rad = 2 * M_PI * freq / dr;	/* Для частоты 250 */

    if (rad == 1.0)
	return reg;

    s = 1 - 2 * (cos(rad) + sin(rad) - 1) / cos(rad);

    if (s < 0.0)
	return reg;		/* -1 ошибка  */

    s = (1 - sqrt(s)) * 65536;
    if (s < 65535.0)
	reg = (u16) (s + 0.5);

    return reg;
}

/**
 * первый байт - команда. вернуть число из буфера
 */
section("L1_code")
u16 get_char_from_buf(void *buf, int pos)
{
    return *(u8 *) buf + pos;
}

/**
 * Вернуть число short
 */
section("L1_code")
u16 get_short_from_buf(void *buf, int pos)
{
    u16 res;
    memcpy(&res, (u8 *) buf + pos, 2);
    return res;
}

/**
 * Вернуть число long
 */
section("L1_code")
u32 get_long_from_buf(void *buf, int pos)
{
    u32 res;
    memcpy(&res, (u8 *) buf + pos, 4);
    return res;
}

/**
 * вернуть число float
 */
section("L1_code")
f32 get_float_from_buf(void *buf, int pos)
{
    union {
	float f;
	u32 l;
    } f;

    memcpy(&f.l, (u8 *) buf + pos, 4);
    return f.f;
}

/**
 * Переводит секунды (time_t) с начала Эпохи в формат TIME_DATE
 */
section("L1_code")
int sec_to_td(long ls, TIME_DATE * t)
{
    struct tm *tm_ptr;

    if ((int) ls != -1) {
	tm_ptr = gmtime(&ls);

	/* Записываем дату, что получилось */
	t->sec = tm_ptr->tm_sec;
	t->min = tm_ptr->tm_min;
	t->hour = tm_ptr->tm_hour;
	t->day = tm_ptr->tm_mday;
	t->mon = tm_ptr->tm_mon + 1;	/* В TIME_DATE месяцы с 1 по 12, в tm_time с 0 по 11 */
	t->year = tm_ptr->tm_year + 1900;	/* В tm годы считаются с 1900 - го, в TIME_DATA с 0-го */
	return 0;
    } else
	return -1;
}

/**
 * Выдать длинное время
 */
section("L1_code")
s64 get_long_time(void)
{
    if (TIMER1_is_run())
	return (s64) TIMER1_get_long_time();
    else
	return (s64) TIMER2_get_long_time();
}


/**
 * Выдать тики в секундах
 */
section("L1_code")
long get_sec_ticks(void)
{
    return (long) ((u64) get_long_time() / TIMER_NS_DIVIDER);	/* Делаем секунды */
}

/**
 * Выдать тики в миллисекундах (секунды + миллисекунды)
 */
section("L1_code")
s64 get_msec_ticks(void)
{
    return (s64) ((u64) get_long_time() / TIMER_US_DIVIDER);	/* Делаем миллисекунды из нс */
}


/**
 * Выдать тики в микросекундах
 */
section("L1_code")
s64 get_usec_ticks(void)
{
    return (s64) ((u64) get_long_time() / TIMER_MS_DIVIDER);	/* Делаем микросекунды из нс */
}
