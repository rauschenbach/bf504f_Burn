/*******************************************************************
 * Analog Devices, Inc. All Rights Reserved.
 * Description:    This file tests the SD interface on the EZ-KIT.
*******************************************************************/
#include <stdio.h>
#include <time.h>
#include "utils.h"
#include "diskio.h"
#include "utils.h"
#include "rsi.h"
#include "ff.h"

/* Private functions ---------------------------------------------------------*/
/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
section("L1_code")
DWORD get_fattime (void)
{
  TIME_DATE t;	
  long sec;

  /*  Смотрим время  */
  sec = get_sec_ticks();
  sec_to_td(sec, &t);

  return	  ((DWORD)(t.year - 1980) << 25)		/* Год  */
			| ((DWORD)t.mon << 21)			/* Месяц:   	1...12  */
			| ((DWORD)t.day << 16)                  /* день:    	1...30  */
			| ((DWORD)t.hour << 11)   		/* часы:    	0...23  */
			| ((DWORD)t.min << 5)		        /* минуты:  	0...59  */
			| ((DWORD)(t.sec / 2) >> 1);		/* секунды * 2:	0...29  */
}

/**
 * Опрашивать состояние диска, и если ее вынули-показать неготовность
 */
section("L1_code")
DSTATUS disk_status (BYTE drv)
{
   if(!(*pPORTGIO & PG1))
       return STA_NOINIT;

   if (!(*pPORTGIO & PG0))
       return STA_PROTECT;	

    return 0;
}


/**
 * Инициализировать SD интерфейс-вызов функции перенес из других файлов ff.c
 * Не вставлена карта или какие-то проблемы?
 */
section("L1_code")
DSTATUS disk_initialize (BYTE drv)
{
	if(rsi_setup())
           return STA_NOINIT;
	return  0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
section("L1_code")
DRESULT disk_read (
	BYTE drv,			/* Physical drive nmuber (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,			/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..128) */
)
{
	DSTATUS s;

	s = disk_status(drv);
	if (s & STA_NOINIT) return RES_NOTRDY;
	if (!count) return RES_PARERR;
	
	/* Прочитать блок / блоки  */	
	rsi_read_blocks_from_sd_card(sector, buff, count);
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
section("L1_code")
DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0) */
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..128) */
)
{

	DSTATUS s;


	s = disk_status(drv);
	if (s & STA_NOINIT) return RES_NOTRDY;
	if (s & STA_PROTECT) return RES_WRPRT;  /* пока не проверяем это */ 
	if (!count) return RES_PARERR;

	/* Записать блок / блоки  */	
	rsi_write_blocks_to_sd_card(sector, (void*)buff, count);
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
section("L1_code")
DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	BYTE n, csd[16];
	WORD cs;

	if (disk_status(drv) & STA_NOINIT)					/* Check if card is in the socket */
		return RES_NOTRDY;

	res = RES_ERROR;
	switch (ctrl) {
		case CTRL_SYNC:		/* Make sure that no pending write process */
			res = RES_OK;
			break;

		case GET_SECTOR_COUNT:	/* Get number of sectors on the disk (DWORD) */
			cs = rsi_get_sec_count();
			*(DWORD*)buff = (DWORD) cs << 10;
			res = RES_OK;
		break;

		case GET_BLOCK_SIZE:	/* Get erase block size in unit of sector (DWORD) */
			*(DWORD*)buff = 128;
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
	}

	return res;
}

