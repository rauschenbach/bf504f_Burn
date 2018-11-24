#include "modem.h"
#include "utils.h"
#include "am0.h"
#include "am3.h"
#include "amt.h"
#include "log.h"


/** 
 * Инициализировать модем, в зависимости от типа
 */
#pragma section("FLASH_code")
int modem_init_all_types(void *par)
{
    int res = -1;

    if (par != NULL) {

	switch (((GNS110_PARAM_STRUCT *) par)->gns110_modem_type) {

	    /* Старый модем */
	case GNS110_MODEM_OLD:
	    res = am0_prg_modem(par);
	    break;

	    /* Модем am3 */
	case GNS110_MODEM_AM3:
	    res = am3_prg_modem(par);
	    break;

	    /* Модем бентос */
	case GNS110_MODEM_BENTHOS:
	    res = amt_prg_modem(par);
	    break;

	    /* Нет модема */
	default:
	    res = 0;
	    break;
	}
    }
    return res;
}


/**
 * Включить радио - пока только доля AM3
 */
#pragma section("FLASH_code")
int modem_set_radio(void *par)
{
    int res = -1;

    if (par != NULL) {


	switch (((GNS110_PARAM_STRUCT *) par)->gns110_modem_type) {

	    /* Старый модем */
	case GNS110_MODEM_OLD:
	    res = 0;
	    break;

	    /* Модем am3 */
	case GNS110_MODEM_AM3:
	    res = am3_set_radio(par);
	    break;

	    /* Модем бентос */
	case GNS110_MODEM_BENTHOS:
	    res = amt_set_radio(par);
	    break;

	    /* Нет модема */
	default:
	    res = 0;
	    break;
	}
    }
    return res;
}

/**
 * Узнать время модема am3
 */
int  modem_check_modem_time(void* par)
{
    int res = -1;
    TIME_DATE td;

    if (par != NULL) {

	switch (((GNS110_PARAM_STRUCT *) par)->gns110_modem_type) {


         /* Модем am3 */
	case GNS110_MODEM_AM3:
	  if (am3_init() < 0) {
		return -1;
	  }
	 /* Уже время в секундах */
         res = am3_get_curr_time(&td);

         am3_close();
    	break;


	default:
	    break;
	}

   }
    return res;
}

/** 
 * Проверить параметры модема, в зависимости от типа
 */
#pragma section("FLASH_code")
int modem_check_params(void *par)
{
    int res = -1;

    if (par != NULL) {

	switch (((GNS110_PARAM_STRUCT *) par)->gns110_modem_type) {

	    /* Модем am3 */
	case GNS110_MODEM_AM3:
	    res = am3_check_modem(par);
	    break;

	default:
	    break;
	}
    }
    return res;
}


/** 
 * Перетранслировать команду в модем
 */
int modem_convey_buf(void *cmd, int size)
{
    int res;
    res = am3_convey_buf(cmd, size);
    return res;
}

