#ifndef _DAC_H
#define _DAC_H

#include "globdefs.h"


// Для 12 ти разрядного будет FFF !!!!! + сдвиг на 2 бита влево
// Для 14 ти разрядного будет 3FFF !!!!!
#define		DAC19_MAX_DATA			0x3fff
#define		DAC19_MIN_DATA			0
#define 	DAC19_INIT_DATA	 		(DAC19_MAX_DATA / 2)	/* Начальный коэффициент для 19 мгц - половина диапазона */

#define 	DAC4_INIT_DATA	 		11450	/* Начальные коэффициент для 4мгц */
#define		DAC4_MAX_DIFF			2500


/**************************************************************
 * 	Структуры и типы ЦАП, кот. присутствуют на плате
 *************************************************************/


/* Типы ЦАП, на кот. подаем сигналы  */
typedef enum {
  DAC_19MHZ = 0,
  DAC_4MHZ,
  DAC_TEST,
} DAC_TYPE_ENUM;


/* Что подается на каждый цап - сохраняется в flash */
typedef struct {
	u16 dac19_data;  /* На цап 19.2  */
	u16 dac19_coef;  /* дельта (коэффициент) */
	u16 dac4_data;   /* На цап 4  */
	u16 data_4_coef;  /* дельта (коэффициент) */
} DEV_DAC_STRUCT;

/* Запись в любой ЦАП */
void DAC_write(DAC_TYPE_ENUM, u16);
void DAC_init(void);

#endif /* dac.h */

