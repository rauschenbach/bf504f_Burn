/* Моргание лампочками */
#ifndef _LED_H
#define _LED_H

#include "globdefs.h"
#include "xpander.h"

/* Состояние и моргание лампочками */
#define		LED_OFF_STATE  		0
#define		LED_ON_STATE   		1
#define		LED_SLOW_STATE 		2
#define		LED_QUICK_STATE 	3
#define		LED_TEST_STATE 		4


/* По номерам */
#define 	LED1 			LED4_PIN
#define 	LED2 			LED3_PIN
#define 	LED3 			LED2_PIN
#define 	LED4 			LED1_PIN
#define 	LEDS_PORT		LED1_PORT

/* По цвету */
#define 	LED_GREEN		LED1
#define 	LED_YELLOW		LED2
#define 	LED_RED  		LED3
#define 	LED_BLUE  		LED4

/* По действию */
#define 	LED_SIGNAL		LED_YELLOW	/* Лампочка обработчика сигнала */
#define 	LED_POWER		LED_RED	/* Лампочка включения */



#define		NUM_LEDS		4	/* 4 лампы всего на плате */

/*******************************************************************
 *  function prototypes
 *******************************************************************/
void LED_init(void);
void LED_on(u8);
void LED_off(u8);
void LED_toggle(u8);
void LED_blink(void);
void LED_test(void);
void LED_set_state(u8, u8, u8, u8);
void LED_get_state(u8 *, u8 *, u8 *, u8 *);

u8 LED_get_power_led_state(void);
void LED_set_power_led_state(u8);

#define LED_all_off()    do { LED_off(LED1);LED_off(LED2);LED_off(LED3);LED_off(LED4);} while(0)
#define LED_all_on()    do { LED_on(LED1);LED_on(LED2);LED_on(LED3);LED_on(LED4);} while(0)



#endif				/* led.h */
