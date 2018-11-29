/* На TWI (I2C) работают:
 * 1) RTC
 * 2) датчик температуры и давления
 * 3) акселерометр и компас
 * + могут появиться другие устройства!
 * NB - в функции записи пакета неправильно стояли делители частоты! TWI работал "на пределе!"
 */

#include "utils.h"
#include "pll.h"
#include "twi.h"

////vvvvv:
/* #define PRESCALER_VALUE 	5  */
#define PRESCALER_VALUE 	(TIMER_FREQ_MHZ / 10 + 1)	/* PRESCALER = SCLK / 10MHz  */

#define RESET_TWI 		0	/* RESET_TWI value for controller */
#define CLKDIV_HI 		16	/* SCL high period */
#define CLKDIV_LO 		17	/* SCL low period */

/** 
 * Инициализация TWI, он не подключен к GPIO :( - просто делаем сброс.
 * изменение регистра контроля делаем в чтении и записи-для разных микросхем
 * там будут стоят разные биты 
 */
#pragma section("FLASH_code")
static void _twi_reset(void)
{
    *pTWI_CONTROL = RESET_TWI;	/* reset TWI controller */
    *pTWI_MASTER_STAT = BUFWRERR | BUFRDERR | LOSTARB | ANAK | DNAK;	/* clear errors before enabling TWI */
    *pTWI_INT_STAT = SINIT | SCOMP | SERR | SOVF | MCOMP | MERR | XMTSERV | RCVSERV;	/* clear interrupts before enabling TWI */
    *pTWI_FIFO_CTL = XMTFLUSH | RCVFLUSH;	/* flush rx and tx fifos */
    ssync();
}

/**
 * Стоп TWI - опрашиваем раз в секунду, все остальное время должен быть выключен
 */
#pragma section("FLASH_code")
void TWI_stop(void)
{
    *pTWI_CONTROL = RESET_TWI;	/* reset TWI controller */
     ssync();
}

/** 
 * Запись в регистр по TWI в режыме мастера 
 * Возвращает успех или нет
 */
#pragma section("FLASH_code")
bool TWI_write_pack(u16 addr, u8 reg, u8 * pointer, u16 count, const void* par)
{
    int i;
    bool res = true;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

    _twi_reset();			/* Сбрасываем интерфейс TWI */

    *pTWI_FIFO_CTL = 0;		/* clear the bit manually */
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* Передаем команду, что мы хотим записать регистр reg */
    *pTWI_MASTER_CTL = (count + 1 << 6) | MEN;	/* Старт передачи малой скорости, 1 байт регистр + count передаем */
    ssync();


    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    res = false;
	    goto met;
	}
    }

    *pTWI_XMT_DATA8 = reg;	/* номер регистра */
    ssync();


    /* # of transfers before stop condition */
    for (i = 0; i < count; i++) {

	t0 = get_msec_ticks();
	while (*pTWI_FIFO_STAT == XMTSTAT) {	/* wait to load the next sample into the TX FIFO */
	    ssync();
	    if (get_msec_ticks() - t0 > 50) {
		res = false;
		goto met;
	    }
	}
	*pTWI_XMT_DATA8 = *pointer++;	/* load the next sample into the TX FIFO */
	ssync();
    }

    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    res = false;
	    goto met;
	}
    }
  met:
    asm("nop;");
    return res;
}


/* Чтение по TWI в реж. мастера */
#pragma section("FLASH_code")
bool TWI_read_pack(u16 addr, u8 reg, u8 * pointer, u16 count, const void* par)
{
    int i;
    bool res = false;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }


    _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* адрес (7-бит + бит чтение/запись) */
    *pTWI_XMT_DATA8 = reg;	/* Начальный адрес регистра  */
    *pTWI_MASTER_CTL = (1 << 6) | MEN;	/* Старт передачи */
    ssync();

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();

	/* Адрес не подтвержден - нет устройтства */
	if (get_msec_ticks() - t0 > 10 || *pTWI_MASTER_STAT & ANAK) {
	    goto met;
	}
    }

/*     _twi_reset();  */
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 100 */
    *pTWI_MASTER_ADDR = addr;	/* адрес (7-бит + бит чтение/запись) */
    *pTWI_MASTER_CTL = (count << 6) | MEN | MDIR;	/* Старт приема */
    ssync();


    /* for each item. Ставим таймаут на 5 мс */
    for (i = 0; i < count; i++) {
	t0 = get_msec_ticks();

	/* wait for data to be in FIFO */
	while (*pTWI_FIFO_STAT == RCV_EMPTY) {
	    ssync();
	    if (get_msec_ticks() - t0 > 50) {
		goto met;
	    }
	}

	*pointer++ = *pTWI_RCV_DATA8;	/* read the data */
	ssync();
    }

    res = true;			/* Все прочитали  */
  met:
    /* service TWI for next transmission */
    *pTWI_INT_STAT = RCVSERV | MCOMP;
    ssync();
    return res;
}


/**
 * Читаем 2 байта из датчика
 * Передаем команду, что мы хотим прочитать регистра reg
 */
#pragma section("FLASH_code")
u8 TWI_read_byte(u16 addr, u8 reg, const void* par)
{
    int i;
    u8 byte;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

     _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* адрес (7-бит + бит чтение/запись) */
    *pTWI_MASTER_CTL = (1 << 6) | MEN;	/* Старт передачи */
     ssync();

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }

    *pTWI_XMT_DATA8 = reg;

    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_MASTER_STAT & MPROG) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }


    _twi_reset();
    *pTWI_FIFO_CTL = 0;
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* адрес (7-бит + бит чтение/запись) */
    *pTWI_MASTER_CTL = (1 << 6) | MEN | MDIR;	/* start transmission */
    ssync();


    /* wait for data to be in FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == RCV_EMPTY) {
	ssync();
	if (get_msec_ticks() - t0 > 50) {
	    goto met;
	}
    }

    byte = *pTWI_RCV_DATA8;	/* read the data */
    ssync();

  met:
    /* service TWI for next transmission */
    *pTWI_INT_STAT = RCVSERV | MCOMP;
    asm("nop;");
    asm("nop;");
    asm("nop;");
    return byte;
}


/* Запись байта */
#pragma section("FLASH_code")
void TWI_write_byte(u16 addr, u16 reg, u8 data, const void* par)
{
    int i;
    u64 t0;
    u16 lo = CLKDIV_LO, hi = CLKDIV_HI;
    twi_clock_div* div;

    if(par != 0) {
       div = (twi_clock_div*)par;
       lo = div->lo_div_clk;
       hi = div->hi_div_clk;
    }

    _twi_reset();			/* Сбрасываем интерфейс TWI */
    *pTWI_FIFO_CTL = 0;		/* clear the bit manually */
    *pTWI_CONTROL = TWI_ENA | PRESCALER_VALUE;	/* Регистр TWI_CONTROL, прескалер = 48 MHz / 10 MHz + разрешить TWI */

    *pTWI_CLKDIV = (hi << 8) | lo;	/* Делитель для CLK = 100 кГц ~ 25 */
    *pTWI_MASTER_ADDR = addr;	/* адрес (7-бит + бит чтение/запись) */
    *pTWI_MASTER_CTL = (2 << 6) | MEN;	/* Старт передачи (пока на малой скорости), 1 байт регистр + count передаем */
    ssync();



    /* wait to load the next sample into the TX FIFO */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }

    *pTWI_XMT_DATA8 = reg;	/* номер регистра */
    ssync();

    /* # of transfers before stop condition */
    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }

    *pTWI_XMT_DATA8 = data;	/* load the next sample into the TX FIFO */
    ssync();

    t0 = get_msec_ticks();
    while (*pTWI_FIFO_STAT == XMTSTAT) {	/* wait to load the next sample into the TX FIFO */
	ssync();
	if (get_msec_ticks() - t0 > 50)
	    goto met;
    }
  met:
    asm("nop;");
}
