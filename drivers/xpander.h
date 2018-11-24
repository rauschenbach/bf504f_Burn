/************************************************************************************************
 * Дефиниции для переключения экспандера (на разных портах) против часовой стрелки по схеме
 ************************************************************************************************/
#ifndef _XPANDER_H
#define _XPANDER_H

#include "globdefs.h"

/**************************************************************************** 
 * Дефиниции для портов из файла <avr/iom165pa.h>,
 * в случае конфликтов - перенести внутрь c-файла 
 *****************************************************************************/
#define PINA    0x20
#define DDRA    0x21
#define PORTA   0x22

#define PINB    0x23
#define DDRB    0x24
#define PORTB   0x25

#define PINC    0x26
#define DDRC    0x27
#define PORTC   0x28

#define PIND    0x29
#define DDRD    0x2A
#define PORTD   0x2B

#define PINE    0x2C
#define DDRE    0x2D
#define PORTE   0x2E

#define PINF    0x2F
#define DDRF    0x30
#define PORTF   0x31

#define PING    0x32		/* Всего шесть пинов!!!  */
#define DDRG    0x33		/* Всего пять направлений!!! */
#define PORTG   0x34		/* Всего пять выходов!!! */

#define PRR	 0x64		/* Регистр power reduction  */
#define PRADC	 0
#define PRUSART0 1
#define PRSPI    2
#define PRTIM1   3


/* АЦП  */
#define ADMUX	0x7C
#define DIDR1	0x7F
#define DIDR0	0x7E
#define ADCSRA	0x7A
#define ADCSRB	0x7B
#define ADCH	0x79
#define ADCL	0x78


#define PIN0   0
#define PIN1   1
#define PIN2   2
#define PIN3   3
#define PIN4   4
#define PIN5   5
#define PIN6   6
#define PIN7   7



/************************************************************************************************
 * Дефиниции для портов по алфавиту
 ************************************************************************************************/

/* 1. --------------------- Порты A --------------------- */
#define		MES_MUX_SELA_PIN	PIN0	/* На выход */
#define		MES_MUX_SELA_PORT	PORTA
#define		MES_MUX_SELA_DIR	DDRA


#define		MES_MUX_SELB_PIN	PIN3	/* На выход */
#define		MES_MUX_SELB_PORT	PORTA
#define		MES_MUX_SELB_DIR	DDRA


#define		FT232_RST_PIN		PIN1
#define		FT232_RST_PORT		PORTA
#define		FT232_RST_DIR		DDRA

#define		USB_IS_ENA_PIN		PIN2
#define		USB_IS_ENA_INPUT_PORT	PINA	// чтение
#define		USB_IS_ENA_DIR		DDRA



#define		SD_EN_PIN		PIN4
#define		SD_EN_PORT		PORTA
#define		SD_EN_DIR		DDRA

#define		AM_RST_PIN		PIN5
#define		AM_RST_PORT		PORTA
#define		AM_RST_DIR		DDRA

#define		GATEBURN_PIN		PIN6
#define		GATEBURN_PORT		PORTA
#define		GATEBURN_DIR		DDRA

#define		USB_VBUSDET_BUF_PIN	PIN7
#define		USB_VBUSDET_BUF_INPUT_PORT	PINA	// чтение
#define		USB_VBUSDET_BUF_DIR	DDRA


/* 2. --------------------- Порты B --------------------- */
#define		EXP_SS_PIN		PIN0	/* чипселект-слейвселект SPI */
#define		EXP_SS_INPUT_PORT	PINB	// вход
#define		EXP_SS_DIR		DDRB

#define		EXP_SCK_PIN		PIN1	/* клок SPI */
#define		EXP_SCK_PORT		PORTB
#define		EXP_SCK_DIR		DDRB

#define		EXP_MOSI_PIN		PIN2	/* MOSI */
#define		EXP_MOSI_INPUT_PORT	PINB	// вход
#define		EXP_MOSI_DIR		DDRB

#define		EXP_MISO_PIN		PIN3	/* MISO */
#define		EXP_MISO_PORT		PORTB
#define		EXP_MISO_DIR		DDRB

#define		HUB_RST_PIN		PIN4
#define		HUB_RST_PORT		PORTB
#define		HUB_RST_DIR		DDRB

#define		RTC_IRQ_PIN		PIN6
#define		RTC_IRQ_INPUT_PORT	PINB	// вход
#define		RTC_IRQ_DIR		DDRB

#define		WUSB_EN_PIN		PIN7
#define		WUSB_EN_PORT		PORTB
#define		WUSB_EN_DIR		DDRB


/* 3. --------------------- Порты C --------------------- */
#define 	DAC4MHZ_CS_PIN    	PIN0
#define 	DAC4MHZ_CS_PORT	  	PORTC
#define 	DAC4MHZ_CS_DIR    	DDRC

#define		APWR_EN_PIN		PIN1	/* Analog POWER enable  */
#define		APWR_EN_PORT		PORTC
#define		APWR_EN_DIR		DDRC

#define 	USBEXT33_EN_PIN		PIN4
#define 	USBEXT33_EN_PORT	PORTC
#define 	USBEXT33_EN_DIR		DDRC

#define 	DAC192MHZ_CS_PIN	PIN5
#define 	DAC192MHZ_CS_PORT	PORTC
#define 	DAC192MHZ_CS_DIR	DDRC

#define 	DAC_TEST_CS_PIN		PIN6
#define 	DAC_TEST_CS_PORT	PORTC
#define 	DAC_TEST_CS_DIR		DDRG

#define 	_1282_RESET_PIN		PIN7
#define 	_1282_RESET_PORT	PORTC
#define 	_1282_RESET_DIR		DDRC


/* 4. --------------------- Порты D --------------------- */
#define 	GPS_EN_PIN		PIN0
#define 	GPS_EN_PORT		PORTD
#define 	GPS_EN_DIR		DDRD

#define 	LSM_DRDY_PIN		PIN1
#define 	LSM_DRDY_INPUT_PORT	PIND	// вход
#define 	LSM_DRDY_DIR		DDRD

#define 	RELEBURN_CS_PIN		PIN2
#define 	RELEBURN_CS_PORT	PORTD
#define 	RELEBURN_CS_DIR		DDRD

#define 	RELEAM_CS_PIN		PIN3
#define 	RELEAM_CS_PORT		PORTD
#define 	RELEAM_CS_DIR		DDRD

#define 	RELEPOW_CS_PIN		PIN4
#define 	RELEPOW_CS_PORT		PORTD
#define 	RELEPOW_CS_DIR		DDRD

#define		USB_EN_PIN		PIN5
#define		USB_EN_PORT		PORTD
#define		USB_EN_DIR		DDRD

#define 	RELE_CLR_PIN		PIN6
#define 	RELE_CLR_PORT		PORTD
#define 	RELE_CLR_DIR		DDRD

#define 	RELE_SET_PIN		PIN7
#define 	RELE_SET_PORT		PORTD
#define 	RELE_SET_DIR		DDRD

/* 5. --------------------- Порты E --------------------- */
#define		UART_SELA_PIN		PIN1
#define 	UART_SELA_PORT		PORTE
#define 	UART_SELA_DIR		DDRE

#define		UART_SELB_PIN		PIN2
#define 	UART_SELB_PORT		PORTE
#define 	UART_SELB_DIR		DDRE

#define		SD_SRCSEL_PIN		PIN3
#define		SD_SRCSEL_PORT		PORTE
#define		SD_SRCSEL_DIR		DDRE

#define		AT_SD_WP_PIN		PIN4
#define		AT_SD_WP_PORT		PORTE
#define		AT_SD_WP_DIR		DDRE

#define		AT_SD_CD_PIN		PIN5
#define		AT_SD_CD_PORT		PORTE
#define		AT_SD_CD_DIR		DDRE

/* 6. --------------------- Порты F --------------------- */

#define		ADC_PORT_PIN		(1 << PIN0) | (1 << PIN1) | (1 << PIN2) | (1 << PIN3) | (1 << PIN4)
#define		ADC_PORT_INPUT_PORT 	PINF
#define		ADC_PORT_DIR		DDRF

/* 7. --------------------- Порты G --------------------- */
#define		LED1_PIN		PIN0
#define		LED2_PIN		PIN1
#define		LED3_PIN		PIN2
#define		LED4_PIN		PIN3

#define		LED1_PORT		PORTG
#define		LED2_PORT		PORTG
#define		LED3_PORT		PORTG
#define		LED4_PORT		PORTG

#define		LED1_DIR		DDRG
#define		LED2_DIR		DDRG
#define		LED3_DIR		DDRG
#define		LED4_DIR		DDRG


/* Коэффициенты преобразования напряжения АЦП при референсе 3.3 Вольта 
 * для формулы:Umv = ADC * 3.3 / 1024 * 11 * 1000. Коэффициенты БЕЗ СКОБОК!!! */
#define		ATMEGA_AMEND_COEF		1	/* Поправочный коэффициент - учитывает падение на стабилитроне - только для U */
#define 	ATMEGA_VOLT_COEF 		36300 >> 10	/* Для напряжения U 3.3 * 11 * 1000 */
#define 	ATMEGA_AMPER_SUPPLY_COEF	330 >> 10	/* Для тока I 3.3 * 1000 */
#define 	ATMEGA_AMPER_BURN_COEF		3300 >> 10	/* Для тока I 3.3 * 1000 */
#define 	ATMEGA_AMPER_AM3_COEF		660 >> 10	/* Для тока I 3.3 * 1000 */



void adc_init(void*);		/* Настроить АЦП */

void pin_set(u8, u8);
void pin_clr(u8, u8);
void pin_hiz(u8, u8);
u8   pin_get(u8);
void adc_stop(void);		/* Выключение АЦП */
bool adc_get(void *);



#endif				/* xpander.h  */
