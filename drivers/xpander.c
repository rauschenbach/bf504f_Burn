/************************************************************************************** 
 * В этом файле будет производица и настройка SPORT для связи в внешней Атмегой
 * плюс посылка исходящих команд и прием данных с АЦП и битов с входных портов
 * Вызовы этих функций должны производиться ИСКЛЮЧИТЕЛЬНО внутри основного цикла!!!
***************************************************************************************/
#include "xpander.h"
#include "bq32k.h"
#include "utils.h"
#include "sport0.h"
#include "ports.h"
#include "bmp085.h"
#include "lsm303.h"
#include "rele.h"
#include "log.h"
#include "led.h"
#include "pll.h"

/**
 * Перезапуск АЦП при остановке
 */
static struct {
    long time;
    u8 num;			// число перезапусков
    bool err;
} adc_error_init;


static u8 XPANDER_read_byte(u8);
static void XPANDER_write_byte(u8, u8);
static void XPANDER_set_bit(u8, u8);
static void XPANDER_clear_bit(u8, u8);
static void XPANDER_toggle_bit(u8, u8);
static u16 XPANDER_adc_get_data(u8);
static void XPANDER_adc_init(void);

#define 	XPANDER_ADC_TIMEOUT		10


/**
 * Инициализирвать все датчики, подкюченные по TWI и в АТмеге
 */
#pragma section("FLASH_code")
void adc_init(void *par)
{
    DEV_STATUS_STRUCT *status;

    if (par) {
	status = (DEV_STATUS_STRUCT *) par;

	// Инициализируем атмегу
	XPANDER_adc_init();

	// Инициализируем датчик температуры
	if (bmp085_init() == false) {
	    status->st_test0 |= 0x02;	// ставим ошибку
	}
	// наклономер с компасом
	if (lsm303_init_acc() == false) {
	    status->st_test0 |= 0x04;	// ставим ошибку
	}

	if (lsm303_init_comp() == false) {
	    status->st_test0 |= 0x08;	// ставим ошибку
	}
    }
}




/**
 * "Быстрая" функция - получить напряжения и ток с ацп, пишем в статус
 *  Заполнить статус данными
 */
#pragma section("FLASH_code")
bool adc_get(void *par)
{
    lsm303_data acc, comp;
    DEV_STATUS_STRUCT *status;
    int t, p;
    u16 u, i;

    if (par == NULL)
	return false;

    status = (DEV_STATUS_STRUCT *) par;

    // Для новых плат 
#if defined  GNS110_R2B_BOARD || defined GNS110_R2C_BOARD
    /* 4 канала напряжения */
    u = XPANDER_adc_get_data(0);
    t = u * ATMEGA_VOLT_COEF;
    status->am_power_volt = t;

    u = XPANDER_adc_get_data(1);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_ext_volt = t;

    u = XPANDER_adc_get_data(2);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_volt = t;

    u = XPANDER_adc_get_data(3);
    t = u * ATMEGA_VOLT_COEF;
    status->regpwr_volt = t;

#if 10				// для теста
    if (t == 0) {
	adc_error_init.err = true;
	if (get_sec_ticks() != adc_error_init.time) {
	    adc_init(status);
	}
    }
#endif


    /* 3 канала тока */
    i = XPANDER_adc_get_data(5);
    t = i * ATMEGA_AMPER_SUPPLY_COEF;
    status->ireg_sense = t - 8;

    i = XPANDER_adc_get_data(4);
    t = i * ATMEGA_AMPER_AM3_COEF;
    t = (t - 16) * 100 / 354;
    status->iam_sense = t;

    i = XPANDER_adc_get_data(6);
    t = i * ATMEGA_AMPER_BURN_COEF;
    status->iburn_sense = (t - 80);

#else				/* Плата A */
    u = XPANDER_adc_get_data(0);
    t = u * ATMEGA_VOLT_COEF;
    status->regpwr_volt = t;	// принимает отрицательное :(

    u = XPANDER_adc_get_data(1);
    t = u * ATMEGA_VOLT_COEF;
    status->burn_volt = t;

    i = XPANDER_adc_get_data(2);
    t = i * ATMEGA_AMPER_SUPPLY_COEF;
    status->ireg_sense = t - 8;

    i = XPANDER_adc_get_data(3);
    t = i * ATMEGA_AMPER_BURN_COEF;
    status->iburn_sense = t - 80;

    status->am_power_volt = 0;
    status->burn_ext_volt = 0;
    status->iam_sense = 0;
#endif

	    /* Если у нас проект для Океанологии - пишем время RTC в статус */
#if !defined		GNS110_PROJECT
	    /* Если нет тестирования */
	    if (!(status->st_main & 0x08) && (t = get_rtc_sec_ticks()) != -1) {
		status->gns_rtc = t;	// Часы RTC
		status->st_test0 &= ~0x01;	// сняли неисправность
	    } else {
		status->st_test0 |= 0x01;	// неисправность поставили
	    }
#endif

    // Если нет ошибки датчика давление по I2C
    if (bmp085_data_get(&t, &p) == true) {
	status->temper1 = t;
	status->press = p;
	status->st_test0 &= ~0x02;	// Неисправность сняли
    } else {
	status->temper1 = 0;
	status->press = 0;
	status->st_test0 |= 0x02;	// Неисправность
    }

    /* Данные акселерометра */
    if (lsm303_get_acc_data(&acc) == true) {
	status->st_test0 &= ~0x04;	// Неисправность сняли
    } else {
	status->st_test0 |= 0x04;	// Неисправность
    }

    /* Данные компаса + температура */
    if (lsm303_get_comp_data(&comp) == true) {
	status->temper0 = comp.t;	// температура
	status->st_test0 &= ~0x08;	// Неисправность сняли
    } else {
	status->temper0 = 0;	// температура
	status->st_test0 |= 0x08;	// Неисправность
    }

    /* Расчитаем углы */
    if (!(status->st_test0 & 0x04) && !(status->st_test0 & 0x08) && calc_angles(&acc, &comp)) {
	status->pitch = acc.x;
	status->roll = acc.y;
	status->head = acc.z;
    }

    return true;		/*vvvv Добавить ошибку для пред. строки!  */
}

/**
 * Запуск преобразования для канала. Получить напряжение на канале АЦП атмеги 
*/
#pragma section("FLASH_code")
static u16 XPANDER_adc_get_data(u8 ch)
{
    u8 byte;
    u16 result = 0;
    int timeout = XPANDER_ADC_TIMEOUT;
    int num;


    /* Выбираем канал АТМЕГИ */
    if (ch < 4) {
	num = 0;

#if defined  GNS110_R2A_BOARD
	num = ch;
#endif

	switch (ch) {
	case 0:
	    XPANDER_clear_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_clear_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	case 1:
	    XPANDER_set_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_clear_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	case 2:
	    XPANDER_clear_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_set_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;

	default:
	    XPANDER_set_bit(MES_MUX_SELA_PORT, 1 << MES_MUX_SELA_PIN);
	    XPANDER_set_bit(MES_MUX_SELB_PORT, 1 << MES_MUX_SELB_PIN);
	    break;
	}
    } else {
	num = ch - 3;
    }

    /* Маскируем 3 старшых бита в мультиплексоре */
    byte = 0x40;
    byte |= (u8) num;

    /* Выбираем канал АЦП */
    XPANDER_write_byte(ADMUX, byte);
    delay_ms(10);

    /* Запуск конверсии */
    XPANDER_set_bit(ADCSRA, (1 << PIN6));

    /* Ждем завершение преобразования на ранее выбранном канале */
    while (--timeout) {

	byte = XPANDER_read_byte(ADCSRA);
	if ((byte & (1 << PIN4))) {	/* Читаем данные - дождались готовности */
	    XPANDER_set_bit(ADCSRA, (1 << PIN4));

	    result = XPANDER_read_byte(ADCL);
	    result |= XPANDER_read_byte(ADCH) << 8;
	    break;
	}
    }

    return result;
}

/***************************************************************************************** 
 * Инициализировать АЦП на атмеге 
 ****************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_adc_init(void)
{
    /* Прием */
    XPANDER_clear_bit(ADC_PORT_DIR, ADC_PORT_PIN);

    /* Первым делом включить регистр енергосбережения ADC нулем */
    XPANDER_clear_bit(PRR, (1 << PRADC));

    /* Ставим регистр ADMUX - внешний референс: 3.3 V  - AVCC with external capacitor at AREF pin 6 бит
     * Самый первый канал, без выравнивания результата влево */
    XPANDER_write_byte(ADMUX, (1 << PIN6));

    /* Ставим регистр ADCSRA - пока не запускаем, прескалер на 128 (частота 62 кГц)  */
    XPANDER_write_byte(ADCSRA, (1 << PIN7) | (1 << PIN2) | (1 << PIN1) | (1 << PIN0));

    /* Ставим регистр ADCSRB = 0. Free Running Mode */
    XPANDER_write_byte(ADCSRB, 0);

    /* Выключим неиспользуемые цыфровые входы единицами, только аналговые */
    XPANDER_write_byte(DIDR0, 0xFF);

    /* 1 раз запустить */
    adc_error_init.num++;
    adc_error_init.err = false;
    adc_error_init.time = get_sec_ticks();
}


/**************************************************************************************** 
 * Чтение байта из порта - все чтение идет за 3 цыкла 
 * в чтении первая команда теперь "0"
 * параметр: адрес
 * возврат:  данные
 ****************************************************************************************/
#pragma section("FLASH_code")
static u8 XPANDER_read_byte(u8 addr)
{
    u8 byte;

    SPORT0_write_read(0);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    byte = SPORT0_write_read(0);
    delay_us(10);

    return byte;
}


/*************************************************************************************** 
 * Посылка исходящей команды по SPORT в експандер, запись идет за 3 цыкла
 * задержка между посылками - 10 мкс. Експандер выполняет одну команду за 0.125 мкс
 * в записи первая команда теперь "0x80"
 * параметр: адрес, данные
 * возврат:  нет
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_write_byte(u8 addr, u8 data)
{
    SPORT0_write_read(0x80);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * Установка 1-го бита по адресу
 * задержка между посылками - 10 мкс. Експандер выполняет одну команду за 0.125 мкс
 * первая команда теперь "0x20"
 * параметр: адрес, данные
 * возврат:  нет
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_set_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x20);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * Сброс 1-го бита по адресу
 * задержка между посылками - 10 мкс. Експандер выполняет одну команду за 0.125 мкс
 * первая команда теперь "0x10"
 * параметр: адрес, данные
 * возврат:  нет
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_clear_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x10);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

/*************************************************************************************** 
 * Переключение 1-го бита по адресу
 * задержка между посылками - 10 мкс. Експандер выполняет одну команду за 0.125 мкс
 * первая команда теперь "0x10"
 * параметр: адрес, данные
 * возврат:  нет
  ***************************************************************************************/
#pragma section("FLASH_code")
static void XPANDER_toggle_bit(u8 addr, u8 data)
{
    SPORT0_write_read(0x40);
    delay_us(10);
    SPORT0_write_read(addr);
    delay_us(10);
    SPORT0_write_read(data);
    delay_us(10);
}

#pragma section("FLASH_code")
void pin_set(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_set_bit(port, 1 << pin);
    XPANDER_set_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
void pin_clr(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_clear_bit(port, 1 << pin);
    XPANDER_set_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
void pin_hiz(u8 port, u8 pin)
{
    u8 dir = port - 1;
    XPANDER_clear_bit(port, 1 << pin);
    XPANDER_clear_bit(dir, 1 << pin);
}

#pragma section("FLASH_code")
u8 pin_get(u8 port)
{
    return XPANDER_read_byte(port);
}

/**
 * Выключить регистр енергосбережения ADC единицей 
 */
#pragma section("FLASH_code")
void adc_stop(void)
{
    XPANDER_set_bit(PRR, (1 << PRADC));
}
