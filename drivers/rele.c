#include "xpander.h"
#include "utils.h"
#include "rele.h"

#define RELE_PORT	PORTD

/**
 * Реле-burn2 в "0" пока! 
 */

static struct {
     bool rele_pow;
     bool rele_am;
     bool rele_burn;
     u8   rsvd;
} rele_status_struct;

/* Инициализация реле */
#pragma section("FLASH_code")
void RELE_init(void)
{
	/* Ставим порты D на выход */
	pin_set(RELEBURN_CS_PORT, RELEBURN_CS_PIN);
	pin_set(RELEAM_CS_PORT,   RELEAM_CS_PIN);
	pin_set(RELEPOW_CS_PORT,  RELEPOW_CS_PIN);
	pin_clr(RELE_SET_PORT,RELE_SET_PIN);	
	pin_clr(RELE_CLR_PORT,RELE_CLR_PIN);

	rele_status_struct.rele_pow = false;
	rele_status_struct.rele_am = false;
	rele_status_struct.rele_burn = false;
}


/**
 * Включение реле, должен быть УЖЕ включен таймер2 и инициализирован XPANDER !
 * параметр: Какое реле
 * возврат:  успех или нет
 */
#pragma section("FLASH_code")
bool RELE_on(RELE_TYPE_ENUM rele)
{
	u8 cs;

	switch (rele) {
	case RELEPOW:
		cs = RELEPOW_CS_PIN;
		rele_status_struct.rele_pow = true;
		break;
	case RELEAM:
		cs = RELEAM_CS_PIN;
		rele_status_struct.rele_am = true;
		break;
	case RELEBURN:
		cs = RELEBURN_CS_PIN;
		rele_status_struct.rele_burn = true;
		break;
	default:
		return false;
	}

	pin_clr(RELE_SET_PORT, RELE_SET_PIN);	/* Ставим SET в 0  */
	pin_set(RELE_CLR_PORT, RELE_CLR_PIN);	/* Ставим CLR в 1  */
	pin_clr(RELE_PORT, cs);			/* CS вниз, все на порте D  */
	delay_ms(15);				/* Задержка для срабатывания поляр реле */                              	
	pin_set(RELE_PORT, cs); 		/* CS вверх, все на порте D  */
	pin_set(RELE_SET_PORT, RELE_SET_PIN);	/* Set в 1  */
	pin_set(RELE_CLR_PORT, RELE_CLR_PIN);	/* Ставим CLR в 1  */
	return true;
}


/**
 * Выключение реле, должен быть УЖЕ включен таймер2 и инициализирован XPANDER !
 * параметр: Какое реле
 * возврат:  успех или нет
 */
#pragma section("FLASH_code")
bool RELE_off(RELE_TYPE_ENUM rele)
{
	u8 cs;
return 1;
	switch (rele) {
	case RELEPOW:
		cs = RELEPOW_CS_PIN;
		rele_status_struct.rele_pow = false;
		break;
	case RELEAM:
		cs = RELEAM_CS_PIN;
		rele_status_struct.rele_am = false;
		break;
	case RELEBURN:
		cs = RELEBURN_CS_PIN;
		rele_status_struct.rele_burn = false;
		break;
	default:
		return false;
	}

	pin_set(RELE_SET_PORT, RELE_SET_PIN);	/* Set в 1  */
	pin_clr(RELE_CLR_PORT, RELE_CLR_PIN);	/* Ставим CLR в 0  */
	pin_clr(RELE_PORT, cs);			/* CS вниз */
	delay_ms(15);				/* Задержка для срабатывания поляр реле */
	pin_set(RELE_PORT, cs);			/* CS вверх */
	pin_set(RELE_CLR_PORT, RELE_CLR_PIN);	/* Ставим CLR в 1  */
	pin_set(RELE_SET_PORT, RELE_SET_PIN);	/* Set в 1  */


	return true;
}


/**
 * Реле пережига включено?
 */
#pragma section("FLASH_code")
bool rele_burn_is_on(void)
{
   return rele_status_struct.rele_burn;
}

/**
 * Реле модема включено?
 */
#pragma section("FLASH_code")
bool rele_modem_is_on(void)
{
   return rele_status_struct.rele_am;
}
