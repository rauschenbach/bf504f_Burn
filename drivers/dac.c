/* Драйвер для микросхемы AD5620 - для тестирования АЦП 
 * В ждучих функциях сделать выход по таймауту!!! */
#include "dac.h"
#include "xpander.h"
#include "utils.h"
#include "spi1.h"


/*************************************************************
 * У нас неправильная микросхема: 14-ти битная AD5640 
 * Напряжение на выходе: Vout = 2.5 * D / 16384 
 ************************************************************/

/* Инициализировать все даки  */
#pragma section("FLASH_code")
void DAC_init(void)
{
	pin_set(DAC192MHZ_CS_PORT, DAC192MHZ_CS_PIN);	/* Сначала 19.2 в единицу. и напрвление  */
#if QUARTZ_CLK_FREQ==(19200000)
	pin_set(DAC4MHZ_CS_PORT, DAC4MHZ_CS_PIN);	/* 4.096 в единицу. и напрвление  */
/*#elif QUARTZ_CLK_FREQ==(8192000)*/
/*	pin_set(DAC_TEST_CS_PORT, DAC_TEST_CS_PIN);*/	/* тестовый в единицу. и напрвление  */
#endif

}


/* Записать число в ЦАП, xpander и SPI1 уже должны быть настроены */
#pragma section("FLASH_code")
void DAC_write(DAC_TYPE_ENUM type, u16 cmd)
{
	u8 port, pin, dir;

	/* Какой у нас DAC?  */
	switch (type) {

		/* для кварца 19.2 мгц */
	case DAC_19MHZ:
		port = DAC192MHZ_CS_PORT;
		pin = DAC192MHZ_CS_PIN;
		dir = DAC192MHZ_CS_DIR;
		break;

		/* тестовый цап */
	case DAC_TEST:
		port = DAC_TEST_CS_PORT;
		pin = DAC_TEST_CS_PIN;
		dir = DAC_TEST_CS_DIR;
		break;

		/* для кварца 4 мгц */
	case DAC_4MHZ:
	default:
		port = DAC4MHZ_CS_PORT;
		pin = DAC4MHZ_CS_PIN;
		dir = DAC4MHZ_CS_DIR;
		break;
	}

	/* Поставить CS нулем */
	pin_clr(port, pin);
	
	delay_ms(5);		/* возможно нужна задежка - CS может перекрыца с тактовым сигналом */

	SPI1_write_read(cmd & 0x3fff);	/* CS выбран - посылаем команду по DMA7 */
	pin_set(port, pin);		/* Убираем CS на експандере 1 вверх */
}
