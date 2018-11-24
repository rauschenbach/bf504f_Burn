/***************************************************************************************
 * Получение данных с 4-х ацп. Во второй версии платы не работает DMA для SPI
 * поэтому сделано простым чтением
 *************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "circbuf.h"
#include "sintab.h"
#include "ads1282.h"
#include "timer3.h"
#include "xpander.h"
#include "eeprom.h"
#include "timer0.h"
#include "utils.h"
#include "main.h"
#include "ports.h"
#include "irq.h"
#include "dac.h"
#include "spi0.h"
#include "pll.h"
#include "log.h"

/**
 * Дефиниции  
 */
#define			PING_PONG_SIZE			1000	/* Размер буфера данных  */
#define  		TEST_BUFFER_SIZE 		100	/* Пакетов */
#define			BYTES_IN_DATA			3	/* Число байт в слове данных АЦП */

/**
 * адреса регистров 
 */
#define 	ID		0x00
#define 	CONFIG0 	0x01
#define 	CONFIG1 	0x02
#define 	HPF0	 	0x03
#define 	HPF1	 	0x04
#define 	OFC0	 	0x05
#define 	OFC1	 	0x06
#define 	OFC2	 	0x07
#define 	FSC0	 	0x08
#define 	FSC1	 	0x09
#define 	FSC2	 	0x0a

/************************************************************************
 * ADS1282 commands - обозначения по даташиту
 ************************************************************************/
#define 	WAKEUP		0x01
#define 	STANDBY	    	0x02
#define 	ADC_SYNC	0x04
#define 	RESET		0x06
#define 	RDATAC		0x10	/* Непрерывное чтение */
#define 	SDATAC		0x11	/* Стоп непрерывное чтение  */
#define 	RDATA		0x12	/* Чтение по команде */
#define 	OFSCAL		0x60	/* смещение нуля */
#define 	GANCAL		0x61	/* коэффициент */
/***********************************************************************************
 * Если ping_pong_flag == 0 - собираем пинг, отправляем на карту понг
 * Если ping_pong_flag == 1 - собираем пoнг, отправляем на карту пинг
 * Размер буфера PING или PONG должен быт кратен 4!
 * Размер буфера PING или PONG - описаны здесь:
 ***********************************************************************************/
section("FLASH_data")
static const ADS1282_WORK_STRUCT adc_work_struct[] = {
// число байт в одном пакете измерений
// время записи одного ping
// число отсчетов со всех каналов в одном ping - не должно превышать 12000
// размер в байтах пакета пинг
    //freq, bytes, sec, num_in_pack, pack_size, period_us, sig in min, sig in hour
    {SPS62,  3, 60, 3750, 11250, 16000, 1, 60},	// 62.5 Гц 3  байт. в пакете. 60 сек
    {SPS62,  6, 30, 1875, 11250, 16000, 2, 120},	// 62.5 Гц 6  байт. в пакете. 30 сек
    {SPS62,  9, 20, 1250, 11250, 16000, 3, 180},	// 62.5 Гц 9  байт в пакете. 20 сек
    {SPS62, 12, 12,  750,  9000, 16000, 5, 300},	// 62.5 Гц 12 байт в пакете. 12 сек

    {SPS125,  3, 20, 2500,  7500, 8000,  3, 180},	// 125 Гц 3  байт. в пакете. 20 сек
    {SPS125,  6, 12, 1500,  9000, 8000,  5, 300},	// 125 Гц 6  байт. в пакете. 12 сек
    {SPS125,  9, 10, 1250, 11250, 8000,  6, 360},	// 125 Гц 9  байт в пакете. 10 сек
    {SPS125, 12,  6,  750,  9000, 8000, 10, 600},	// 125 Гц 12 байт в пакете. 6 сек

    {SPS250,  3, 12, 3000,  9000, 4000,  5, 300},	// 250 Гц 3  байт. в пакете. 12 сек
    {SPS250,  6,  6, 1500,  9000, 4000, 10, 600},	// 250 Гц 6  байт. в пакете. 6 сек
    {SPS250,  9,  4, 1000,  9000, 4000, 15, 900},	// 250 Гц 9  байт в пакете. 4 сек
    {SPS250, 12,  4, 1000, 12000, 4000, 15, 900},	// 250 Гц 12 байт в пакете. 4 сек

    {SPS500,  3, 6, 3000,  9000, 2000, 10,  600},	// 500 Гц 3  байт в пакете. 6 сек
    {SPS500,  6, 4, 2000, 12000, 2000, 15,  900},	// 500 Гц 6  байт в пакете. 4 сек
    {SPS500,  9, 2, 1000,  9000, 2000, 30, 1800},	// 500 Гц 9  байт в пакете  2 сек
    {SPS500, 12, 2, 1000, 12000, 2000, 30, 1800},	// 500 Гц 12 байт в пакете. 2 сек

    {SPS1K,  3, 4, 4000, 12000, 1000, 15,  900},	// 1000 Гц 3  байт в пакете. 4 сек
    {SPS1K,  6, 2, 2000, 12000, 1000, 30, 1800},	// 1000 Гц 6  байт в пакете. 2 сек
    {SPS1K,  9, 1, 1000,  9000, 1000, 60, 3600},	// 1000 Гц 9  байт в пакете. 1 сек
    {SPS1K, 12, 1, 1000, 12000, 1000, 60, 3600},	// 1000 Гц 12 байт в пакете. 1 сек

    {SPS2K,  3, 2, 4000, 12000, 500,  30, 1800},	// 2000 Гц 3 байт. в пакете. 2 сек
    {SPS2K,  6, 1, 2000, 12000, 500,  60, 3600},	// 2000 Гц 6 байт. в пакете. 1 сек
    {SPS2K,  9, 0, 1000,  9000, 500, 120, 7200},	// 2000 Гц 9  байт в пакете. 1/2 сек 
    {SPS2K, 12, 0, 1000, 12000, 500, 120, 7200},	// 2000 Гц 12 байт в пакете. 1/2 сек 

    {SPS4K,  3, 1, 4000, 12000, 250,  60,  3600},	// 4000 Гц 3 байт. в пакете. 1 сек
    {SPS4K,  6, 0, 2000, 12000, 250, 120,  7200},	// 4000 Гц 6 байт. в пакете. 1/2 сек
    {SPS4K,  9, 0, 1000,  9000, 250, 240, 14400},	// 4000 Гц 9  байт в пакете. 1/4 сек
    {SPS4K, 12, 0, 1000, 12000, 250, 240, 14400},	// 4000 Гц 12 байт в пакете. 1/4 сек
};

/**
 * Параметры заполнения буферов для АЦП и режим работы АЦП. Командный или осцилограф 
 * Внутреняя структура
 */
static struct {
	ADS1282_Regs regs;	/*  Коэффициенты АПЦ */

	u64 long_time0;
	u64 long_time1;

	u32 test_irq_num;
	u32 num_irq;		/* количество прерываний */
	s32 num_sig;		/* Номер сигнала */
	u32 sig_in_time;	/* Как часто записывать часовой файл */
	u16 pack_cnt;		/* Счетчик пакетов */
	u16 sig_in_min;		/* Как часто записывать заголовок */

	u16 ping_pong_size;	/* Число собранных пакетов за заданное время */
	u16 sample_us;		/* Уход периода следования прерываний в микросекундах */

	u8 sps_code;		/* Код частоты дискретизации */
	u8 data_size;		/* Размер пакета данных со всех каналов 3..6..9..12 */
	u8 bitmap;		/* Карта каналов */
	u8 ping_pong_flag;	/* Флаг записи - пинг или понг  */
	u8 mode;		/* Режим работы - test, work или cmd */

	bool handler_write_flag;	/* Флаг того, что запись удалась */
	bool handler_sig_flag;	/* Для лампочек */
	bool is_set;		/* Установкеи */
	bool is_init;		/* Используем для "Инициализирован" */
	bool is_run;		/* Используем для "Работает" */
} adc_pars_struct;

/**
 * Ошыпки АЦП пишем сюда
 */
static ADS1282_ERROR_STRUCT adc_error;

static ADS1282_PACK_STRUCT *pack;
static CircularBuffer cb;

static u8 *ADC_DATA_ping = 0;
static u8 *ADC_DATA_pong = 0;


/*************************************************************************************
 * 	Статические функции - видны тока здесь
 *************************************************************************************/
static bool save_adc_const(u8, u32, u32);
static void pin_config(void);
static void irq_register(void);
static void cmd_write(u8);
static void reg_write(u8, u8);
static u8 reg_read(u8);
static int regs_config(u8, u8, u16);
static bool write_adc_const(u8, u32, u32);
static bool read_adc_const(u8, u32 *, u32 *);
static void adc_reset(void);
static void signal_handler(int);
static inline void adc_irq_acc(void);
static inline void adc_sync(void);

static void select_chan(int);
static void read_data_from_spi(int, u32[]);
static void cmd_mode_handler(u32 *, int);
static void work_mode_handler(u32 *, int);

/**
 * Расчитать и заполнить размер буфера для частоты и числа байт в пакете
 * для 125 и 62.5 будет менятся частота тактиования АЦП
 */
#pragma section("FLASH_code")
static int calculate_ping_pong_buffer(ADS1282_FreqEn freq, u8 bitmap, int len)
{
	int i;
	int res = -1;
	u8 bytes = 0;


	/* Какие каналы пишем. Размер 1 сампла (число 1 * 3 - со всех работающих АЦП сразу) */
	for (i = 0; i < 4; i++) {
		if ((bitmap >> i) & 0x01)
			bytes += 3;
	}

	/* Здесь будет переменный размер буфера ping-pong */
	for (i = 0; i < sizeof(adc_work_struct) / sizeof(ADS1282_WORK_STRUCT); i++) {

		if ( /*freq == 0 || */ bytes == 0 || len == 0) {
			break;
		}

		if (freq == adc_work_struct[i].sample_freq && bytes == adc_work_struct[i].num_bytes) {

			adc_pars_struct.bitmap = bitmap;	/* Какие каналы пишем  */
			adc_pars_struct.data_size = adc_work_struct[i].num_bytes;
			adc_pars_struct.ping_pong_size = adc_work_struct[i].samples_in_ping;	/* число отсчетов в одном ping */
			adc_pars_struct.sig_in_min = adc_work_struct[i].sig_in_min;	/* вызывать signal раз в минуту */
			adc_pars_struct.sig_in_time = adc_work_struct[i].sig_in_time;	/* Заголовок пишется столько раз за 1 час */
			adc_pars_struct.sig_in_time *= len;	/* Сколько часов пишем? **** */

			adc_pars_struct.num_sig = 0;
			adc_pars_struct.num_irq = 0;
			adc_pars_struct.ping_pong_flag = 0;
			adc_pars_struct.handler_sig_flag = false;
			adc_pars_struct.pack_cnt = 0;	/* обнуляем счетчики */
			adc_pars_struct.sample_us = adc_work_struct[i].period_us;

			/* Выделим память под буферы - для записи 1 с 2 с, 4 с или больше */
			if (ADC_DATA_ping == NULL && ADC_DATA_pong == NULL) {
				ADC_DATA_ping = calloc(adc_work_struct[i].ping_pong_size_in_byte, 1);
				ADC_DATA_pong = calloc(adc_work_struct[i].ping_pong_size_in_byte, 1);

				if (ADC_DATA_ping == NULL || ADC_DATA_pong == NULL) {
					log_write_log_file("ERROR: Can't alloc memory for ping-pong\n");
					break;
				}
				log_write_log_file("INFO: freq:  %d Hz, bytes: %d\n",
						   TIMER_US_DIVIDER / adc_pars_struct.sample_us, bytes);
				log_write_log_file("INFO: alloc: %d bytes for ping & pong\n",
						   adc_work_struct[i].ping_pong_size_in_byte * 2);
			}
			res = 0;
			break;
		}
	}
	/* Уход - макс. 5% */
	adc_pars_struct.sample_us += adc_pars_struct.sample_us / 20;
	return res;
}


/**
 * Конфигурирование АЦП без запуска
 * Готовим АЦП к настройке - создаем необходимые буферы
 * Проверяем правильность параметров, коотрые были переданы
 */
#pragma section("FLASH_code")
bool ADS1282_config(void *arg)
{
	int volatile i;
	ADS1282_Params *par;
	u8 cfg1, cfg0, hi = 0;
	u16 hpf;
	bool res = false;

	do {
		if (arg == NULL) {
			break;
		}

		IRQ_unregister_vector(ADS1282_VECTOR_NUM);	/* Если запущен - выключаем прерывания АЦП */

		par = (ADS1282_Params *) arg;
		adc_pars_struct.mode = par->mode;	/* Сохраняем режим работы */
		read_ads1282_coefs_from_eeprom(&adc_pars_struct.regs);	/* Получаем коэффициенты из eeprom - эта функция сама выставит правильные коэффициенты */

		hi = (par->res == 1) ? 1 : 0;	/* Энергопотребление */
		cfg0 = (hi << 6) | (1 << 1);	/* Первый регистр: Pulse SYNC, Sample frequency = 250 Hz, High-resolution, Linear phase, Sinc + LPF filter blocks */

		/* Если неверные настройки. NB: SIVY не поддерживает частоту больше 4 кГц 
		 * подсчитаем частоту и fpga с выходом по ошибке */
		if (par->sps > SPS4K || par->pga > 6) {
			log_write_log_file("ERROR: ADC doesn't support this parameters! freq = %d. pga = %d\n",
					   (1 << par->sps) * 125 / 2, 1 << par->pga);
			break;
		}
		log_write_log_file("INFO: ADC parameters OK! freq = %d. pga = %d\n", (1 << par->sps) * 125 / 2,
				   1 << par->pga);


/* на частотах менее 250 уменьшаем частоту тактирования
 * если 125 - в 2 раза
 * если 62.5 - в 4 раза 
 */
		adc_pars_struct.sps_code = par->sps;

		/* Слово конфигурации в рабочем режиме */
		switch (par->sps) {
		case SPS4K:
			cfg0 |= 4 << 3;
			break;

		case SPS2K:
			cfg0 |= 3 << 3;
			break;

		case SPS1K:
			cfg0 |= 2 << 3;
			break;

		case SPS500:
			cfg0 |= 1 << 3;
			break;

		case SPS250:
			cfg0 |= 1 << 3;
			break;

#if QUARTZ_CLK_FREQ==(8192000)
		case SPS125:
			cfg0 |= 0 << 3;
			TIMER3_change_freq(SPS125);
			break;

		case SPS62:		
		default:
			cfg0 |= 0 << 3;
			TIMER3_change_freq(SPS62);
			break;
#endif		
		}


//////

		cfg1 = (par->pga);	/* Второй регистр: PGA chopping disabled + PGA Gain Select. AINP1 и AINN1 */


		/* добавим chopping  */
		if (par->chop) {
			cfg1 |= (1 << 3);
		}

		hpf = par->hpf;	/* Если (-1) или (0) не ставим фильтр и не меняем коэффициент-пишем предупреждение */
		if (hpf == 0xffff || hpf == 0) {
			log_write_log_file("WARN: HI-Pass filter is not correct (0x%04X)\n", hpf);
			log_write_log_file("WARN: Filter won't be set\n");
			hpf = 0;
		}

		/* В рабочем режиме проводим эти настройки  */
		if (par->mode == WORK_MODE) {

			if (calculate_ping_pong_buffer
			    ((ADS1282_FreqEn) adc_pars_struct.sps_code, par->bitmap, par->file_len) != 0) {
				log_write_log_file("ERROR: Can't calculate buffer PING PONG\n");
				break;
			}

			/* Теперь заполняем заголовок */
			log_fill_adc_header(adc_pars_struct.sps_code, adc_pars_struct.bitmap,
					    adc_pars_struct.data_size);
			log_write_log_file("INFO: change ADS1282 header...OK\n");

		} else if (par->mode == CMD_MODE) {

			/* Если нет буфера для данных - создадим его */
			if (pack == NULL) {
				pack = calloc(sizeof(ADS1282_PACK_STRUCT), 1);
				if (pack == NULL) {
					adc_pars_struct.is_run = false;
					break;
				}
			}
			pack->rsvd = 0;	/* каждый раз обнуляем */

			/* Если буфер не существует создадим его на TEST_BUFFER_SIZE пачек ADS1282_PACK_STRUCT */
			if (!adc_pars_struct.is_run && cb_init(&cb, TEST_BUFFER_SIZE)) {
				adc_pars_struct.is_run = false;
				break;
			}


		} else {	/* Теst mode */
			log_write_log_file("--------------------Checking ADS1282 config-----------------\n");

			/* Первый регистр: Pulse SYNC, Sample frequency = 250 Hz, High-resolution, Linear phase, Sinc + LPF filter blocks */
			cfg0 = (hi << 6) | (1 << 1);
			cfg0 |= ((par->sps - 1) << 3);
			cfg1 = (1 << 3) | 2;
		}
		memset(&adc_error, 0, sizeof(adc_error));

		adc_pars_struct.is_set = true;
		if (regs_config(cfg0, cfg1, hpf) == 0) {	/*  Конфигурируем регистры АЦП во всех режимах */
			adc_pars_struct.is_init = true;	/* Инициализирован  */
			res = true;
		} else {
			log_write_log_file("Error config regs\n");
			adc_pars_struct.is_init = false;	/* Инициализирован  */
		}
	} while (0);
	return res;
}


/**
 * Конфигурировать регистры всех 4 АЦП, SPI0, чипселект дергаем в этой функции
 * Настраиваем выводы _1282_OWFL.._1282_DRDY - вход, F14..CH_SEL_B - выход
 * Сразу поставим Data Rate в параметре
 */
#pragma section("FLASH_code")
static int regs_config(u8 cfg0, u8 cfg1, u16 hpf)
{
	int i;
	int res = 0;

	pin_config();		/* Выводы DRDY и OWFL + мультиплексор + SPI0 */
	adc_reset();		/* Before using the Continuous command or configured in read Data serial interface: reset the serial interface. */

	if (!adc_pars_struct.is_set)
		return -1;

	/* Выбираем АЦП, в засисимости от карты включенных каналов */
	for (i = 0; i < ADC_CHAN; i++) {
		u8 byte0, byte1, byte2;
		u16 temp;

		select_chan(i);

		/* Остановим. The SDATAC command must be sent be read by Read Data By Command. 
		 * The Read before register read/write operations to cancel the Data opcode
		 * command must be sent in this mode Read Data Continuous mode  
		 */
		cmd_write(SDATAC);

		/* Фильтр сначала, если стоит ноль то установки по-умолчанию */
		if (hpf != 0) {
			cfg0 |= 0x03;	/* HPF + LPF + Sinc filter  */
			reg_write(HPF0, hpf);	/* Младший */
			reg_write(HPF1, hpf >> 8);	/* Старший  */
		}
		reg_write(CONFIG0, cfg0);	/* SPS = xxx  */
		reg_write(CONFIG1, cfg1);	/* PGA = xxx , AINP1 + AINN1 +  с чопером */

		/* В качестве проверки сделать чтение - должны прочитать то же самое что записали */
		log_write_log_file("------------------ADC %d tunning------------------\n", i + 1);
		log_write_log_file("ID  : 0x%02x\n", reg_read(ID));


		byte0 = reg_read(CONFIG0);
		if (byte0 != cfg0) {
			log_write_log_file("FAIL: CFG0 = 0x%02x\n", byte0);
			res = -1;
		}
		log_write_log_file("CFG0: 0x%02x\n", byte0);

		adc_error.adc[i].cfg0_wr = cfg0;
		adc_error.adc[i].cfg0_rd = byte0;


		byte1 = reg_read(CONFIG1);
		if (byte1 != cfg1) {
			log_write_log_file("FAIL: CFG1 = 0x%02x\n", byte1);
			res = -1;
		}
		log_write_log_file("CFG1: 0x%02x\n", byte1);

		adc_error.adc[i].cfg1_wr = cfg1;
		adc_error.adc[i].cfg1_rd = byte1;


		if (hpf != 0) {
			byte0 = reg_read(HPF0);
			byte1 = reg_read(HPF1);
			temp = ((u16) byte1 << 8) | byte0;

			if (temp != hpf) {
				log_write_log_file("FAIL: HPF = 0x%04x. Need to be 0x%04x\n", temp, hpf);
			}
			log_write_log_file("HPF : 0x%04x\n", hpf);
		}

		/* При ошибках EEPROМ или если нет фильтра */
		if (hpf == 0) {
			log_write_log_file("Get from EEPROM OFFSET: %d and GAIN: %d\n",
					   adc_pars_struct.regs.chan[i].offset, adc_pars_struct.regs.chan[i].gain);

			/* Смещение - проверка чтобы в EEPROM все было верно */
			if (abs((int) adc_pars_struct.regs.chan[i].offset) > 100000) {
				log_write_log_file("Offset error! set to 64000\n");
				adc_pars_struct.regs.chan[i].offset = 64000;
			}
		} else {
			log_write_log_file("Set offset to 0\n");
			adc_pars_struct.regs.chan[i].offset = 0;
		}

		/* Проверяем ошыпки  eeprom */
		if (abs((int) adc_pars_struct.regs.chan[i].gain) < 2000000
		    && abs((int) adc_pars_struct.regs.chan[i].gain) > 8000000) {
			log_write_log_file("Gain error! set to 0x400000\n");
			/* Была ошибка - offset вместо gain! */
			adc_pars_struct.regs.chan[i].gain = 0x400000;
		}

		/* Запишем в регистр АЦП */
		if (write_adc_const(i, adc_pars_struct.regs.chan[i].offset, adc_pars_struct.regs.chan[i].gain) == true) {	/* Запишем их внутрь регистров */
			log_write_log_file("Write regs offset: %d & gain: %d OK\n", adc_pars_struct.regs.chan[i].offset,
					   adc_pars_struct.regs.chan[i].gain);
		} else {
			log_write_log_file(" FAIL: Write regs offset & gain\n");
			res = -1;
		}
	}
	log_write_log_file("-------------- end reg_config ---------------\n");

	return res;
}


/**
 * Запустить все АЦП с заданным коэффициентом
 */
#pragma section("FLASH_code")
void ADS1282_start(void)
{
	u8 i;
	u8 cfg1;

	if (adc_pars_struct.is_run) {	/* Уже запускали */
		return;
	}


	/* Ставим сигнал на IVG14 если в рабочем режиме. На pga не смотрим. */
	if (adc_pars_struct.mode == WORK_MODE) {
		adc_pars_struct.handler_write_flag = true;	/* В первый раз говорим, что можно писать */
		signal(SIGIVG14, signal_handler);
	}

	/* Set the data mode. Запустили все */
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);
		cmd_write(RDATAC);
	}

	adc_sync();		/* Синхронизируем АЦП */
	IRQ_register_vector(ADS1282_VECTOR_NUM);	/* Регистрируем обработчик для АЦП на 8 приоритет */
	irq_register();		/* Теперь конфигурируем IRQ */
	select_chan(0);		/* Выбираем 0-й канал */
	adc_pars_struct.num_irq = 0;
	adc_pars_struct.is_run = true;	/* Уже запускали */
	log_write_log_file("INFO: ADS1282 start OK\n");
}


/**
 * Стоп АЦП из режима CONTINIOUS в PowerDown, удаление буферов
 * Выключение SPI0
 */
#pragma section("FLASH_code")
void ADS1282_stop(void)
{
	int i;

	ADS1282_stop_irq();	/* Запретить прерывания */
	IRQ_unregister_vector(ADS1282_VECTOR_NUM);	/* Если запущен - выключаем прерывания АЦП */
	pin_config();		/* Выводы DRDY и OWFL + мультиплексор + SPI0 */

	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* Выбираем АЦП */
		cmd_write(SDATAC);

		if (WORK_MODE == adc_pars_struct.mode) {
			u16 hpf;
			log_write_log_file("------------------ADC stop------------------\n");
			log_write_log_file("CFG0: 0x%02x\n", reg_read(CONFIG0));	// Прочитаем регистры для проверки
			log_write_log_file("CFG1: 0x%02x\n", reg_read(CONFIG1));

			hpf = ((u16) reg_read(HPF1) << 8) | reg_read(HPF0);
			log_write_log_file("HPF : 0x%04x\n", hpf);
			cmd_write(STANDBY);
		}
	}


	/* Рабочий режим  */
	if (WORK_MODE == adc_pars_struct.mode) {

		/* Удаляем буферы */
		if (ADC_DATA_ping != NULL) {
			free(ADC_DATA_ping);
			ADC_DATA_ping = 0;
		}

		if (ADC_DATA_pong != NULL) {
			free(ADC_DATA_pong);
			ADC_DATA_pong = 0;
		}
	}
	adc_pars_struct.is_run = false;	/* не запущен  */
	adc_pars_struct.is_init = false;	/* Требуется инициализация  */
	adc_pars_struct.num_sig = 0;
	adc_pars_struct.test_irq_num = adc_pars_struct.num_irq;	/* Сохраним число */
	adc_pars_struct.num_irq = 0;
	adc_pars_struct.pack_cnt = 0;	/* обнуляем счетчики */
	adc_pars_struct.ping_pong_flag = 0;
	adc_pars_struct.handler_sig_flag = false;

	SPI0_stop();
}

/**
 * Прерывание в командном режиме - можно из FLASH
 * Байты наверх в Little Endian
 */
#pragma section("L1_code")
static void cmd_mode_handler(u32 * data, int num)
{
	DEV_STATUS_STRUCT status;

	if (adc_pars_struct.pack_cnt == 0) {
		u64 time_ms = get_msec_ticks();

		/* Секунда + миллисекунды первого пакета */
		pack->adc = adc_pars_struct.sps_code;	/* код частоты дискретизации */
		pack->msec = time_ms % 1000;	/* Миллисекунда первого пакета  */
		pack->sec = time_ms / 1000;	/* время UNIX                   */
	}

	/* Убираем самый младший байт */
	pack->data[adc_pars_struct.pack_cnt].x = data[0] >> 8;
	pack->data[adc_pars_struct.pack_cnt].y = data[1] >> 8;
	pack->data[adc_pars_struct.pack_cnt].z = data[2] >> 8;
	pack->data[adc_pars_struct.pack_cnt].h = data[3] >> 8;
	adc_pars_struct.pack_cnt++;

	/* Пишем 0...19 в круговой буфер  */
	if (adc_pars_struct.pack_cnt >= NUM_ADS1282_PACK) {
		adc_pars_struct.pack_cnt = 0;
		if (cb_is_full(&cb)) {
			cmd_get_dsp_status(&status);
			status.st_main |= 0x20;	/* ставим статус - "буфер полон". Очищаем при чтении, не здесь! */
			pack->rsvd++;
			cmd_set_dsp_status(&status);
		}
		cb_write(&cb, pack);	/* Пишем в буфер */
	}
}



/**
 * Прерывание в рабочем режиме 4 канала данных
 */
section("L1_code")
static void work_mode_handler(u32 * data, int num)
{
	u8 *ptr;
	register u8 i, ch, shift = 0;
	u32 d;
	u64 ns, us;

	ns = get_long_time();	// время в наносекундах
	us = ns / 1000;		// микросекунды
	adc_error.time_last = adc_error.time_now;
	adc_error.time_now = us;	// Время сампла

	/* Смотрим порядок следования прерываний АЦП. Если опоздали на 5% - фиксировать ошибку АЦП */
	if (adc_error.time_last != 0 && adc_error.time_now - adc_error.time_last > adc_pars_struct.sample_us) {
		adc_error.sample_miss++;
	}


	/* Пишем время первого пакета */
	if (0 == adc_pars_struct.pack_cnt) {
		adc_pars_struct.long_time0 = ns;	/* Секунды + наносекунды первого пакета */
	}

	/* Собираем пинг или понг за 1 секунду */
	if (0 == adc_pars_struct.ping_pong_flag) {
		ptr = ADC_DATA_ping;	/* 0 собираем пинг */
	} else {
		ptr = ADC_DATA_pong;	/* 1 собираем понг */
	}


	/* Какие каналы включены */
	ch = adc_pars_struct.bitmap;

	for (i = 0; i < ADC_CHAN; i++) {
		if ((1 << i) & ch) {
			d = byteswap4(data[i]);	/* Если канал включен - перевернем в Big Endian и запишем только 3 байта */
			memcpy(ptr + adc_pars_struct.pack_cnt * adc_pars_struct.data_size + shift, &d, BYTES_IN_DATA);
			shift += BYTES_IN_DATA;
		}
	}

	adc_pars_struct.pack_cnt++;	/* Счетчик пакетов увеличим  */

	/*  1 раз в секунду за 2 или за 4 секунды будет 1000 пакетов */
	if (adc_pars_struct.pack_cnt >= adc_pars_struct.ping_pong_size) {
		adc_pars_struct.pack_cnt = 0;	/* обнуляем счетчик */
		adc_pars_struct.ping_pong_flag = !adc_pars_struct.ping_pong_flag;	/* меняем флаг. что собрали то и посылаем */
		ssync();

		/* Если нет флага записи: handler_write_flag, значит запись на SD застопорилась - скидываем на flash  */
		if (!adc_pars_struct.handler_write_flag)
			adc_error.block_timeout++;	/* Считаем ошибки сброса блока на SD */

		adc_pars_struct.long_time1 = adc_pars_struct.long_time0;	/* Скопируем. иначе можем затереть */
		raise(SIGIVG14);	/* Пока сигналом! */
	}
}


/**
 * Прерывание по переходу из "1" в "0" для АЦП
 */
section("L1_code")
void ADS1282_ISR(void)
{
	u32 data[ADC_CHAN];	/* 4 канала  */

	read_data_from_spi(ADC_CHAN, data);	/* Получаем в формате BIG ENDIAN!!! Сверху вниз */

	do {
#if 0
		/* Для частоты 125 - беру каждый 2 отсчет. условие в такой последовательности! */
		if ((adc_pars_struct.sps_code == SPS125) && (adc_pars_struct.num_irq % 2 == 1)) {
			break;
		}
#endif

		/* Для режима ОТ PC */
		if (adc_pars_struct.mode == CMD_MODE) {
			cmd_mode_handler(data, ADC_CHAN);	/* Получаем на 125  */
		} else if (adc_pars_struct.mode == WORK_MODE) {	/* Программный режым - запись на SD карту  */
			work_mode_handler(data, ADC_CHAN);
		}
	} while (0);
	adc_pars_struct.num_irq++;	/* счетчик прерываний */

	adc_irq_acc();		/* Подтвердим в конце, иначе будут вхождение внутрь одного прерывания */

	PLL_sleep(DEV_REG_STATE);
}

/**
 * Получить 32 бита из АЦП данные сразу со всех каналов в Little ENDIAN
 */
section("L1_code")
static void read_data_from_spi(int num, u32 data[num])
{
	int i, j;
	register u8 b0, b1, b2, b3;

	/* Получаем в формате Little ENDIAN!!! Сверху вниз */
	for (i = num - 1; i >= 0; i--) {
		select_chan(i);
		b0 = SPI0_write_read(0);	// старший
		b1 = SPI0_write_read(0);	// ст.средний
		b2 = SPI0_write_read(0);	// мл. средний
		b3 = SPI0_write_read(0);	// младший

		data[i] = ((u32) b0 << 24) | ((u32) b1 << 16) | ((u32) b2 << 8) | ((u32) b3);


		/* Для проверки пропусков АЦП пишем синус, если есть сбои - синус будет кривым и сбойным */
#if defined ENABLE_TEST_DATA_PACK
		if (i == 2) {
			int d;
			j = get_usec_ticks() / 25;	// доли миллисекунды
			d = get_sin_table(j % SIN_TABLE_SIZE) - 0x3fff;
			data[i] = d << 8;
			adc_error.test_counter++;
		}
#endif
	}
}

/**
 * Сигнал - вызывется 1...4 раза в секунду при нашей частоте при заполнения любого PING_PONG
 */
section("L1_code")
static void signal_handler(int s)
{
	u8 *ptr;
	u64 ns = adc_pars_struct.long_time1;	/* Получили в наносекундах */
	adc_pars_struct.handler_write_flag = false;	/* Убираем флаг сигнала - входим в обработчик */

	/*  Делаем суточные файлы или часовые - по времени измеренном в ISR */
	if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_time) {
		log_create_hour_data_file(ns);
	}

	/* 1 раз в минуту скидываем заголовок с изменившимся временем  */
	if (0 == adc_pars_struct.num_sig % adc_pars_struct.sig_in_min) {
		log_write_adc_header_to_file(ns);	/* Миллисекунды пишем в заголовок */
	}


	/* Cбрасываем буфер на SD карту */
	if (1 == adc_pars_struct.ping_pong_flag) {	/* пишем массив пинг */
		ptr = ADC_DATA_ping;
	} else {		/* пишем массив понг */
		ptr = ADC_DATA_pong;
	}

	log_write_adc_data_to_file(ptr, adc_pars_struct.ping_pong_size * adc_pars_struct.data_size);

	adc_pars_struct.num_sig++;	/* Сбрасываем данные за 4, 2 или 1 секунду */
	adc_pars_struct.handler_sig_flag = true;	/* 1 раз в секунду - зеленый светодиод */
	adc_pars_struct.handler_write_flag = true;	/* Ставим флаг - выходим из обработчика. */
	signal(SIGIVG14, signal_handler);	/* Снова восстановим сигнал на IVG14 */
	PLL_sleep(DEV_REG_STATE);	/* vvvvv: находимся в режиме регистрации - спать! */
}

/**
 * Возвращаем счетчик ошибок
 */
#pragma section("FLASH_code")
void ADS1282_get_error_count(ADS1282_ERROR_STRUCT * err)
{
	if (err != NULL) {
		memcpy(err, &adc_error, sizeof(adc_error));
	}
}

/**
 * Команда ацп в PD - перед включением
 */
#pragma section("FLASH_code")
void ADS1282_standby(void)
{
	int i;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* Выбираем АЦП */
		cmd_write(SDATAC);
		delay_ms(5);
		cmd_write(STANDBY);
	}
}

/**
 * Команда offset cal
 */
#pragma section("FLASH_code")
bool ADS1282_ofscal(void)
{
	int i;

	/* НЕ когда запущен!  */
	if (adc_pars_struct.is_run)
		return false;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* Выбираем АЦП */
		cmd_write(OFSCAL);
	}
	return true;
}

/**
 * Команда gain cal
 */
#pragma section("FLASH_code")
bool ADS1282_gancal(void)
{
	int i;
	/* НЕ когда запущен!  */
	if (adc_pars_struct.is_run)
		return false;

	pin_config();
	for (i = 0; i < ADC_CHAN; i++) {
		select_chan(i);	/* Выбираем АЦП */
		cmd_write(GANCAL);
	}
	return true;
}



/**
 * АЦП запущен?
 */
#pragma section("FLASH_code")
bool ADS1282_is_run(void)
{
	return adc_pars_struct.is_run;
}

/**
 * Получить число прерываний
 */
#pragma section("FLASH_code")
int ADS1282_get_irq_count(void)
{
	return adc_pars_struct.test_irq_num;	/* счетчик прерываний */
}


/**
 *  Эта функция откачивает данные из пакета
 */
section("L1_code")
bool ADS1282_get_pack(void *buf)
{
	if (!cb_is_empty(&cb) && buf != NULL) {
		cb_read(&cb, (ElemType *) buf);
		return true;
	} else {
		return false;	//  "Данные не готовы"
	}
}

/**
 * Прочесть содержимое калибровачнных регистров для n-го канала
 */
#pragma section("FLASH_code")
bool ADS1282_get_adc_const(u8 ch, u32 * offset, u32 * gain)
{
	/* Может быть только 4 канала  */
	if (offset == NULL || gain == NULL || ch > 3 || adc_pars_struct.regs.magic != MAGIC)
		return false;

	/* Выбираем из структуры */
	*offset = adc_pars_struct.regs.chan[ch].offset;
	*gain = adc_pars_struct.regs.chan[ch].gain;
	return true;
}

/**
 * Записать содержимое калибровки для n-го канала в структуру
 */
#pragma section("FLASH_code")
bool ADS1282_set_adc_const(u8 ch, u32 offset, u32 gain)
{
	if (!adc_pars_struct.is_run && ch == 0xff) {	// Записать во flash
		adc_pars_struct.regs.magic = MAGIC;	// Ставим магическое число
		if (write_all_ads1282_coefs_to_eeprom(&adc_pars_struct.regs))	// Столько запишем первых значений
			return false;
		return true;
	}

	/* Нельзя писать регистры запущенного АЦП, может быть только 4 канала, усиление не может быть == 0  */
	if (adc_pars_struct.is_run || gain == 0 || ch > 3) {
		return false;
	}

	/* Записать и в структуру */
	adc_pars_struct.regs.chan[ch].offset = offset;
	adc_pars_struct.regs.chan[ch].gain = gain;
	return true;
}


/**  
 *  Очистить буфер данных
 */
#pragma section("FLASH_code")
bool ADS1282_clear_adc_buf(void)
{
	cb_clear(&cb);
	return true;
}


/**
 *  Записать в регистры константы 1 - го канала
 *  Нельзя писать регистры запущенного АЦП
 */
#pragma section("FLASH_code")
static bool write_adc_const(u8 ch, u32 offset, u32 gain)
{
	/* может быть только 4 канала, усиление не может быть == 0  */
	if (gain == 0 || ch > 3 || adc_pars_struct.is_run)
		return false;

	select_chan(ch);	/* Выбираем АЦП */
	reg_write(OFC0, offset & 0xFF);
	reg_write(OFC1, offset >> 8);
	reg_write(OFC2, offset >> 16);

	reg_write(FSC0, gain & 0xFF);
	reg_write(FSC1, gain >> 8);
	reg_write(FSC2, gain >> 16);
	return true;
}

/**
 *  Прочитать константы любого канала
 */
#pragma section("FLASH_code")
static bool read_adc_const(u8 ch, u32 * offset, u32 * gain)
{
	u8 byte0, byte1, byte2;

	/* Нельзя читать регистры запущенного АЦП, может быть только 4 канала, усиление не может быть == 0  */
	if (gain == 0 || ch > 3 || adc_pars_struct.is_run)
		return false;

	select_chan(ch);	/* Выбираем АЦП */

	byte0 = reg_read(OFC0);
	byte1 = reg_read(OFC1);
	byte2 = reg_read(OFC2);
	*offset = (byte2 << 16) | (byte1 << 8) | (byte0);

	byte0 = reg_read(FSC0);
	byte1 = reg_read(FSC1);
	byte2 = reg_read(FSC2);
	*gain = (byte2 << 16) | (byte1 << 8) | (byte0);
	return true;
}


/**
 * Узнать флаг внутри сигнала
 */
#pragma section("FLASH_code")
bool ADS1282_get_handler_flag(void)
{
	return adc_pars_struct.handler_sig_flag;
}

/**
 * Сбросить флаг внутри сигнала
 */
#pragma section("FLASH_code")
void ADS1282_reset_handler_flag(void)
{
	adc_pars_struct.handler_sig_flag = false;
}


/**
 * Подтвердить прерывание
 */
#pragma always_inline
static inline void adc_irq_acc(void)
{
	*pPORTFIO_CLEAR = _1282_DRDY;
	ssync();
}

/* Запретим прерывания  */
#pragma section("FLASH_code")
void ADS1282_stop_irq(void)
{
	*pSIC_IMASK0 &= ~IRQ_PFA_PORTF;
	asm("ssync;");
}

/**
 * Запуск АЦП с установленными настройками
 */
#pragma section("FLASH_code")
static void irq_register(void)
{
	/* Прерывание возникнет по перепаду из единицы в ноль,
	 * пин будет читаться как 1 при перепаде в 0 */
	*pPORTFIO_POLAR |= _1282_DRDY;
	*pPORTFIO_EDGE |= _1282_DRDY;
	*pPORTFIO_MASKA_SET |= _1282_DRDY;	/* Установим прерывание  - маска A */

	*pSIC_IAR3 &= 0xFF0FFFFF;
	*pSIC_IAR3 |= 0x00100000;
	asm("ssync;");
}



/**
 * Просто конфирурация ножек DRDY и OWF и инициализация переключателя АЦП
 * Делаем ее один раз всего!
 */
#pragma section("FLASH_code")
static void pin_config(void)
{
	*pPORTF_FER &= ~(_1282_OWFL | _1282_DRDY);	/* Отключаем функции */
	*pPORTFIO_INEN |= (_1282_OWFL | _1282_DRDY);	/* Входы переполнение и готовность на вход */
	*pPORTF_FER &= ~(CH_SEL_A | CH_SEL_B);	/* Отключаем функции */
	*pPORTFIO_DIR |= CH_SEL_A | CH_SEL_B;	/* Выбор этих адресов на выход */
	asm("ssync;");
	SPI0_init();		/* Для АЦП заранее */
}



/*****************************************************************************************
 * Синхронизация всех 4-х АЦП с BlackFin на частоте 192 МГц
 * период импульса SYNC минимум 0.5 мкс 
 * Если эта функция вызывается из прерывания - то пауза делается циклом
 * Но лучше так не делать!
 *****************************************************************************************/
#pragma section("FLASH_code")
static inline void adc_sync(void)
{
	/* Отключаем функции для SYNC */
	*pPORTG_FER &= ~_1282_SYNC;
	*pPORTGIO_DIR |= _1282_SYNC;
	*pPORTGIO_CLEAR = _1282_SYNC;	/* sync изначально в нуле */
	*pPORTGIO_SET = _1282_SYNC;
	asm("ssync;");

	/* Пауза минимум 0.5 мкс - ! */
	delay_us(10);

	*pPORTGIO_CLEAR = _1282_SYNC;
	asm("ssync;");

	*pPORTFIO_CLEAR = _1282_DRDY;	/* или перенести в start_irq   */
	*pILAT |= EVT_IVG8;	/* clear pending IVG8 interrupts */
	ssync();
}


 /************************************************************************
 * Подача команды в АЦП 
 * параметр: команда 
 * возврат:  нет
 ************************************************************************/
#pragma section("FLASH_code")
static void cmd_write(u8 cmd)
{
	int volatile i;

	/* отправка команды */
	SPI0_write_read(cmd);

	/* Пауза в 24 цикла Fclk минимум ~5 микросекунд */
	delay_us(10);
}

/************************************************************************
 * Запись в 1 регистр  АЦП
 * параметр:  адрес регистра, данные
 * возврат:  нет
 ************************************************************************/
#pragma section("FLASH_code")
static void reg_write(u8 addr, u8 data)
{
	int volatile i, j, z;
	u8 volatile cmd[3];

	cmd[0] = 0x40 + addr;
	cmd[1] = 0;
	cmd[2] = data;


	for (i = 0; i < 3; i++) {
		SPI0_write_read(cmd[i]);

		/* Пауза в ~24 цикла Fclk (5 мкс) - минимум! */
		delay_us(10);
	}
}

/************************************************************************
 * Прочитать 1 регистр  АЦП: 
 * параметр:  адрес регистра
 * возврат:   данные
 ************************************************************************/
#pragma section("FLASH_code")
static u8 reg_read(u8 addr)
{
	int volatile i, j;
	u8 volatile cmd[2];
	u8 volatile data;

	cmd[0] = 0x20 + addr;
	cmd[1] = 0;

	for (i = 0; i < 2; i++) {
		SPI0_write_read(cmd[i]);


		/* Пауза в ~24 цикла Fclk (5 мкс) - минимум! */
		delay_us(10);
	}
	data = SPI0_write_read(0);
	return data;
}

/**
 * Сброс АЦП (командой) или експандером
 */
#pragma section("FLASH_code")
static void adc_reset(void)
{
	pin_clr(_1282_RESET_PORT, _1282_RESET_PIN);	/* Опускаем RESET# АЦП на PC7 */
	delay_ms(5);		/* Задержка  */
	pin_set(_1282_RESET_PORT, _1282_RESET_PIN);	/*  Поднимаем вверх  */
}


/**
 * Выбор АЦП - исключительно! 00 - 0, 01 - 1, 10 - 2, 11 - 3,  счет АЦП с нуля
 * если они переключаюца подряд - можно менять только один канал! 
 */
section("L1_code")
static void select_chan(int chan)
{
	switch (chan) {

		/* Первый канал АЦП */
	case 0:
		*pPORTFIO_CLEAR = CH_SEL_B | CH_SEL_A;
		ssync();
		break;

		/* Второй канал АЦП */
	case 1:
		*pPORTFIO_SET = CH_SEL_A;
		*pPORTFIO_CLEAR = CH_SEL_B;
		ssync();
		break;

		/* Третий канал АЦП */
	case 2:
		*pPORTFIO_CLEAR = CH_SEL_A;
		*pPORTFIO_SET = CH_SEL_B;
		ssync();
		break;

		/* Четвертый канал АЦП */
	case 3:
	default:
		*pPORTFIO_SET = CH_SEL_B | CH_SEL_A;
		ssync();
		break;
	}
}
