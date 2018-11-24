#ifndef _VERSION_H
#define _VERSION_H

#include "globdefs.h"

/* Меняем только здесь */
#define __MY_VER__	1
#define __MY_REV__	116

#ifndef _WIN32			/* Embedded platform */
	section("FLASH_data") static const unsigned char  prog_ver = __MY_VER__; /* Версия программы */
	section("FLASH_data") static const unsigned short prog_rev = __MY_REV__; /* Ревизия программы */
#else
	const unsigned char  prog_ver = __MY_VER__; /* Версия программы */
	const unsigned short prog_rev = __MY_REV__; /* Ревизия программы == */
#endif


/**
 *  Возвращает версию модуля
 */
IDEF unsigned char get_version(void)
{
    return prog_ver; 	/* Версия программы */
}



/**
 *  Возвращает ревизию модуля
 */
IDEF unsigned short get_revision(void)
{
    return prog_rev; 	/* Версия программы */
}



#endif /* version.h */

