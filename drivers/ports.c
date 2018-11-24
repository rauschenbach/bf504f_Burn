#include "xpander.h"
#include "main.h"
#include "ports.h"
#include "timer2.h"
#include "timer3.h"
#include "sport0.h"
#include "utils.h"
#include "rele.h"
#include "spi1.h"
#include "led.h"



/**
 * Инициализация портов на БФ + sport0 и SPI1
 * По-умолчанию все порты на входе в Z - состоянии (page 9 - 3)
 * Просто отключим все их функции
 * 
 */
#pragma section("FLASH_code")
void init_bf_ports(void)
{
    /* Порты F: отключаем функции и все на вход */
    *pPORTF_FER = 0;
    *pPORTFIO_DIR = 0;

    /* От всех портов отключаем функции */
    *pPORTG_FER &= ~(PG0 | PG1 | PG2 | PG3 | PG4 | PG5 | PG6 | PG7 | PG8 | PG9 | PG10 | PG11 /* | PG12 | PG13 */  | PG14 | PG15);
    *pPORTGIO_CLEAR = (PG3 | PG4 | PG11);	/* ставим в ноль */
    *pPORTGIO_DIR = (PG3 | PG4 | PG11);	/* На выход, все остальные на вход  */
#if 0
    *pPORTH_FER &= ~(PH0 | PH1 | PH2);	/* Порты H: PH1 на вход-остальные на выход и в ноль */
    *pPORTHIO_CLEAR = (PH0 | PH2);	/* ставим в ноль */
    *pPORTHIO_DIR = (PH0 | PH2);
#else
    *pPORTH_FER &= ~(PH1 | PH2);	/* Порты H: PH1 на вход-остальные на выход и в ноль */
    *pPORTHIO_CLEAR = PH2;	/* ставим в ноль */
    *pPORTHIO_DIR = PH2;
#endif
    TIMER2_init();		/* Запускаем вспомогательный таймер - самый первый! В этой функции!!! */
    SPI1_init();		/* Подключенные по SPI1 устройства */
    SPORT0_config();		/* СПОРТ0  */
}

/**
 * Инициализация портов на атмеге
 * По-умолчанию все порты на входе в Z - состоянии (page 9 - 3)
 */
#pragma section("FLASH_code")
void init_atmega_ports(void)
{
#if 0
    /* Сбросим и уберем reset с экспандера PH0 */
    *pPORTH_FER &= ~PH0;	/* Отключаем функции */
    *pPORTHIO_CLEAR = PH0;	/* Делаем 0 на выход */
    *pPORTHIO_DIR |= PH0;	/* Делаем их на выход */
    ssync();
    *pPORTHIO_SET = PH0;	/* Делаем 1 на выход */
    ssync();
#endif

    delay_ms(WAIT_START_ATMEGA);	/* Задержка, т.к. Наш DSP очень быстрый - не успевают инициализироваца Atmega регистры!!! */


    pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* направления порта B - только WUSB_EN_PIN и MISO для обмена, остальные в Z */
    pin_clr(USBEXT33_EN_PORT, USBEXT33_EN_PIN);	/* направления порта - все Z кроме выход USBEXT33 */
    pin_clr(GATEBURN_PORT, GATEBURN_PIN);	/* направления порта A - все в Z - кроме GATEBURN (на нем 0) закрыть реде пережига */
    pin_set(USB_EN_PORT, USB_EN_PIN);	/* Порты D в Z состояние, кроме USB_EN_PIN  и GPS_EN_PIN */
    pin_set(UART_SELA_PORT, UART_SELA_PIN);	/* Установить порты E в z состояние кроме следующих, на модем */
    pin_clr(APWR_EN_PORT, APWR_EN_PIN);	/* Установим порты  - чтобы не сбросить!!! */

    pin_clr(EXP_MISO_PORT, EXP_MISO_PIN);
    pin_clr(GPS_EN_PORT, GPS_EN_PIN);
    pin_set(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    pin_set(FT232_RST_PORT, FT232_RST_PIN);	/* 1 на FT232 */
    pin_set(MES_MUX_SELA_PORT, MES_MUX_SELA_PIN);	/* На выход */
    pin_set(MES_MUX_SELB_PORT, MES_MUX_SELB_PIN);	/* На выход */

    /* Общяя чясть. Obscaja castj */
    LED_init();			/* Огоньки, внутри запускается експандер и SPI */
    RELE_init();		/* Иниц. всех реле */
}


#pragma section("FLASH_code")
static void set_status_bit(u8 bit)
{
    DEV_STATUS_STRUCT status;
    cmd_get_dsp_status(&status);
    status.ports |= bit;
    cmd_set_dsp_status(&status);
}


#pragma section("FLASH_code")
static void clr_status_bit(u8 bit)
{
    DEV_STATUS_STRUCT status;
    cmd_get_dsp_status(&status);
    status.ports &= ~bit;
    cmd_set_dsp_status(&status);
}



/**
 * Переключить на сильвану портом PD0 експандера
 */
#pragma section("FLASH_code")
void select_gps_module(void)
{
    pin_set(GPS_EN_PORT, GPS_EN_PIN);
    set_status_bit(RELE_GPS_BIT);
}

/**
 * Выключить сильвану
 */
#pragma section("FLASH_code")
void unselect_gps_module(void)
{
    pin_clr(GPS_EN_PORT, GPS_EN_PIN);
    clr_status_bit(RELE_GPS_BIT);
}


/**
 * Включить реле модема
 */
#pragma section("FLASH_code")
void modem_on(void)
{
    RELE_on(RELEAM);
    set_status_bit(RELE_MODEM_BIT);
}


/**
 * Выключить модем
 */
#pragma section("FLASH_code")
void modem_off(void)
{
    RELE_off(RELEAM);
    clr_status_bit(RELE_MODEM_BIT);
}


/**
 * Включить пережиг
 */
#pragma section("FLASH_code")
void burn_wire_on(void)
{
    pin_set(GATEBURN_PORT, GATEBURN_PIN);
    set_status_bit(RELE_BURN_BIT);
}

/**
 * Выключить пережиг
 */
#pragma section("FLASH_code")
void burn_wire_off(void)
{
    pin_clr(GATEBURN_PORT, GATEBURN_PIN);
    clr_status_bit(RELE_BURN_BIT);
}


/**
 * Включить аналоговую часть
 */
#pragma section("FLASH_code")
void select_analog_power(void)
{
#if QUARTZ_CLK_FREQ==(8192000)
    TIMER3_enable();
#endif
    pin_set(APWR_EN_PORT, APWR_EN_PIN);
    set_status_bit(RELE_ANALOG_POWER_BIT);
}

/**
 * Выключить аналоговую часть
 */
#pragma section("FLASH_code")
void unselect_analog_power(void)
{
    pin_clr(APWR_EN_PORT, APWR_EN_PIN);
    clr_status_bit(RELE_ANALOG_POWER_BIT);
    TIMER3_disable();
}


/**
 * Переключить на UART - 0 для UART
 */
#pragma section("FLASH_code")
void select_debug_module(void)
{
    pin_clr(UART_SELA_PORT, UART_SELA_PIN);
    set_status_bit(RELE_DEBUG_MODULE_BIT);
    clr_status_bit(RELE_MODEM_MODULE_BIT);
}


/**
 * Переключить на аналоговый модем
 */
#pragma section("FLASH_code")
void select_modem_module(void)
{
    pin_set(UART_SELA_PORT, UART_SELA_PIN);
    set_status_bit(RELE_MODEM_MODULE_BIT);
    clr_status_bit(RELE_DEBUG_MODULE_BIT);
}

/**
 * Отключить UART
 */
#pragma section("FLASH_code")
void unselect_debug_uart(void)
{
    pin_hiz(USB_EN_PORT, USB_EN_PIN);	/* Включаем направление */
    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* Выключаем ft232  */
    clr_status_bit(RELE_MODEM_MODULE_BIT);
    clr_status_bit(RELE_DEBUG_MODULE_BIT);
}




/**
 * Подключить беспроводной USB, возвращает успех или нет
 * Успех - есть WUSB, 0 - подключено через проводок
 */
#pragma section("FLASH_code")
bool wusb_on(void)
{
    u8 pin;
    bool ret;


    pin_clr(USBEXT33_EN_PORT, USBEXT33_EN_PIN);
    pin_clr(USB_EN_PORT, USB_EN_PIN);	/* Включение питания USB */


    pin_clr(HUB_RST_PORT, HUB_RST_PIN);	/* Сброс Хаба  */
    delay_ms(5);			/* Задержка */

    pin_set(HUB_RST_PORT, HUB_RST_PIN);	/* Сброс Хаба  */
    delay_ms(250);			/* Задержка */
    /* Определим, есть ли внешнее питание с USB(если 0), если есть 1 - WUSB */
    pin = pin_get(USB_VBUSDET_BUF_INPUT_PORT);
    ret = ((pin & (1 << USB_VBUSDET_BUF_PIN)) ? true : false);


    // всегда включаем
    if ((pin & (1 << USB_VBUSDET_BUF_PIN)) == 0) {
	pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* Убираем питание с Alereon - нет WUSB */
        set_status_bit(RELE_USB_BIT);
        clr_status_bit(RELE_WUSB_BIT);
    } else {
	pin_set(WUSB_EN_PORT, WUSB_EN_PIN);	/* Подаем питание на Alereon - есть WUSB */
	clr_status_bit(RELE_USB_BIT);
        set_status_bit(RELE_WUSB_BIT);
    }

    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* Reset  */
    delay_ms(250);
    pin_set(FT232_RST_PORT, FT232_RST_PIN);	/* Убираем reset  */
    delay_ms(125);

    return ret;
}

/**
 * Выключить беспроводной USB, и все остальные USB
 */
#pragma section("FLASH_code")
void wusb_off(void)
{
    pin_clr(WUSB_EN_PORT, WUSB_EN_PIN);	/* Выключаем WUSB_EN_PIN в "0" */
    pin_set(USB_EN_PORT, USB_EN_PIN);	/* Выключаем USB */
    pin_clr(FT232_RST_PORT, FT232_RST_PIN);	/* Выключаем ft232  */
    pin_set(UART_SELA_PORT, UART_SELA_PIN);	/* Отключить UART. Отключить все выводы */

    pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);	/* Вешаем в Z */
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);	/* Вешаем в Z */
    pin_clr(HUB_RST_PORT, HUB_RST_PIN);	/* Вешаем в 0 */
    clr_status_bit(RELE_USB_BIT);
    clr_status_bit(RELE_WUSB_BIT);
}


/**
 * SD карта вставлена в слот?
 */
#pragma section("FLASH_code")
bool check_sd_card(void)
{
    u8 res;

    /* кард детект */
    res = (*pPORTGIO & PG1);

    if (res)
	pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);
    else
	pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);

    return res;
}

/**
 * Включить SD карту 
 * SD карта подключена к BF
 */
#pragma section("FLASH_code")
void select_sdcard_to_bf(void)
{
    pin_clr(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    delay_ms(100);
    pin_clr(SD_EN_PORT, SD_EN_PIN);
    delay_ms(10);
    pin_set(AT_SD_WP_PORT, AT_SD_WP_PIN);
    delay_ms(100);
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);
    delay_ms(10);
    pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);
    delay_ms(50);
}


/**
 * Включить SD карту к кардридеру. SD карта подключена к кардридеру
 */
#pragma section("FLASH_code")
void select_sdcard_to_cr(void)
{
    pin_clr(USB_EN_PORT, USB_EN_PIN);	/* 0 - включено */
    pin_set(HUB_RST_PORT, HUB_RST_PIN);	/* Убираем reset с HUB */
    delay_ms(5);
    pin_set(SD_SRCSEL_PORT, SD_SRCSEL_PIN);
    pin_clr(SD_EN_PORT, SD_EN_PIN);
    pin_clr(AT_SD_WP_PORT, AT_SD_WP_PIN);
    pin_set(AT_SD_CD_PORT, AT_SD_CD_PIN);
    delay_ms(5);
    pin_clr(AT_SD_CD_PORT, AT_SD_CD_PIN);
}


/**
 * Ресет старого модема
 */
#pragma section("FLASH_code")
void old_modem_reset(void)
{
    pin_clr(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);

    pin_set(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);

    pin_clr(AM_RST_PORT, AM_RST_PIN);
    delay_ms(250);
    LED_toggle(LED_GREEN);
}
