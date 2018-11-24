#ifndef __EEPROM_H
#define __EEPROM_H

#include "globdefs.h"
#include "flash.h"


#define PAGE_SIZE             ((uint32_t)0x02000)  /* Размер мелкого сектора (страницы) 8 кБайт */

/* EEPROM start address in Flash. Начинаем с 63 сектора */
#define EEPROM_START_ADDRESS 	(FLASH_START_ADDR + 0x3F0000) /* EEPROM emulation start address:  from sector2, after 8KByte of used Flash memory */

/* Pages 0 and 1 base and end addresses */
#define PAGE0_BASE_ADDRESS    ((uint32_t)(EEPROM_START_ADDRESS + 0x0000))
#define PAGE0_END_ADDRESS     ((uint32_t)(EEPROM_START_ADDRESS + (PAGE_SIZE - 1)))

#define PAGE1_BASE_ADDRESS    ((uint32_t)(EEPROM_START_ADDRESS + 0x2000)) 			/* 63 сектор  */
#define PAGE1_END_ADDRESS     ((uint32_t)(EEPROM_START_ADDRESS + (2 * PAGE_SIZE - 1)))  	/* 64 сектор */

/* Used Flash pages for EEPROM emulation */
#define PAGE0                 ((uint16_t)0x0000)
#define PAGE1                 ((uint16_t)0x0001)

/* No valid page define */
#define NO_VALID_PAGE         ((uint16_t)0x00AB)

/* Page status definitions */
#define ERASED                ((uint16_t)0xFFFF)     /* Page is empty */
#define RECEIVE_DATA          ((uint16_t)0xEEEE)     /* Page is marked to receive data */
#define VALID_PAGE            ((uint16_t)0x0000)     /* Page containing valid data */

/* Valid pages in read and write defines */
#define READ_FROM_VALID_PAGE  ((uint8_t)0x00)
#define WRITE_IN_VALID_PAGE   ((uint8_t)0x01)

/* Page full define */
#define PAGE_FULL             ((uint8_t)0x80)


/* Что будем хранить в eeprom. не паковать! */
typedef struct {
    u16   Addr;
    u16   Exist;
    u32   Data;
} EEPROM_VALUE;


/* Exported functions ------------------------------------------------------- */
uint16_t eeprom_init(void);
void eeprom_get_status(long*, long*, long*);

u32  read_time_cmd_from_eeprom(void);
u32  read_time_modem_from_eeprom(void);
u32  read_time_work_from_eeprom(void);
u16  read_mod_id_from_eeprom(void);
u32  read_all_data_from_eeprom(void);
u32  read_reset_cause_from_eeprom(void);
int  read_dac_coefs_from_eeprom(void *);
int  read_ads1282_coefs_from_eeprom(void *);
u32  read_rsvd0_from_eeprom(void);
u32  read_rsvd1_from_eeprom(void);
u32  read_rsvd2_from_eeprom(void);

void write_default_data_to_eeprom(void*);
int  write_time_work_to_eeprom(u32);
void write_mod_id_to_eeprom(u16);
int  write_time_modem_to_eeprom(u32);
int  write_time_cmd_to_eeprom(u32);
void write_rsvd0_to_eeprom(u32);
void write_rsvd1_to_eeprom(u32);
void write_rsvd2_to_eeprom(u32);
void write_reset_cause_to_eeprom(u32);
int  write_all_ads1282_coefs_to_eeprom(void*);
int  write_dac4_coef_to_eeprom(u16);
int  write_dac19_coef_to_eeprom(u16);
int  write_dacT_coef(u16);


#endif /* __EEPROM_H */

