#include <string.h>
#include "utils.h"
#include "bmp085.h"
#include "twi.h"

#define 	BMP085_ADDR		(0xEE >> 1)
#define 	BMP085_TWI_DIV_HI	16
#define 	BMP085_TWI_DIV_LO       17
#define 	BMP085_NUM_MOD		0

/* Настройки интерфейса TWI */
section("FLASH_data")
static const twi_clock_div bmp085_val = {
    .lo_div_clk = BMP085_TWI_DIV_LO,
    .hi_div_clk = BMP085_TWI_DIV_HI,
    .rsvd = BMP085_NUM_MOD
};


/* Записано в eeprom датчика - можно забить во FLASH */
static struct bmp085_calibr_pars {
    s16 ac1;
    s16 ac2;
    s16 ac3;

    u16 ac4;
    u16 ac5;
    u16 ac6;

    s16 b1;
    s16 b2;
    s16 mb;
    s16 mc;
    s16 md;
    bool init;
} pars;


/**
 * Инициализация, потом заменить на чтение сразу всей пачки
 */
#pragma section("FLASH_code")
bool bmp085_init(void)
{
    bool res;

    do {
	// Внутри pars все поставлено в 0
	memset(&pars, 0, sizeof(struct bmp085_calibr_pars));
	pars.init = false;

	/* Читаем сначала калибровочные коэффициенты */
	res = TWI_read_pack(BMP085_ADDR, 0xaa, (u8 *) & pars.ac1, 2, &bmp085_val);
	pars.ac1 = byteswap2(pars.ac1);
	if (pars.ac1 == 0 || pars.ac1 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xac, (u8 *) & pars.ac2, 2, &bmp085_val);
	pars.ac2 = byteswap2(pars.ac2);
	if (pars.ac2 == 0 || pars.ac2 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xae, (u8 *) & pars.ac3, 2, &bmp085_val);
	pars.ac3 = byteswap2(pars.ac3);
	if (pars.ac3 == 0 || pars.ac3 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xb0, (u8 *) & pars.ac4, 2, &bmp085_val);
	pars.ac4 = byteswap2(pars.ac4);
	if (pars.ac4 == 0 || pars.ac4 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xb2, (u8 *) & pars.ac5, 2, &bmp085_val);
	pars.ac5 = byteswap2(pars.ac5);
	if (pars.ac5 == 0 || pars.ac5 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xb4, (u8 *) & pars.ac6, 2, &bmp085_val);
	pars.ac6 = byteswap2(pars.ac6);
	if (pars.ac6 == 0 || pars.ac6 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xb6, (u8 *) & pars.b1, 2, &bmp085_val);
	pars.b1 = byteswap2(pars.b1);
	if (pars.b1 == 0 || pars.b1 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xb8, (u8 *) & pars.b2, 2, &bmp085_val);
	pars.b2 = byteswap2(pars.b2);
	if (pars.b2 == 0 || pars.b2 == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xba, (u8 *) & pars.mb, 2, &bmp085_val);
	pars.mb = byteswap2(pars.mb);
	if (pars.mb == 0 || pars.mb == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xbc, (u8 *) & pars.mc, 2, &bmp085_val);
	pars.mc = byteswap2(pars.mc);
	if (pars.mc == 0 || pars.mc == 0xffff || res == false)
	    break;

	res = TWI_read_pack(BMP085_ADDR, 0xbe, (u8 *) & pars.md, 2, &bmp085_val);
	pars.md = byteswap2(pars.md);
	if (pars.md == 0 || pars.md == 0xffff || res == false)
	    break;

	pars.init = true;
    } while (0);


    return pars.init;
}


/* Получить данные */
#pragma section("FLASH_code")
bool bmp085_data_get(int *temp, int *press)
{
    u32 ut;
    long x1, x2, b5;
    s32 x3, b3, b6, p;
    u32 b4, b7;
    u16 up;
    u8 data;
    bool res = false;


    do {
	// не запущен!
	if (!pars.init)
	    break;

	/* Получим температуру - 2 байта */
	data = 0x2e;
	TWI_write_pack(BMP085_ADDR, 0xf4, &data, 1, &bmp085_val);
	delay_ms(5);		// ждем 5 мс и читаем из F6 и F7
	res = TWI_read_pack(BMP085_ADDR, 0xf6, (u8 *) & ut, 2, &bmp085_val);
	if (res == false)
	    break;

	ut = byteswap2(ut);

	/* Получим давление - 2 байта */
	data = 0x34;
	TWI_write_pack(BMP085_ADDR, 0xf4, &data, 1, &bmp085_val);
	delay_ms(5);
	res = TWI_read_pack(BMP085_ADDR, 0xf6, (u8 *) & up, 2, &bmp085_val);
	if (res == false)
	    break;

	up = byteswap2(up);

	if (up == 0 || ut == 0 || up == 0xffff || ut == 0xffff) {
	    break;
	}

	/* расчет температуры */
	x1 = (ut - pars.ac6) * pars.ac5 >> 15;
	x2 = (pars.mc << 11) / (x1 + pars.md);
	b5 = (x1 + x2);


	if (temp != NULL)
	    *temp = (b5 + 8) >> 4;

	/* расчет давления */
	b6 = b5 - 4000;
	x1 = (pars.b2 * (b6 * b6 >> 12)) >> 11;
	x2 = pars.ac2 * b6 >> 11;
	x3 = x1 + x2;
	b3 = ((int32_t) pars.ac1 * 4 + x3 + 2) >> 2;
	x1 = pars.ac3 * b6 >> 13;
	x2 = (pars.b1 * (b6 * b6 >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;
	b4 = (pars.ac4 * (uint32_t) (x3 + 32768)) >> 15;
	b7 = ((uint32_t) up - b3) * 50000;

	p = ((b7 < 0x80000000) ? ((b7 * 2) / b4) : ((b7 / b4) * 2));

	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;

	if (press != NULL)
	    *press = p + ((x1 + x2 + 3791) >> 4);
	res = true;
    } while (0);

    return res;
}
