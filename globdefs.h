#ifndef _GLOBDEFS_H
#define _GLOBDEFS_H

#include <signal.h>
#include <stdint.h>
#include <time.h>


#define		SEC_IN_HOUR			3600

/* Включения для 504 блекфина, не 506!!!!  */
#ifndef _WIN32			/* Embedded platform */
#include <adi_types.h>
#include <services_types.h>
#include <sys/exception.h>
#include <cdefBF504f.h>
#include <defBF504f.h>
#include <ccblkfn.h>
#include <float.h>
#include "config.h"


typedef  	char c8;


#else				/* windows   */
#include <windows.h>
#include <tchar.h>

#ifndef u8
#define u8 unsigned char
#endif

#ifndef s8
#define s8 char
#endif

#ifndef c8
#define c8 char
#endif

#ifndef u16
#define u16 unsigned short
#endif


#ifndef s16
#define s16 short
#endif

#ifndef i32
#define i32  int
#endif


#ifndef u32
#define u32 unsigned long
#endif


#ifndef s32
#define s32 long
#endif


#ifndef u64
#define u64 uint64_t
#endif


#ifndef s64
#define s64 int64_t
#endif


/* Длинное время */
#ifndef	time64
#define time64	int64_t
#endif

#endif



#ifndef u64
#define u64 uint64_t
#endif

#ifndef s64
#define s64 int64_t
#endif

/* Длинное время */
#ifndef	time64
#define time64	int64_t
#endif


#ifndef _WIN32			/* Embedded platform */

/* long double не поддержываеца -  в линуксе может такого не быть! */
#ifndef f32
#define f32 float32_t
#endif

#ifndef bool
#define bool u8
#endif


#ifdef true
#define true 1
#endif

#ifdef false
#define false 0
#endif

#else
/* long double не поддержываеца  */
#ifndef f32
#define f32 float
#endif

#endif



#ifndef IDEF
#define IDEF static inline
#endif

/* На этот адрес будет отвечать наше устройство */
#define 	BROADCAST_ADDR	0xff

/* Сколько пакетов в пачке  */
#define 	NUM_ADS1282_PACK	20

#define 	ADC_CHAN			4	/* число каналов  */
#define		MAGIC				0x4b495245


/*******************************************************************************
 * Состояние устройств для описания State machine
 *******************************************************************************/
typedef enum {
    DEV_POWER_ON_STATE = 0,
    DEV_CHOOSE_MODE_STATE = 1,
    DEV_INIT_STATE,
    DEV_TUNE_WITHOUT_GPS_STATE,	/*Запуск таймера БЕЗ GPS */
    DEV_GET_3DFIX_STATE,
    DEV_TUNE_Q19_STATE,		/* Первичная настройка кварца 19 МГц */
    DEV_SLEEP_AND_DIVE_STATE,	/* Погружение и сон перед началом работы */
    DEV_WAKEUP_STATE,
    DEV_REG_STATE,
    DEV_FINISH_REG_STATE,
    DEV_WAIT_GPS_STATE,
    DEV_HALT_STATE,
    DEV_COMMAND_MODE_STATE,
    DEV_POWER_OFF_STATE,
    DEV_EMERGENCY_WAIT_STATE,	/* аварийное ожидание */            
    DEV_ERROR_STATE = -1	/* Авария */
} DEV_STATE_ENUM;


/*******************************************************************************
 * Типы подключеных модемов
 *******************************************************************************/
typedef enum {
    GNS110_NOT_MODEM = 0,
    GNS110_MODEM_OLD,		//1
    GNS110_MODEM_AM3,		//2
    GNS110_MODEM_BENTHOS,	//3
} GNS110_MODEM_TYPE;


/**
 * Параметры запуска прибора 
 * В этих структурах записываются параметры таймеров
 * соответствует такой записи:
 * 16.10.12 17.15.22	// Время начала регистрации
 * 17.10.12 08.15.20	// Время окончания регистрации
 * 18.10.12 11.17.00	// Время всплытия, включает 5 мин. времени пережига
 * ....
 */
#pragma pack(4)
typedef struct {
    /* Файлы регистрации */
    u32 gns110_pos;		/* Позиция установки */

    char gns110_dir_name[18];	/* Название директории для всех файлов */
    u8 gns110_file_len;		/* Размер файла данных в часах */
    bool gns110_const_reg_flag;	/* Постоянная регистрация */


    /* Модем */
    s32 gns110_modem_rtc_time;	/* Время часов модема */
    s32 gns110_modem_alarm_time;	/* аварийное время всплытия от модема   */
    s32 gns110_modem_type;	/* Тип модема */
    u16 gns110_modem_num;	/* Номер модема */
    u16 gns110_modem_burn_len_sec;	/* Длительность пережига проволоки в секундах от модема */
    u8 gns110_modem_h0_time;
    u8 gns110_modem_m0_time;
    u8 gns110_modem_h1_time;
    u8 gns110_modem_m1_time;

    /* Времена */
    s32 gns110_wakeup_time;	/* время начала подстройки перед регистрацией */
    s32 gns110_start_time;	/* время начала регистрации  */
    s32 gns110_finish_time;	/* время окончания регистрации */
    s32 gns110_burn_on_time;	/* Время начала пережига проволки  */
    s32 gns110_burn_off_time;	/* Время окончания пережига проволки  */
    s32 gns110_gps_time;	/* время включения gps после времени всплытия */

    /* АЦП */
    f32 gns110_adc_flt_freq;	/* Частота среза фильтра HPF */

    u16 gns110_adc_freq;	/* Частота АЦП  */
    u8 gns110_adc_pga;		/* Усиление АЦП  */
    u8 gns110_adc_bitmap;	/* какие каналы АЦП используются */
    u8 gns110_adc_consum;	/* энергопотребление АЦП сюда */
    u8 gns110_adc_bytes;	/* Число байт в слове данных  */
    u8 rsvd[2];
} GNS110_PARAM_STRUCT;


/* Структура, описывающая время, исправим c8 на u8 - время не м.б. отрицательным */
#pragma pack(1)
typedef struct {
    u8 sec;
    u8 min;
    u8 hour;
    u8 week;			/* день недели...не используеца */
    u8 day;
    u8 month;			/* 1 - 12 */
#define mon 	month
    u16 year;
} TIME_DATE;
#define TIME_LEN  8		/* Байт */


/**
 * Этой структуры нету в заголовках   
 */
#pragma pack(4)
struct timespec {
    time_t tv_sec;		/* Секунды  */
    long tv_nsec;		/* Наносекунды */
};



/**
 * В эту стуктуру пишется минутный заголовок при записи на SD карту (pack по 1 байту!)
 */
#if defined   ENABLE_NEW_SIVY
/* В новом сиви паковать по 4 байта */
#pragma pack(4)
typedef struct {
    c8 DataHeader[12];		/* Заголовок данных SeismicDat0\0  */

    c8 HeaderSize;		/* Размер заголовка - 80 байт */
    u8 ConfigWord;		/* Конфигурация */
    c8 ChannelBitMap;		/* Включенные каналы: 1 канал включен */
    c8 SampleBytes;		/* Число байт слове данных */

    u32 BlockSamples;		/* Размер 1 минутного блока в байтах */

    s64 SampleTime;		/* Время минутного сампла: наносекунды времени UNIX */
    s64 GPSTime;		/* Время синхронизации: наносекунды времени UNIX  */
    s64 Drift;			/* Дрифт от точных часов GPS: наносекунды  */

    u32 rsvd0;			/* Резерв. Не используется */

    /* Параметры среды: температура, напряжения и пр */
    u16 u_pow;			/* Напряжение питания, U mv */
    u16 i_pow;			/* Ток питания, U ma */
    u16 u_mod;			/* Напряжение модема, U mv */
    u16 i_mod;			/* Ток модема, U ma */

    s16 t_reg;			/* Температура регистратора, десятые доли градуса: 245 ~ 24.5 */
    s16 t_sp;			/* Температура внешней платы, десятые доли градуса: 278 ~ 27.8  */
    u32 p_reg;			/* Давление внутри сферы кПа */

    /* Положение прибора в пространстве (со знаком) */
    s32 lat;			/* широта (+ ~N, - S):   55417872 ~ 5541.7872N */
    s32 lon;			/* долгота(+ ~E, - W): -37213760 ~ 3721.3760W */

    s16 pitch;			/* крен, десятые доли градуса: 12 ~ 1.2 */
    s16 roll;			/* наклон, десятые доли градуса 2 ~ 0.2 */

    s16 head;			/* азимут, десятые доли градуса 2487 ~ 248.7 */
    u16 rsvd1;			/* Резерв для выравнивания */
} ADC_HEADER;
#else

#pragma pack(1)
typedef struct {
    c8 DataHeader[12];		/* Заголовок данных SeismicData\0  */

    c8 HeaderSize;		/* Размер заголовка - 80 байт */
    c8 ConfigWord;
    c8 ChannelBitMap;
    u16 BlockSamples;
    c8 SampleBytes;

    TIME_DATE SampleTime;	/* Минутное время сампла */

    u16 Bat;
    u16 Temp;
    u8 Rev;			/* Ревизия = 2 всегда  */
    u16 Board;

    u8 NumberSV;
    s16 Drift;

    TIME_DATE SedisTime;	/* Не используется пока  */
    TIME_DATE GPSTime;		/* Время синхронизации */

    union {
	struct {
	    u8 rsvd0;
	    bool comp;		/* компас */
	    c8 pos[23];		/* Позиция (координаты) */
	    c8 rsvd1[3];
	} coord;		/* координаты */

	u32 dword[7];		/* Данные */
    } params;
} ADC_HEADER;
#endif

//////////////////////////////////////////////////////////////////////////////////////////
//                        На отправление
//////////////////////////////////////////////////////////////////////////////////////////

/**
 * Пакет данных на отправление - 20 измерений акселерометра -  240 байт, упакован на 1!!!
 */
#pragma pack(1)
typedef struct {
    u8 len;			/* Длина пакета без контрольной суммы */
    u8 adc;			/* Номер работающих АЦП - четыре бита подряд */
    u16 msec;			/* Миллисекунда первого измерения */
    u32 sec;			/* UNIX TIME первого измерения */

    struct {			/* 3-х байтный пакет (* 4) */
	unsigned x:24;
	unsigned y:24;
	unsigned z:24;
	unsigned h:24;
    } data[NUM_ADS1282_PACK];

    u16 rsvd;			/* Выравнивание */
    u16 crc16;			/* Контрольная сумма пакета  */
} ADS1282_PACK_STRUCT;



/** 
 *  Статус и ошибки устройств на отправление 64 байт. Короткий статус посылается при ошибке (только 2 байта + 1 len + 2 CRC16)
 */
#pragma pack(4)
typedef struct {
    u8 len;			/* 1....Длина пакета без контрольной суммы */
    u8 st_main;			/* 2....главный: нет времени, нет констант, ошибка в команде, нет файла, неисправен, переполнение памяти, остановлен, режим работы */
    u8 st_test0;		/* 3....Самотестирование и ошибки: 0 - часов, 1 - датчик T&P, 2 - Акселер/ компас, 3 - модем, 4 - GPS, 5 - EEPROM, 6 - карта SD, 7 - flash */
    u8 st_test1;		/* 4....Различные устроства */

    u8 st_reset;		/* 5....Причина сброса */
    u8 st_adc;			/* 6....Резерв - пишем здесь ошибки АЦП */
    s16 temper0;		/* 7-8..Температура от внешнего шнура * 10 */

    s32 st_tim4[2];		/* 9-16..Значения таймера 4 во время теста генераторов. Min и Мах. здесь пишем время и дрифт */
    s32 st_tim3[2];		/* 17-24..Значения таймера 3 во время теста генераторов. Min и Мах */

#define gns_rtc      st_tim4[0]
#define gns_time     st_tim4[1]
#define gns_drift    st_tim3[0]

    u32 eeprom;			/* 25-28..Статус EEPROM - данные по одному биту */

    s16 am_power_volt;		/* 29-30..На 1 ноге-сразу 4 канала напряжения */
    s16 burn_ext_volt;          /* 31-32 */

    s16 burn_volt;              /* 33-34 */
    s16 regpwr_volt;            /* 35-36 */

    s16 iam_sense;		/* 37-38.. 3 канала тока */
    s16 ireg_sense;             /* 39-40 */

    s16 iburn_sense;            /* 41-42 */
    s16 temper1;		/* 43-44..Температура * 10 */

    u32 press;			/* 45-48..Давление кПа */

    s16 pitch;			/* 49-50..Крен */
    s16 roll;			/* 51-52..Наклон */

    s16 head;			/* 53-54.. Азимут */
    u16 ports;                  /* 55-56 ..включенные реле и порты */

    u32 quartz;                 /* 57-60 резерв - 4 байта */
    u8  cclk;   		/* 61-62..Частоты в MHz - 2 байта */
    u8  sclk;

    u16 crc16;			/* 63-64..Контрольная сумма пакета  */
} DEV_STATUS_STRUCT;



/**
 * Время работы прибора на отправление
 */
#pragma pack(1)
typedef struct {
    u8 len;			/* Число байт  */
    u32 time_work;		/* Время работы в режиме сбора данных */
    u32 time_cmd;		/* Время работы в командном режиме */
    u32 time_modem;		/* Время работы модема */
    u16 crc16;			/* CRC16  */
} DEV_WORK_TIME_STRUCT;


/* Имя и адрес */
#pragma pack(4)
typedef struct {
    u8 len;			/* Длина */
    u8 ver;			/* Версия ПО: 1, 2, 3, 4 итд */
    u16 rev;			/* Ревизия ПО 0, 1, 2 итд */
    u32 time;			/* Время компиляции */
    u16 addr;			/* Адрес платы */
    u16 crc16;
} DEV_ADDR_STRUCT;


/**
 * Счетчики обменов по УАРТ
 */
typedef struct {
    int rx_pack;		/* Принятые пакеты */
    int tx_pack;		/* переданные пакеты */
    int rx_cmd_err;		/* Ошибка в команде */
    int rx_stat_err;		/* Ошибки набегания, кадра (без стопов) и пр */
    int rx_crc_err;		/* Ошибки контрольной суммы */
} DEV_UART_COUNTS;


/* Описывает когда случился таймаут карты и прочие ошибки */
typedef struct {
    u32 cmd_error;
    u32 read_timeout;
    u32 write_timeout;
    u32 any_error;
} SD_CARD_ERROR_STRUCT;


/**
 * Управление Уартами
 */
typedef struct {
    int baud;			/*Скорость обмена порта */
    void (*rx_call_back_func) (u8);	/* Указатель на функцыю чтения */
    void (*tx_call_back_func) (void);	/* Указатель на функцыю записи */
    bool is_open;		/* Если открыт */
} DEV_UART_STRUCT;

#define 	MODEM_BUF_LEN			64	/* Вполне достаточно */

/**
 * Внешняя команда пришла с UART
 */
#pragma pack (4)
typedef struct {
    u8 cmd;
    u8 len;
    u8 rsvd0[2];

    union {
	c8 cPar[MODEM_BUF_LEN];
	u16 wPar[MODEM_BUF_LEN/2];
	u32 lPar[MODEM_BUF_LEN/4];
	f32 fPar[MODEM_BUF_LEN/4];
    } u;
} DEV_UART_CMD;

/* Строки в лог файле  */
#define   INFO_NUM_STR  	0
#define   POS_NUM_STR		1
#define   BEGIN_REG_NUM_STR	2
#define   END_REG_NUM_STR	3
#define   BEGIN_BURN_NUM_STR	4
#define   BURN_LEN_NUM_STR	5
#define   POPUP_LEN_NUM_STR	6
#define   MODEM_NUM_NUM_STR	7
#define   ALARM_TIME_NUM_STR	8
#define   DAY_TIME_NUM_STR	9
#define   ADC_FREQ_NUM_STR	10
#define   ADC_CONSUM_NUM_STR	11
#define   ADC_PGA_NUM_STR	12
#define   MODEM_TYPE_NUM_STR	13
#define   ADC_BITMAP_NUM_STR	14
#define   FILE_LEN_NUM_STR	15
#define   FLT_FREQ_NUM_STR	16
#define   CONST_REG_NUM_STR	17


#define   RELE_GPS_BIT			(1 << 0)
#define   RELE_MODEM_BIT		(1 << 1)
#define   RELE_BURN_BIT			(1 << 2)
#define   RELE_ANALOG_POWER_BIT		(1 << 3)
#define   RELE_DEBUG_MODULE_BIT		(1 << 4)
#define   RELE_MODEM_MODULE_BIT		(1 << 5)
#define   RELE_USB_BIT			(1 << 6)
#define   RELE_WUSB_BIT			(1 << 7)



#endif				/* globdefs.h */
