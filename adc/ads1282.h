#ifndef _ADS1282_H
#define _ADS1282_H

#include "globdefs.h"
#include "utils.h"


/************************************************************************
 * Дефиниции для ног на блекфине
 ************************************************************************/
#define _1282_SYNC	PG14
#define _1282_OWFL	PF8
#define _1282_DRDY	PF9
#define  CH_SEL_A	PF14
#define  CH_SEL_B	PF15

/* Самплы в секунду - только с этими значениями может работать АЦП   
 * соответствует ConfigWord в заголовке */
typedef enum {
    SPS62 = 0,
    SPS125,
    SPS250,
    SPS500,
    SPS1K,
    SPS2K,
    SPS4K,
} ADS1282_FreqEn;


/* Режим работы АЦП */
typedef enum {
    TEST_MODE = 0,
    WORK_MODE,
    CMD_MODE
} ADS1282_ModeEn;

/* Усиление канала - только с этими значениями может работать АЦП   */
typedef enum {
    PGA1 = 0,
    PGA2,
    PGA4,
    PGA8,
    PGA16,
    PGA32,
    PGA64
} ADS1282_PgaEn;


/* Передаеца внутрь настройки каждого АЦП */
typedef struct {
    ADS1282_FreqEn sps;  /* частота  */
    ADS1282_PgaEn  pga;  /* усиление */
    ADS1282_ModeEn mode; /* режим работы */
    u16     hpf;	 /* Фильтр */
    u8	    res;	 /* Hi или Lo */
    u8      chop;	 /* choping */
    u8      bitmap;	 /* Включенные каналы: 1 канал включен, 0 - выключен */
    u8      file_len;    /* Длина файла записи */
} ADS1282_Params;


/**
 * Параметры АЦП для всех 4-х каналов.
 * Смещение 0 и усиление 
 */
typedef struct {
    u32 magic;			/* Магич. число */
    struct {
	u32 offset;		/* коэффициент 1 - смещение */
	u32 gain;		/* коэффициент 2 - усиление */
    } chan[ADC_CHAN];			/* 4 канала */
} ADS1282_Regs;

/**
 * Ошыпки АЦП пишем сюда
 */
#pragma pack(4)
typedef struct {
    s64 time_now;		/* время, если произойдет ошибка - узнаем в какое время */
    s64 time_last;
    s32 sample_miss;		/* отсчет пришел не вовремя */
    s32 block_timeout;		/* Блок не успел вовремя записаться */
    u32 test_counter;		/* счетчик тестовых пакетов */

    struct {
	u8 cfg0_wr;
	u8 cfg0_rd;
	u8 cfg1_wr;
	u8 cfg1_rd;
    } adc[ADC_CHAN];
} ADS1282_ERROR_STRUCT;


/**
 * Расчет для буфера АЦП в этой структуре
 */
#pragma pack(4)
typedef struct {
   ADS1282_FreqEn  sample_freq; // частота

   u8   num_bytes;   // число байт в одном пакете измерений
   u8   time_ping;   // время записи одного ping
   u16  samples_in_ping; // число отсчетов в одном ping

   u16  ping_pong_size_in_byte; 
   u16  period_us;	// период одного сампла в мкс

   u16  sig_in_min; 
   u16  sig_in_time;
} ADS1282_WORK_STRUCT;


/*****************************************************************************************
 * Прототипы функций. Вcе функции с преффиксом void ADS1282_ вызываются 
 * из других файлов и сразу влияют на все подключенные АЦП
 ******************************************************************************************/
bool ADS1282_config(void *);
void ADS1282_start(void);	/* Запуск АЦП */
void ADS1282_stop(void);	/* Стоп АЦП и POWERDOWN для всех */
bool ADS1282_is_run(void);
bool ADS1282_get_pack(void *);
void ADS1282_get_error_count(ADS1282_ERROR_STRUCT*);
void ADS1282_standby(void);
int ADS1282_get_irq_count(void);
bool ADS1282_ofscal(void);
bool ADS1282_gancal(void);
bool ADS1282_get_adc_const(u8, u32*, u32*);
bool ADS1282_set_adc_const(u8, u32, u32);
bool ADS1282_clear_adc_buf(void);

void ADS1282_ISR(void);		/* Прерывание по готовности */
bool ADS1282_get_handler_flag(void);	/* Для сброса и получения флага пока так! */
void ADS1282_reset_handler_flag(void);
void ADS1282_stop_irq(void);

/*****************************************************************************************
 * Старт АЦП - только разрешим прерывания! 
 ****************************************************************************************/
IDEF void ADS1282_start_irq(void)
{
    *pPORTFIO_CLEAR = _1282_DRDY;
    *pILAT |= EVT_IVG8;		/* clear pending IVG8 interrupts */
    *pSIC_IMASK0 |= IRQ_PFA_PORTF;
    asm("ssync;");
}



#endif				/* ads1282.h */
