#include <string.h>
#include <stdio.h>
#include "globdefs.h"
#include "eeprom.h"
#include "flash.h"
#include "ads1282.h"
#include "version.h"
#include "dac.h"
#include "led.h"
#include "log.h"
#include "pll.h"


/**
 * Что будем хранить в eeprom в виде u32
 * Внутри eeprom овских функций - адрес * 2 и + 1 2-е слово
 * Дописывать ТОЛЬКО с предпоследнего места!!!
 * Или стирать послностью флеш перед работой!
 */
enum eeprom_id {
    EEPROM_MOD_ID = 0,
    EEPROM_RSVD0,
    EEPROM_RSVD1,
    EEPROM_TIME_WORK,
    EEPROM_TIME_CMD,
    EEPROM_TIME_MODEM,
    EEPROM_DAC19_COEF,
    EEPROM_DAC4_COEF,
    EEPROM_RSVD2,
    EEPROM_ADC_OFS0,
    EEPROM_ADC_FSC0,
    EEPROM_ADC_OFS1,
    EEPROM_ADC_FSC1,
    EEPROM_ADC_OFS2,
    EEPROM_ADC_FSC2,
    EEPROM_ADC_OFS3,
    EEPROM_ADC_FSC3,
    EEPROM_RESET_CAUSE,
    EEPROM_END_OF_VAR
};


/**
 * Данные, которые храним в eeprom
 */
static struct {
    EEPROM_VALUE mod_id;	/* Номер прибора */
    EEPROM_VALUE rsvd0;		/* резерв 0 */
    EEPROM_VALUE rsvd1;		/* резерв 1 */
    EEPROM_VALUE time_work;	/* Время работы в режиме сбора данных */
    EEPROM_VALUE time_cmd;	/* Время работы в режиме PC */
    EEPROM_VALUE time_modem;	/* Время работы модема */
    EEPROM_VALUE dac19_coef;	/* Начальный коэффициент для кварца 19 МГц  */
    EEPROM_VALUE dac4_coef;	/* Начальный коэффициент для кварца 4 МГц  */
    EEPROM_VALUE rsvd2;		/* резерв 2  */
    EEPROM_VALUE adc_ofs0;	/* Коэффициент смещения 0 для 1-го АЦП  */
    EEPROM_VALUE adc_fsc0;	/* Коэффициент передачи для 1-го АЦП  */
    EEPROM_VALUE adc_ofs1;	/* Коэффициент смещения 0 для 2-го АЦП  */
    EEPROM_VALUE adc_fsc1;	/* Коэффициент передачи для 2-го АЦП  */
    EEPROM_VALUE adc_ofs2;	/* Коэффициент смещения 0 для 3-го АЦП  */
    EEPROM_VALUE adc_fsc2;	/* Коэффициент передачи для 3-го АЦП  */
    EEPROM_VALUE adc_ofs3;	/* Коэффициент смещения 0 для 4-го АЦП  */
    EEPROM_VALUE adc_fsc3;	/* Коэффициент передачи для 4-го АЦП  */
    EEPROM_VALUE reset_cause;	/* Результат последней перезагрузки  */
} eeprom_data = {  
    .mod_id = { EEPROM_MOD_ID, 0, 0},
    .rsvd0 = {        EEPROM_RSVD0, 0, 0},
    .rsvd1 = {        EEPROM_RSVD1, 0, 0},
    .time_work = {    EEPROM_TIME_WORK, 0, 0},
    .time_cmd = {     EEPROM_TIME_CMD, 0, 0},
    .time_modem = {   EEPROM_TIME_MODEM, 0, 0},
    .dac19_coef = {   EEPROM_DAC19_COEF, 0, 0},
    .dac4_coef = {   EEPROM_DAC4_COEF, 0, 0},
    .rsvd2 = {       EEPROM_RSVD2, 0, 0},
    .adc_ofs0 = {    EEPROM_ADC_OFS0, 0, 0},
    .adc_fsc0 = {    EEPROM_ADC_FSC0, 0, 0},
    .adc_ofs1 = {    EEPROM_ADC_OFS1, 0, 0},
    .adc_fsc1 = {   EEPROM_ADC_FSC1, 0, 0},
    .adc_ofs2 = {   EEPROM_ADC_OFS2, 0, 0},
    .adc_fsc2 = {   EEPROM_ADC_FSC2, 0, 0},
    .adc_ofs3 = {   EEPROM_ADC_OFS3, 0, 0},
    .adc_fsc3 = {  EEPROM_ADC_FSC3, 0, 0},
    .reset_cause = {EEPROM_RESET_CAUSE, 0, 0},
};

/**
 * Подсчет количества обращений к eeprom за сеанс
 */
static struct {
    long formated;
    long erased0;
    long erased1;
} EEPROM_ERASE_STATUS;


#pragma section("FLASH_code")
void eeprom_get_status(long *f, long *e0, long *e1)
{
    *f =  EEPROM_ERASE_STATUS.formated;
    *e0 = EEPROM_ERASE_STATUS.erased0;
    *e1 = EEPROM_ERASE_STATUS.erased1;
}


/* Объявлено в globdefs.h   */
#define 	VAR_IN_EEPROM  	(EEPROM_END_OF_VAR * 2)


static uint16_t EE_Format(void);
static uint16_t EE_ErasePage(u32);
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t, uint16_t);
static uint16_t EE_PageTransfer(uint16_t, uint16_t);
static uint16_t EE_FindValidPage(uint8_t);
static uint16_t EE_ReadVariable(u16, u16 *);
static uint16_t EE_WriteVariable(u16, u16);
static uint16_t eeprom_write(EEPROM_VALUE *);
static uint16_t eeprom_read(EEPROM_VALUE *);



/**
  * @brief  Restore the pages to a known good state in case of page's status
  *   corruption after a power loss.
  * @param  None.
  * @retval - Flash error code: on write Flash error
  *         - FLASH_COMPLETE: on success
  */
#pragma section("FLASH_code")
uint16_t eeprom_init(void)
{
    u16 PageStatus0 = 6, PageStatus1 = 6;
    u16 VarIdx = 0;
    int16_t x = -1;
    u16 FlashStatus;
    u16 EepromStatus = 0, ReadStatus = 0;
    uint16_t DataVar = 0;	/* Global variable used to store variable value in read sequence */

/* "Открыть" flash один раз */
/*    FLASH_init();		*/ 

    memset(&EEPROM_ERASE_STATUS, 0, sizeof(EEPROM_ERASE_STATUS));	/* Сотрем данные если есть */

    PageStatus0 = (*(volatile uint16_t *) PAGE0_BASE_ADDRESS);	/* Get Page0 status */
    PageStatus1 = (*(volatile uint16_t *) PAGE1_BASE_ADDRESS);	/* Get Page1 status */

    /* Если необходимо - стереть сектор (страницу) */
    switch (PageStatus0) {

    case ERASED:
	if (PageStatus1 == VALID_PAGE) {	/* Page0 erased, Page1 valid */

	    FlashStatus = EE_ErasePage(PAGE0_BASE_ADDRESS);	/* Erase Page0 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else if (PageStatus1 == RECEIVE_DATA) {	/* Page0 erased, Page1 receive */

	    FlashStatus = EE_ErasePage(PAGE0_BASE_ADDRESS);	/* Erase Page0 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }

	    FlashStatus = FLASH_program_half_word(PAGE1_BASE_ADDRESS, VALID_PAGE);	/* Mark Page1 as valid */

	    /* If program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else {		/* Самый First EEPROM access (Page0 & Page1 are erased) or invalid state -> format EEPROM */

	    FlashStatus = EE_Format();	/* Erase both Page0 and Page1 and set Page0 as valid page */

	    /* If erase/program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	}
	break;

    case RECEIVE_DATA:
	if (PageStatus1 == VALID_PAGE) {	/* Page0 receive, Page1 valid */

	    /* Transfer data from Page1 to Page0 */
	    for (VarIdx = 0; VarIdx < VAR_IN_EEPROM; VarIdx++) {
		if ((*(volatile uint16_t *) (PAGE0_BASE_ADDRESS + 6)) == VarIdx) {
		    x = VarIdx;
		}
		if (VarIdx != x) {

		    /* Read the last variables' updates */
		    ReadStatus = EE_ReadVariable(VarIdx, &DataVar);

		    /* In case variable corresponding to the virtual address was found */
		    if (ReadStatus != 0x01) {

			EepromStatus = EE_VerifyPageFullWriteVariable(VarIdx, DataVar);	/* Transfer the variable to the Page0 */

			/* If program operation was failed, a Flash error code is returned */
			if (EepromStatus != 0) {
			    return EepromStatus;
			}
		    }
		}
	    }

	    FlashStatus = FLASH_program_half_word(PAGE0_BASE_ADDRESS, VALID_PAGE);	/* Mark Page0 as valid */

	    /* If program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }

	    FlashStatus = EE_ErasePage(PAGE1_BASE_ADDRESS);	/* Erase Page1 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else if (PageStatus1 == ERASED) {	/* Page0 receive, Page1 erased */

	    FlashStatus = EE_ErasePage(PAGE1_BASE_ADDRESS);	/* Erase Page1 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }

	    FlashStatus = FLASH_program_half_word(PAGE0_BASE_ADDRESS, VALID_PAGE);	/* Mark Page0 as valid */

	    /* If program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else {		/* Invalid state -> format eeprom */

	    FlashStatus = EE_Format();	/* Erase both Page0 and Page1 and set Page0 as valid page */

	    /* If erase/program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	}
	break;

    case VALID_PAGE:
	if (PageStatus1 == VALID_PAGE) {	/* Invalid state -> format eeprom */

	    FlashStatus = EE_Format();	/* Erase both Page0 and Page1 and set Page0 as valid page */

	    /* If erase/program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else if (PageStatus1 == ERASED) {	/* Page0 valid, Page1 erased */

	    FlashStatus = EE_ErasePage(PAGE1_BASE_ADDRESS);	/* Erase Page1 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	} else {		/* Page0 valid, Page1 receive */

	    /* Transfer data from Page0 to Page1 */
	    for (VarIdx = 0; VarIdx < VAR_IN_EEPROM; VarIdx++) {
		if ((*(volatile uint16_t *) (PAGE1_BASE_ADDRESS + 6)) == VarIdx) {
		    x = VarIdx;
		}

		if (VarIdx != x) {

		    /* Read the last variables' updates */
		    ReadStatus = EE_ReadVariable(VarIdx, &DataVar);

		    /* In case variable corresponding to the virtual address was found */
		    if (ReadStatus != 0x1) {

			/* Transfer the variable to the Page1 */
			EepromStatus = EE_VerifyPageFullWriteVariable(VarIdx, DataVar);

			/* If program operation was failed, a Flash error code is returned */
			if (EepromStatus != 0) {
			    return EepromStatus;
			}
		    }
		}
	    }

	    FlashStatus = FLASH_program_half_word(PAGE1_BASE_ADDRESS, VALID_PAGE);	/* Mark Page1 as valid */

	    /* If program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }

	    FlashStatus = EE_ErasePage(PAGE0_BASE_ADDRESS);	/* Erase Page0 */

	    /* If erase operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	}
	break;

    default:			/* Any other state -> format eeprom */
	FlashStatus = EE_Format();	/* Erase both Page0 and Page1 and set Page0 as valid page */

	/* If erase/program operation was failed, a Flash error code is returned */
	if (FlashStatus != 0) {
	    return FlashStatus;
	}
	break;
    }

    return 0;			/* Все OK */
}



/**
  * @brief  Returns the last stored variable data, if found, which correspond to
  *   the passed virtual address
  * @param  VirtAddress: Variable virtual address
  * @param  Data: Global variable contains the read variable value
  * @retval Success or error status:
  *           - 0: if variable was found
  *           - 1: if the variable was not found
  *           - NO_VALID_PAGE: if no valid page was found.
  */
#pragma section("FLASH_code")
static uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t * Data)
{
    uint16_t ValidPage = PAGE0;
    uint16_t AddressValue = 0x5555, ReadStatus = 1;
    uint32_t Address, PageStartAddress;

    /* Get active Page for read operation */
    ValidPage = EE_FindValidPage(READ_FROM_VALID_PAGE);

    /* Check if there is no valid page */
    if (ValidPage == NO_VALID_PAGE) {
	return NO_VALID_PAGE;
    }

    /* Get the valid Page start Address */
    PageStartAddress = (uint32_t) (EEPROM_START_ADDRESS + (uint32_t) (ValidPage * PAGE_SIZE));

    /* Get the valid Page end Address */
    Address = (uint32_t) ((EEPROM_START_ADDRESS - 2) + (uint32_t) ((1 + ValidPage) * PAGE_SIZE));

    /* Check each active page address starting from end */
    while (Address > (PageStartAddress + 2)) {

	AddressValue = (*(volatile uint16_t *) Address);	/* Get the current location content to be compared with virtual address */

	/* Compare the read address with the virtual address */
	if (AddressValue == VirtAddress) {
	    *Data = (*(volatile uint16_t *) (Address - 2));	/* Get content of Address-2 which is variable value */
	    ReadStatus = 0;	/* In case variable value is read, reset ReadStatus flag */
	    break;
	} else {
	    Address = Address - 4;	/* Next address location */
	}
    }

    return ReadStatus;		/* Return ReadStatus value: (0: variable exist, 1: variable doesn't exist) */
}

/**
  * @brief  Writes/updates variable data in EEPROM.
  * @param  VirtAddress: Variable virtual address
  * @param  Data: 16 bit data to be written
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
#pragma section("FLASH_code")
static uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    uint16_t Status = 0;

    Status = EE_VerifyPageFullWriteVariable(VirtAddress, Data);	/* Write the variable virtual address and value in the EEPROM */

    /* In case the EEPROM active page is full */
    if (Status == PAGE_FULL) {
	Status = EE_PageTransfer(VirtAddress, Data);	/* Perform Page transfer */
    }

    return Status;		/* Return last operation status */
}

/**
  * @brief  Erases PAGE0 and PAGE1 and writes VALID_PAGE header to PAGE0
  * @param  None
  * @retval Status of the last operation (Flash write or erase) done during
  *         EEPROM formating
  */
#pragma section("FLASH_code")
static uint16_t EE_Format(void)
{
    u16 FlashStatus = 0;

    FlashStatus = EE_ErasePage(PAGE0_BASE_ADDRESS);	/* Erase Page0 */

    /* If erase operation was failed, a Flash error code is returned */
    if (FlashStatus != 0) {
	return FlashStatus;
    }

    FlashStatus = FLASH_program_half_word(PAGE0_BASE_ADDRESS, VALID_PAGE);	/* Set Page0 as valid page: Write VALID_PAGE at Page0 base address */

    /* If program operation was failed, a Flash error code is returned */
    if (FlashStatus != 0) {
	return FlashStatus;
    }

    /* Erase Page1 */
    FlashStatus = EE_ErasePage(PAGE1_BASE_ADDRESS);

    EEPROM_ERASE_STATUS.formated++;

    return FlashStatus;		/* Return Page1 erase operation status */
}


/**
 * Форматировать 1 сектор нашего псевдо eeprom
 * подсчитать количество вызовов за 1 запуск
 */
#pragma section("FLASH_code")
static uint16_t EE_ErasePage(u32 page)
{
    u16 FlashStatus = 0;

    FlashStatus = FLASH_erase_page(page);	/* Erase Page0 */

    if (page == PAGE0_BASE_ADDRESS)
	EEPROM_ERASE_STATUS.erased0++;
    else
	EEPROM_ERASE_STATUS.erased1++;

    return FlashStatus;
}


/**
  * @brief  Find valid Page for write or read operation
  * @param  Operation: operation to achieve on the valid page.
  *   This parameter can be one of the following values:
  *     @arg READ_FROM_VALID_PAGE: read operation from valid page
  *     @arg WRITE_IN_VALID_PAGE: write operation from valid page
  * @retval Valid page number (PAGE0 or PAGE1) or NO_VALID_PAGE in case
  *   of no valid page was found
  */
#pragma section("FLASH_code")
static uint16_t EE_FindValidPage(uint8_t Operation)
{
    uint16_t PageStatus0 = 6, PageStatus1 = 6;

    PageStatus0 = (*(volatile uint16_t *) PAGE0_BASE_ADDRESS);	/* Get Page0 actual status */
    PageStatus1 = (*(volatile uint16_t *) PAGE1_BASE_ADDRESS);	/* Get Page1 actual status */

    /* Write or read operation */
    switch (Operation) {

    case WRITE_IN_VALID_PAGE:	/* ---- Write operation ---- */
	if (PageStatus1 == VALID_PAGE) {

	    /* Page0 receiving data */
	    if (PageStatus0 == RECEIVE_DATA) {
		return PAGE0;	/* Page0 valid */
	    } else {
		return PAGE1;	/* Page1 valid */
	    }
	} else if (PageStatus0 == VALID_PAGE) {

	    /* Page1 receiving data */
	    if (PageStatus1 == RECEIVE_DATA) {
		return PAGE1;	/* Page1 valid */
	    } else {
		return PAGE0;	/* Page0 valid */
	    }
	} else {
	    return NO_VALID_PAGE;	/* No valid Page */
	}

    case READ_FROM_VALID_PAGE:	/* ---- Read operation ---- */
	if (PageStatus0 == VALID_PAGE) {
	    return PAGE0;	/* Page0 valid */
	} else if (PageStatus1 == VALID_PAGE) {
	    return PAGE1;	/* Page1 valid */
	} else {
	    return NO_VALID_PAGE;	/* No valid Page */
	}

    default:
	return PAGE0;		/* Page0 valid */
    }
}

/**
  * @brief  Verify if active page is full and Writes variable in EEPROM.
  * @param  VirtAddress: 16 bit virtual address of the variable
  * @param  Data: 16 bit data to be written as variable value
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
#pragma section("FLASH_code")
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    u16 FlashStatus = 0;
    uint16_t ValidPage = PAGE0;
    uint32_t Address, PageEndAddress;

    ValidPage = EE_FindValidPage(WRITE_IN_VALID_PAGE);	/* Get valid Page for write operation */

    /* Check if there is no valid page */
    if (ValidPage == NO_VALID_PAGE) {
	return NO_VALID_PAGE;
    }

    Address = (uint32_t) (EEPROM_START_ADDRESS + (uint32_t) (ValidPage * PAGE_SIZE));	/* Get the valid Page start Address */
    PageEndAddress = (uint32_t) ((EEPROM_START_ADDRESS - 2) + (uint32_t) ((1 + ValidPage) * PAGE_SIZE));	/* Get the valid Page end Address */

    /* Check each active page address starting from begining */
    while (Address < PageEndAddress) {

	/* Verify if Address and Address+2 contents are 0xFFFFFFFF */
	if ((*(volatile uint32_t *) Address) == 0xFFFFFFFF) {

	    FlashStatus = FLASH_program_half_word(Address, Data);	/* Set variable data */

	    /* If program operation was failed, a Flash error code is returned */
	    if (FlashStatus != 0) {
		return FlashStatus;
	    }
	    /* Set variable virtual address */
	    FlashStatus = FLASH_program_half_word(Address + 2, VirtAddress);

	    /* Return program operation status */
	    return FlashStatus;
	} else {
	    /* Next address location */
	    Address = Address + 4;
	}
    }

    /* Return PAGE_FULL in case the valid page is full */
    return PAGE_FULL;
}

/**
  * @brief  Transfers last updated variables data from the full Page to
  *   an empty one.
  * @param  VirtAddress: 16 bit virtual address of the variable
  * @param  Data: 16 bit data to be written as variable value
  * @retval Success or error status:
  *           - FLASH_COMPLETE: on success
  *           - PAGE_FULL: if valid page is full
  *           - NO_VALID_PAGE: if no valid page was found
  *           - Flash error code: on write Flash error
  */
#pragma section("FLASH_code")
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data)
{
    u16 FlashStatus = 0;
    uint32_t NewPageAddress, OldPageAddress;
    uint16_t ValidPage = PAGE0, VarIdx = 0;
    uint16_t EepromStatus = 0, ReadStatus = 0;
    uint16_t DataVar = 0;

    ValidPage = EE_FindValidPage(READ_FROM_VALID_PAGE);	/* Get active Page for read operation */

    if (ValidPage == PAGE1) {	/* Page1 valid */

	NewPageAddress = PAGE0_BASE_ADDRESS;	/* New page address where variable will be moved to */
	OldPageAddress = PAGE1_BASE_ADDRESS;	/* Old page address where variable will be taken from */
    } else if (ValidPage == PAGE0) {	/* Page0 valid */
	NewPageAddress = PAGE1_BASE_ADDRESS;	/* New page address where variable will be moved to */
	OldPageAddress = PAGE0_BASE_ADDRESS;	/* Old page address where variable will be taken from */
    } else {
	return NO_VALID_PAGE;	/* No valid Page */
    }

    /* Set the new Page status to RECEIVE_DATA status */
    FlashStatus = FLASH_program_half_word(NewPageAddress, RECEIVE_DATA);

    /* If program operation was failed, a Flash error code is returned */
    if (FlashStatus != 0) {
	return FlashStatus;
    }

    EepromStatus = EE_VerifyPageFullWriteVariable(VirtAddress, Data);	/* Write the variable passed as parameter in the new active page */

    /* If program operation was failed, a Flash error code is returned */
    if (EepromStatus != 0) {
	return EepromStatus;
    }

    /* Transfer process: transfer variables from old to the new active page */
    for (VarIdx = 0; VarIdx < VAR_IN_EEPROM; VarIdx++) {
	if (VarIdx != VirtAddress) {	/* Check each variable except the one passed as parameter */

	    /* Read the other last variable updates */
	    ReadStatus = EE_ReadVariable(VarIdx, &DataVar);

	    /* In case variable corresponding to the virtual address was found */
	    if (ReadStatus != 0x1) {

		/* Transfer the variable to the new active page */
		EepromStatus = EE_VerifyPageFullWriteVariable(VarIdx, DataVar);

		/* If program operation was failed, a Flash error code is returned */
		if (EepromStatus != 0) {
		    return EepromStatus;
		}
	    }
	}
    }

    FlashStatus = EE_ErasePage(OldPageAddress);	/* Erase the old Page: Set old Page status to ERASED status */

    /* If erase operation was failed, a Flash error code is returned */
    if (FlashStatus != 0) {
	return FlashStatus;
    }

    FlashStatus = FLASH_program_half_word(NewPageAddress, VALID_PAGE);	/* Set new Page status to VALID_PAGE status */

    /* If program operation was failed, a Flash error code is returned */
    if (FlashStatus != 0) {
	return FlashStatus;
    }

    /* Return last operation flash status */
    return FlashStatus;
}

//////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Запись значения (адрес должен быть кратен 4!)
 */
#pragma section("FLASH_code")
uint16_t eeprom_write(EEPROM_VALUE * val)
{
    u16 res;
    u16 addr = val->Addr * 2;	// пишем по 4 байта (2 слова)
    u32 data = val->Data;

    res = EE_WriteVariable(addr, data & 0xffff);
    res |= EE_WriteVariable(addr + 1, data >> 16);
    return res;
}


/**
 * Чтение значения 
  *           - 0: if variable was found
  *           - 1: if the variable was not found
  *           - NO_VALID_PAGE: if no valid page was found.
 */
#pragma section("FLASH_code")
uint16_t eeprom_read(EEPROM_VALUE * val)
{
    u16 res;
    u16 d0, d1;
    u16 addr = val->Addr * 2;	// пишем по 4 байта (2 слова)

    res = EE_ReadVariable(addr, &d0);	// младший
    res += EE_ReadVariable(addr + 1, &d1);	// старший
    val->Data = (u32) d0 | ((u32) d1 << 16);
    val->Exist = res;
    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Видно снаружи - запись
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Обнулить все параметры EEPROM */
#pragma section("FLASH_code")
void write_zero_to_eeprom(void)
{
    int i;
    EEPROM_VALUE val;

    for (i = 0; i < EEPROM_END_OF_VAR - 1; i++) {
	memcpy(&val, &eeprom_data + sizeof(val) * i, sizeof(val));
	eeprom_write(&val);
    }
}

/**
 * Записать причину сброса
 */
#pragma section("FLASH_code")
void write_reset_cause_to_eeprom(u32 cause)
{
    EEPROM_VALUE val;
    val.Addr = EEPROM_RESET_CAUSE;
    val.Data = cause;
    eeprom_write(&val);
}

/**  
 *  Скинуть коэффициент 19.2 ЦАП на flash
 */
#pragma section("FLASH_code")
int write_dac19_coef_to_eeprom(u16 data)
{
    int res = 0;
    eeprom_data.dac19_coef.Data = data;
    res = eeprom_write(&eeprom_data.dac19_coef);
    return res;
}


/**  
 *  Скинуть все коэффициенты 4.096 ЦАП на flash
 */
#pragma section("FLASH_code")
int write_dac4_coef_to_eeprom(u16 data)
{
    int res = 0;
    eeprom_data.dac4_coef.Data = data;
    res = eeprom_write(&eeprom_data.dac4_coef);
    return res;
}


/**  
 *  Скинуть все коэффициенты АЦП на flash
 */
#pragma section("FLASH_code")
int write_all_ads1282_coefs_to_eeprom(void *par)
{
    int res;
    ADS1282_Regs *regs;

    if (par == NULL)
	return -1;

    regs = (ADS1282_Regs *) par;

    eeprom_data.adc_ofs0.Data = regs->chan[0].offset;
    eeprom_data.adc_fsc0.Data = regs->chan[0].gain;
    eeprom_data.adc_ofs1.Data = regs->chan[1].offset;
    eeprom_data.adc_fsc1.Data = regs->chan[1].gain;
    eeprom_data.adc_ofs2.Data = regs->chan[2].offset;
    eeprom_data.adc_fsc2.Data = regs->chan[2].gain;
    eeprom_data.adc_ofs3.Data = regs->chan[3].offset;
    eeprom_data.adc_fsc3.Data = regs->chan[3].gain;

    /* пишем данные */
    res = eeprom_write(&eeprom_data.adc_ofs0);
    res += eeprom_write(&eeprom_data.adc_fsc0);
    res += eeprom_write(&eeprom_data.adc_ofs1);
    res += eeprom_write(&eeprom_data.adc_fsc1);
    res += eeprom_write(&eeprom_data.adc_ofs2);
    res += eeprom_write(&eeprom_data.adc_fsc2);
    res += eeprom_write(&eeprom_data.adc_ofs3);
    res += eeprom_write(&eeprom_data.adc_fsc3);

    return res;
}


/**
 *  Записывает номер прибора в eeprom
 */
#pragma section("FLASH_code")
void write_mod_id_to_eeprom(u16 addr)
{
    eeprom_data.mod_id.Data = addr;
    eeprom_write(&eeprom_data.mod_id);
}

/**
 *  Записывает rsvd0
 */
#pragma section("FLASH_code")
void write_rsvd0_to_eeprom(u32 r)
{
    eeprom_data.rsvd0.Data = r;
    eeprom_write(&eeprom_data.rsvd0);
}


/**
 *  Записывает rsvd1
 */
#pragma section("FLASH_code")
void write_rsvd1_to_eeprom(u32 r)
{
    eeprom_data.rsvd1.Data = r;
    eeprom_write(&eeprom_data.rsvd1);
}

/**  
 *  Скинуть rsvd2
 */
#pragma section("FLASH_code")
void write_rsvd2_to_eeprom(u32 d)
{
    int res = 0;
    eeprom_data.rsvd2.Data = d;
    res = eeprom_write(&eeprom_data.rsvd2);
}

/**
 *  Записывает время работы  в режиме сбора данных
 */
#pragma section("FLASH_code")
int write_time_work_to_eeprom(u32 t)
{
    int res;
    eeprom_data.time_work.Data = t;
    res = eeprom_write(&eeprom_data.time_work);
    return res;
}


/**
 *  Записывает время работы в командном режиме
 */
#pragma section("FLASH_code")
int write_time_cmd_to_eeprom(u32 t)
{
    int res;
    eeprom_data.time_cmd.Data = t;
    res = eeprom_write(&eeprom_data.time_cmd);
    return res;
}

/**
 *  Записывает время работы
 */
#pragma section("FLASH_code")
int write_time_modem_to_eeprom(u32 t)
{
    int res;
    eeprom_data.time_modem.Data = t;
    res = eeprom_write(&eeprom_data.time_modem);
    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Видно снаружи - чтение
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Причина последнего сброса. Читаем значение, возвращаем и стираем в EEPROM
 * При непредвиденном сбросе будет 0, в других случаях прогорамма поставит значение
 */
#pragma section("FLASH_code")
u32 read_reset_cause_from_eeprom(void)
{
    u32 res;
#if 1
    static u8 read = false;

    // Читаем причину
    if (read == false) {
	EEPROM_VALUE val;
	read = true;

	// Читаем в причину
	eeprom_read(&eeprom_data.reset_cause);

	// В любом случае стираем значение
	val.Addr = EEPROM_RESET_CAUSE;
	val.Data = CAUSE_UNKNOWN_RESET;
	eeprom_write(&val);
    }
#else

    // Читаем в причину
    eeprom_read(&eeprom_data.reset_cause);

#endif

    res = eeprom_data.reset_cause.Data;
    return res;
}


/**
 * Получить коэффициенты ЦАП с flash
 */
#pragma section("FLASH_code")
int read_dac_coefs_from_eeprom(void *par)
{
    int res;
    DEV_DAC_STRUCT *data;

    if (par == NULL)
	return -1;

    data = (DEV_DAC_STRUCT *) par;

    data->dac19_data = eeprom_data.dac19_coef.Data;
    data->dac4_data = eeprom_data.dac4_coef.Data;

    if (data->dac19_data > DAC19_MAX_DATA) {
	log_write_log_file("ERROR: bad dac19 = %d. Set to default %d\n", data->dac19_data, DAC19_INIT_DATA);
	data->dac19_data = DAC19_INIT_DATA;
	res = -1;
    }

    if (data->dac4_data > DAC19_MAX_DATA) {
	log_write_log_file("ERROR: bad dac4 = %d. Set to default %d\n", data->dac4_data, DAC4_INIT_DATA);
	data->dac4_data = DAC4_INIT_DATA;
	res = -1;
    }


    return res;
}



/**
 * Получить ВСЕ коэффициенты АЦП с flash - знаем с какой структурой работаем (несмотря на int len)
 * Переделать по образцу STM32. Ошибки исправлять прямо здесь!
 * Прочитать можно и из FLASH code 
 */
#pragma section("FLASH_code")
int read_ads1282_coefs_from_eeprom(void *par)
{
    ADS1282_Regs *regs;
    int res = 0, i;

    if (par == NULL)
	return -1;

    regs = (ADS1282_Regs *) par;

    // Читаем данные
    regs->chan[0].offset = eeprom_data.adc_ofs0.Data;
    regs->chan[0].gain = eeprom_data.adc_fsc0.Data;

    regs->chan[1].offset = eeprom_data.adc_ofs1.Data;
    regs->chan[1].gain = eeprom_data.adc_fsc1.Data;

    regs->chan[2].offset = eeprom_data.adc_ofs2.Data;
    regs->chan[2].gain = eeprom_data.adc_fsc2.Data;

    regs->chan[3].offset = eeprom_data.adc_ofs3.Data;
    regs->chan[3].gain = eeprom_data.adc_fsc3.Data;

    for (i = 0; i < 4; i++) {
	if (regs->chan[i].offset == 0xffffffff) {
	    log_write_log_file("ERROR: bad offset(%d). Set to default 64000\n", i);
	    regs->chan[i].offset = 64000;
	    res = -1;		/* Нет констант в EEPROM */
	}

	if (regs->chan[i].gain == 0) {
	    log_write_log_file("ERROR: bad gain(%d). Set to default 0x400000\n", i);
	    regs->chan[i].gain = 0x400000;
	    res = -1;		/* Нет констант в EEPROM */
	}
    }

    if (res == 0)
	regs->magic = MAGIC;
    return res;
}



/**
 *  Возвращает номер прибора
 */
#pragma section("FLASH_code")
u16 read_mod_id_from_eeprom(void)
{
    u16 num;
    num = (u16) eeprom_data.mod_id.Data;

    if (num > 9999 || num == 0) {
	log_write_log_file("ERROR: bad module num %d. Need to be 1..9999\n", num);
    }

    return num;
}


/**
 * Время работы в режиме сбора данных
 */
#pragma section("FLASH_code")
u32 read_time_work_from_eeprom(void)
{
    return eeprom_data.time_work.Data;
}


/**
 *  Возвращает время работы в командном режиме
 */
#pragma section("FLASH_code")
u32 read_time_cmd_from_eeprom(void)
{
    return eeprom_data.time_cmd.Data;
}


/**
 *  Возвращает время работы модема
 */
#pragma section("FLASH_code")
u32 read_time_modem_from_eeprom(void)
{
    return eeprom_data.time_modem.Data;
}



/**
 * получить rsvd0
*/
#pragma section("FLASH_code")
u32 read_rsvd0_from_eeprom(void)
{
    return eeprom_data.rsvd0.Data;
}


/**
 * получить rsvd1
*/
#pragma section("FLASH_code")
u32 read_rsvd1_from_eeprom(void)
{
    return eeprom_data.rsvd1.Data;
}


/**
 * получить rsvd2
*/
#pragma section("FLASH_code")
u32 read_rsvd2_from_eeprom(void)
{
    return eeprom_data.rsvd2.Data;
}


/** 
 *  Прочитать с eeprom данные. Разбираемся с ошибками выше
 */
#pragma section("FLASH_code")
u32 read_all_data_from_eeprom(void)
{
    u32 res = 0;

    // За одно ставим и адрес платы     
    if (eeprom_read(&eeprom_data.mod_id))
	res |= 1 << EEPROM_MOD_ID;

    eeprom_read(&eeprom_data.rsvd0);
    eeprom_read(&eeprom_data.rsvd1);

    if (eeprom_read(&eeprom_data.time_work))
	res |= 1 << EEPROM_TIME_WORK;

    if (eeprom_read(&eeprom_data.time_cmd))
	res |= 1 << EEPROM_TIME_CMD;

    if (eeprom_read(&eeprom_data.time_modem))
	res |= 1 << EEPROM_TIME_MODEM;

    if (eeprom_read(&eeprom_data.dac19_coef))
	res |= 1 << EEPROM_DAC19_COEF;

    if (eeprom_read(&eeprom_data.dac4_coef))
	res |= 1 << EEPROM_DAC4_COEF;

    eeprom_read(&eeprom_data.rsvd2);

    if (eeprom_read(&eeprom_data.adc_ofs0))
	res |= 1 << EEPROM_ADC_OFS0;

    if (eeprom_read(&eeprom_data.adc_fsc0))
	res |= 1 << EEPROM_ADC_FSC0;

    if (eeprom_read(&eeprom_data.adc_ofs1))
	res |= 1 << EEPROM_ADC_OFS1;

    if (eeprom_read(&eeprom_data.adc_fsc1))
	res |= 1 << EEPROM_ADC_FSC1;

    if (eeprom_read(&eeprom_data.adc_ofs2))
	res |= 1 << EEPROM_ADC_OFS2;

    if (eeprom_read(&eeprom_data.adc_fsc2))
	res |= 1 << EEPROM_ADC_FSC2;

    if (eeprom_read(&eeprom_data.adc_ofs3))
	res |= 1 << EEPROM_ADC_OFS3;

    if (eeprom_read(&eeprom_data.adc_fsc3))
	res |= 1 << EEPROM_ADC_FSC3;

    return res;
}


/** 
 *  Записать данные по умолчанию на eeprom.
 */
#pragma section("FLASH_code")
void write_default_data_to_eeprom(void *par)
{
    ADS1282_Regs regs;
    DEV_STATUS_STRUCT *status;
    int i;

    LED_on(LED_YELLOW);

    // сбрасываем eeprom
    if (par != NULL) {
	status = (DEV_STATUS_STRUCT *) par;
	status->st_test0 |= 0x10;
	status->eeprom = (u32)(-1);
	status->st_test1 |= 0x08;

	for (i = 0; i < 4; i++) {
	    regs.chan[i].offset = 64000;
	    regs.chan[i].gain = 0x400000;
	}
	regs.magic = MAGIC;

	write_mod_id_to_eeprom(9999);
	write_dac4_coef_to_eeprom(DAC4_INIT_DATA);
	write_dac19_coef_to_eeprom(DAC19_INIT_DATA);
	write_time_work_to_eeprom(0);
	write_time_modem_to_eeprom(0);
	write_time_cmd_to_eeprom(0);

	write_rsvd0_to_eeprom(0);
	write_rsvd1_to_eeprom(0);
	write_rsvd2_to_eeprom(0);
	write_all_ads1282_coefs_to_eeprom(&regs);

	status->st_test0 &= ~0x10;
	status->eeprom = 0;
	status->st_test1 &= ~0x08;
    }
    LED_off(LED_YELLOW);
}
