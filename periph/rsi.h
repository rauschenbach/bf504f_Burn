/***************************************************************************************************************
 *                                                                      
 *   (C) Copyright 2009 - Analog Devices, Inc.  All rights reserved.    
 *                                                                      
 *    FILE:     rsi.h ( )					  	
 *                                                                             
 *    CHANGES:  1.00.0  - initial release    					
 *										
 ***************************************************************************************************************/
#ifndef __RSI_H__
#define __RSI_H__

#include <adi_types.h>
#include <ccblkfn.h>
#include <cdefBF504f.h>  /*  У нас другой блекфин!!! */
#include <cycles.h>
#include <sys\exception.h>
#include "globdefs.h"
#include "pll.h"
#include "config.h"


/****************************************************/
/*  SD Card Status Register                         */
/****************************************************/
/****************************************************/
/*  SD Card Status Register Enumerations            */
/****************************************************/
typedef enum {  SD_CSR_CURRENT_STATE_IDLE            = (0<<9),
                SD_CSR_CURRENT_STATE_READY           = (1<<9),
                SD_CSR_CURRENT_STATE_IDENTIFICATION  = (2<<9),
                SD_CSR_CURRENT_STATE_STANDBY         = (3<<9),
                SD_CSR_CURRENT_STATE_TRANSFER        = (4<<9),
                SD_CSR_CURRENT_STATE_DATA            = (5<<9),
                SD_CSR_CURRENT_STATE_RECEIVE         = (6<<9),
                SD_CSR_CURRENT_STATE_PROGRAM         = (7<<9),
                SD_CSR_CURRENT_STATE_DISCONNECT      = (8<<9),
                SD_CSR_CURRENT_STATE_RESERVED0       = (9<<9),
                SD_CSR_CURRENT_STATE_RESERVED1       = (10<<9),
                SD_CSR_CURRENT_STATE_RESERVED2       = (11<<9),
                SD_CSR_CURRENT_STATE_RESERVED3       = (12<<9),
                SD_CSR_CURRENT_STATE_RESERVED4       = (13<<9),
                SD_CSR_CURRENT_STATE_RESERVED5       = (14<<9),
                SD_CSR_CURRENT_STATE_IO_MODE         = (15<<9)
} SD_CSR_CURRENT_CARD_STATE;

/****************************************************/
/*  SD Card Status Register Bit Masks               */
/****************************************************/
#define SD_CSR_AKE_SEQ_ERROR        (1<<3)
#define SD_CSR_APP_CMD              (1<<5)
#define SD_CSR_READY_FOR_DATA       (1<<8)
#define SD_CSR_CURRENT_STATE        (15<<9)
#define SD_CSR_ERASE_RESET          (1<<13)
#define SD_CSR_CARD_ECC_DISABLED    (1<<14)
#define SD_CSR_WP_ERASE_SKIP        (1<<15)
#define SD_CSR_CSD_OVERWRITE        (1<<16)
#define SD_CSR_ERROR                (1<<19)
#define SD_CSR_CC_ERROR             (1<<20)
#define SD_CSR_CARD_ECC_FAILED      (1<<21)
#define SD_CSR_ILLEGAL_COMMAND      (1<<22)
#define SD_CSR_COM_CRC_ERROR        (1<<23)
#define SD_CSR_LOCK_UNLOCK_FAILED   (1<<24)
#define SD_CSR_CARD_IS_LOCKED       (1<<25)
#define SD_CSR_WP_VIOLATION         (1<<26)
#define SD_CSR_ERASE_PARAM          (1<<27)
#define SD_CSR_ERASE_SEQ_ERROR      (1<<28)
#define SD_CSR_BLOCK_LEN_ERROR      (1<<29)
#define SD_CSR_ADDRESS_ERROR        (1<<30)
#define SD_CSR_OUT_OF_RANGE         (1<<31)

typedef unsigned long ADI_RSI_SD_CSR_REGISTER;
/****************************************************/
/*  SD Status Register                              */
/****************************************************/

/****************************************************/
/*  SD Status Register Enumerations                 */
/****************************************************/
typedef enum {  SD_SSR_DAT_BUS_WIDTH_1           = 0,
                SD_SSR_DAT_BUS_WIDTH_RESERVED0   = 1,
                SD_SSR_DAT_BUS_WIDTH_4           = 2,
                SD_SSR_DAT_BUS_WIDTH_RESERVED1   = 3
} SD_SSR_DAT_BUS_WIDTH;

typedef enum {  SD_SSR_SECURED_MODE_DISABLED           = 0,
                SD_SSR_SECURED_MODE_ENABLED            = 1
} SD_SSR_SECURED_MODE;

typedef enum {  SD_SSR_SD_CARD_TYPE_REGULAR_RD_WR_CARD   = 0,
                SD_SSR_SD_CARD_TYPE_ROM                  = 1
} SD_SSR_SD_CARD_TYPE;

typedef enum {  SD_SSR_SPEED_CLASS_0 = 0,
                SD_SSR_SPEED_CLASS_2 = 1,
                SD_SSR_SPEED_CLASS_3 = 2,
                SD_SSR_SPEED_CLASS_6 = 3
} SD_SSR_SPEED_CLASS;

typedef enum {  SD_SSR_AU_SIZE_NOT_DEFINED   = 0,
                SD_SSR_AU_SIZE_16KB          = 1,
                SD_SSR_AU_SIZE_32KB          = 2,
                SD_SSR_AU_SIZE_64KB          = 3,
                SD_SSR_AU_SIZE_128KB         = 4,
                SD_SSR_AU_SIZE_256KB         = 5,
                SD_SSR_AU_SIZE_512KB         = 6,
                SD_SSR_AU_SIZE_1MB           = 7,
                SD_SSR_AU_SIZE_2MB           = 8,
                SD_SSR_AU_SIZE_4MB           = 9,
                SD_SSR_AU_SIZE_RESERVED0     = 10,
                SD_SSR_AU_SIZE_RESERVED1     = 11,
                SD_SSR_AU_SIZE_RESERVED2     = 12,
                SD_SSR_AU_SIZE_RESERVED3     = 13,
                SD_SSR_AU_SIZE_RESERVED4     = 14,
                SD_SSR_AU_SIZE_RESERVED5     = 15
}SD_SSR_AU_SIZE;


/****************************************************/
/*  SD Status Register Structure                    */
/****************************************************/
typedef struct _ADI_RSI_SD_SSR_REGISTER{
    SD_SSR_DAT_BUS_WIDTH dat_bus_width;
    SD_SSR_SECURED_MODE  secured_mode;
    SD_SSR_SD_CARD_TYPE  sd_card_type;
    unsigned long           size_of_protected_area;
    SD_SSR_SPEED_CLASS   speed_class;
    unsigned char           performance_move;
    SD_SSR_AU_SIZE       au_size;
    unsigned short          erase_size;
    unsigned char           erase_timeout;
    unsigned char           erase_offset;
} ADI_RSI_SD_SSR_REGISTER;

/****************************************************/
/*  SD OCR Register                                 */
/****************************************************/
/****************************************************/
/*  SD OCR Register Bit Masks                       */
/****************************************************/
#define SD_OCR_LOW_VOLTAGE          (1<<7)
#define SD_OCR_VOLTAGE_27_28        (1<<15)
#define SD_OCR_VOLTAGE_28_29        (1<<16)
#define SD_OCR_VOLTAGE_29_30        (1<<17)
#define SD_OCR_VOLTAGE_30_31        (1<<18)
#define SD_OCR_VOLTAGE_31_32        (1<<19)
#define SD_OCR_VOLTAGE_32_33        (1<<20)
#define SD_OCR_VOLTAGE_33_34        (1<<21)
#define SD_OCR_VOLTAGE_34_35        (1<<22)
#define SD_OCR_VOLTAGE_35_36        (1<<23)
#define SD_OCR_CARD_CAPACITY_STATUS (1<<30)
#define SD_OCR_CARD_POWER_UP_STATUS (1<<31)

typedef  unsigned long ADI_RSI_SD_OCR_REGISTER;

/****************************************************/
/*  SD CID Register                                 */
/****************************************************/
/****************************************************/
/*  SD CID Register Structure                       */
/****************************************************/
typedef struct _ADI_RSI_SD_CID_REGISTER{
    unsigned char   mid;
    char            oid[2];
    char            pnm[5];
    unsigned char   prv;
    unsigned long   psn;
    unsigned short  mdt;
    unsigned char   crc;
} ADI_RSI_SD_CID_REGISTER;


/****************************************************/
/*  SD CSD Register Version 1.0 & 2.0               */
/****************************************************/
/****************************************************/
/*  SD CSD Register Version 1.0 & 2.0 Enumerations  */
/****************************************************/
typedef enum{
    SD_CSD_STRUCTURE_VERSION_1_0          = 0,
    SD_CSD_STRUCTURE_VERSION_2_0          = 1,
    SD_CSD_STRUCTURE_VERSION_RESERVED0    = 2,
    SD_CSD_STRUCTURE_VERSION_RESERVED1    = 3
}SD_CSD_STRUCTURE;

typedef enum{
    SD_CSD_VDD_CURR_MIN_0_5mA   = 0,
    SD_CSD_VDD_CURR_MIN_1_0mA   = 1,
    SD_CSD_VDD_CURR_MIN_5_0mA   = 2,
    SD_CSD_VDD_CURR_MIN_10_0mA  = 3,
    SD_CSD_VDD_CURR_MIN_25_0mA  = 4,
    SD_CSD_VDD_CURR_MIN_35_0mA  = 5,
    SD_CSD_VDD_CURR_MIN_60_0mA  = 6,
    SD_CSD_VDD_CURR_MIN_100_0mA = 7,
    SD_CSD_VDD_CURR_MIN_NOT_APPLICABLE = 8
}SD_CSD_VDD_CURR_MIN;

typedef enum{
    SD_CSD_VDD_CURR_MAX_1_0mA   = 0,
    SD_CSD_VDD_CURR_MAX_5_0mA   = 1,
    SD_CSD_VDD_CURR_MAX_10_0mA  = 2,
    SD_CSD_VDD_CURR_MAX_25_0mA  = 3,
    SD_CSD_VDD_CURR_MAX_35_0mA  = 4,
    SD_CSD_VDD_CURR_MAX_45_0mA  = 5,
    SD_CSD_VDD_CURR_MAX_80_0mA  = 6,
    SD_CSD_VDD_CURR_MAX_200_0mA = 7,
    SD_CSD_VDD_CURR_MAX_NOT_APPLICABLE = 8
}SD_CSD_VDD_CURR_MAX;

typedef enum{
    SD_CSD_FILE_FORMAT_HD_WITH_PARTITION_TABLE          = 0,
    SD_CSD_FILE_FORMAT_DOS_FAT_WITH_BOOT_SECTOR_ONLY    = 1,
    SD_CSD_FILE_FORMAT_UNIVERSAL_FILE_FORMAT            = 2,
    SD_CSD_FILE_FORMAT_OTHER_UNKNOWN                    = 3
}SD_CSD_FILE_FORMAT;

/********************************************************/
/*  SD CSD Register Version 1.0 and 2.0 Structure       */
/********************************************************/
/********************************************************/
/* For Version 2.0 support C_size has been extended     */
/* Vdd_curr_min and Vdd_curr_max values not applicable  */
/* C_SIZE_MULT not applicable                           */
/********************************************************/
typedef struct _ADI_RSI_SD_CSD_REGISTER{
    SD_CSD_STRUCTURE    csd_structure;
    unsigned char       taac;
    unsigned char       nsac;
    unsigned char       tran_speed;
    unsigned short      ccc;
    unsigned char       read_bl_len;
    bool                read_bl_partial;
    bool                write_blk_misalign;
    bool                read_blk_misalign;
    bool                dsr_imp;
    unsigned long       c_size;
    SD_CSD_VDD_CURR_MIN vdd_r_curr_min;
    SD_CSD_VDD_CURR_MAX vdd_r_curr_max;
    SD_CSD_VDD_CURR_MIN vdd_w_curr_min;
    SD_CSD_VDD_CURR_MAX vdd_w_curr_max;
    unsigned char       c_size_mult;
    bool                erase_blk_en;
    unsigned char       sector_size;
    unsigned char       wp_grp_size;
    bool                wp_grp_enable;
    unsigned char       r2w_factor;
    unsigned char       write_bl_length;
    bool                write_bl_partial;
    bool                file_format_grp;
    bool                copy;
    bool                perm_write_protect;
    bool                tmp_write_protect;
    SD_CSD_FILE_FORMAT  file_format;
    unsigned char       crc;
}ADI_RSI_SD_CSD_REGISTER;


/********************************************************/
/*  SD RCA Register                                     */
/********************************************************/    
typedef unsigned short ADI_RSI_SD_RCA_REGISTER;

/********************************************************/
/*  SD DSR Register                                     */
/********************************************************/    
typedef unsigned short ADI_RSI_SD_DSR_REGISTER;


/****************************************************/
/*  SD SCR Register                                 */
/****************************************************/
/****************************************************/
/*  SD SCR Register Enumerations                    */
/****************************************************/
typedef enum{
    SD_SCR_STRUCTURE_VERSION_NO_1_0    = 0,
    SD_SCR_STRUCTURE_VERSION_RESERVED0  = 1,
    SD_SCR_STRUCTURE_VERSION_RESERVED1  = 2,
    SD_SCR_STRUCTURE_VERSION_RESERVED2  = 3,
    SD_SCR_STRUCTURE_VERSION_RESERVED3  = 4,
    SD_SCR_STRUCTURE_VERSION_RESERVED4  = 5,
    SD_SCR_STRUCTURE_VERSION_RESERVED5  = 6,
    SD_SCR_STRUCTURE_VERSION_RESERVED6  = 7,
    SD_SCR_STRUCTURE_VERSION_RESERVED7  = 8,
    SD_SCR_STRUCTURE_VERSION_RESERVED8  = 9,
    SD_SCR_STRUCTURE_VERSION_RESERVED9  = 10,
    SD_SCR_STRUCTURE_VERSION_RESERVED10 = 11,
    SD_SCR_STRUCTURE_VERSION_RESERVED11 = 12,
    SD_SCR_STRUCTURE_VERSION_RESERVED12 = 13,
    SD_SCR_STRUCTURE_VERSION_RESERVED13 = 14,
    SD_SCR_STRUCTURE_VERSION_RESERVED14 = 15
}SD_SCR_STRUCTURE;

typedef enum{
    SD_SCR_SD_SPEC_VERSION_1_0_TO_1_01   = 0,
    SD_SCR_SD_SPEC_VERSION_1_10      = 1,
    SD_SCR_SD_SPEC_VERSION_2_00      = 2,
    SD_SCR_SD_SPEC_RESERVED0        = 3,
    SD_SCR_SD_SPEC_RESERVED1        = 4,
    SD_SCR_SD_SPEC_RESERVED2        = 5,
    SD_SCR_SD_SPEC_RESERVED3        = 6,
    SD_SCR_SD_SPEC_RESERVED4        = 7,
    SD_SCR_SD_SPEC_RESERVED5        = 8,
    SD_SCR_SD_SPEC_RESERVED6        = 9,
    SD_SCR_SD_SPEC_RESERVED7        = 10,
    SD_SCR_SD_SPEC_RESERVED8        = 11,
    SD_SCR_SD_SPEC_RESERVED9        = 12,
    SD_SCR_SD_SPEC_RESERVED10       = 13,
    SD_SCR_SD_SPEC_RESERVED11       = 14,
    SD_SCR_SD_SPEC_RESERVED12       = 15
}SD_SCR_SD_SPEC;


typedef enum{
    SD_SCR_SD_SECURITY_NO_SECURITY  = 0,
    SD_SCR_SD_SECURITY_NOT_USED     = 1,
    SD_SCR_SD_SECURITY_VERSION_1_01  = 2,
    SD_SCR_SD_SECURITY_VERSION_2_00  = 3,
    SD_SCR_SD_SECURITY_RESERVED0    = 4,
    SD_SCR_SD_SECURITY_RESERVED1    = 5,
    SD_SCR_SD_SECURITY_RESERVED2    = 6,
    SD_SCR_SD_SECURITY_RESERVED3    = 7
}SD_SCR_SD_SECURITY;

typedef enum{
    SD_SCR_SD_BUS_WIDTHS_1_BIT      = 0,
    SD_SCR_SD_BUS_WIDTHS_RESERVED0  = 1,
    SD_SCR_SD_BUS_WIDTHS_4_BIT      = 2,
    SD_SCR_SD_BUS_WIDTHS_RESERVED1  = 3,
    SD_SCR_SD_BUS_WIDTHS_1_BIT_AND_4_BIT = 5
}SD_SCR_SD_BUS_WIDTHS;

/********************************************************/
/*  SD SCR Register Structure                           */
/********************************************************/

typedef struct _ADI_RSI_SD_SCR_REGISTER{
    SD_SCR_STRUCTURE        scr_structure;
    SD_SCR_SD_SPEC          sd_spec;
    bool                    data_stat_after_erase;
    SD_SCR_SD_SECURITY      sd_security;
    SD_SCR_SD_BUS_WIDTHS    sd_bus_widths;
}ADI_RSI_SD_SCR_REGISTER;


/********************************************************/
/*  SD Card Register Structure                          */
/********************************************************/
typedef struct _ADI_RSI_SD_CARD_REGISTERS{
    ADI_RSI_SD_CSR_REGISTER csr;
    ADI_RSI_SD_SSR_REGISTER ssr;
    ADI_RSI_SD_OCR_REGISTER ocr;
    ADI_RSI_SD_CID_REGISTER cid;
    ADI_RSI_SD_CSD_REGISTER csd;
    ADI_RSI_SD_RCA_REGISTER rca;
    ADI_RSI_SD_DSR_REGISTER dsr;
    ADI_RSI_SD_SCR_REGISTER scr;
}ADI_RSI_SD_CARD_REGISTERS;   



/*********************************************************************
Macros specific to SD/SDIO/Multimedia card configuration
*********************************************************************/

/* Vdd operating range for ADSP-BF518 EZ-KIT */
#define     SD_MMC_VDD_OPERATING_RANGE              0x00FF8000      /* range - 2.7 to 3.6 volts (used as default) */
#define     SD_MMC_SECTOR_MODE_SUPPORT              0x40000000
#define     SD_MMC_BYTE_MODE_SUPPORT                0x00000000
#define     MMC_RCA                                 0x5A7E


#define NO_RESPONSE     0x00
#define R1_RESPONSE     0x40
#define R1B_RESPONSE    0x40
#define R2_RESPONSE     0xC0
#define R3_RESPONSE     0x40
#define R6_RESPONSE     0x40
#define R7_RESPONSE     0x40


/******** Commands common to SD/SDIO/Multimedia cards ********/

#define     SD_MMC_CMD0      0      /* Resets all cards to idle state */
#define     SD_MMC_CMD2      2      /* Requests the CID numbers on CMD line of all connected cards */ 
#define     SD_MMC_CMD3      3      /* Send relative address(SD/SDIO/SDHC), Set relative address (MMC)  */
#define     SD_MMC_CMD4      4      /* Programs the DSR of all cards */
#define     SDIO_MMC_CMD5    5
#define     SD_MMC_CMD6      6      /* Select/De-select card    */
#define     SD_MMC_CMD7      7      /* Select/De-select card                                            */
#define     SD_MMC_CMD8      8      /* Send SD Memory card interface condition (SDHC)                   */
#define     SD_MMC_CMD9      9      /* Send Card Specific Data (CSD)                                    */
#define     SD_MMC_CMD10    10      /* Send Card Identification Data (CID)                              */
#define     SD_MMC_CMD12    12      /* Stop Transmission                                                */
#define     SD_MMC_CMD13    13      /* Send Status                                                      */
#define     SD_MMC_CMD15    15      /* Go Inactive State                                                */
#define     SD_MMC_CMD16    16      /* Set Block Length                                                 */
#define     SD_MMC_CMD17    17      /* Read Single Block                                                */
#define     SD_MMC_CMD18    18      /* Read Multiple Blocks                                             */
#define     SD_MMC_CMD24    24      /* Write Single Block                                               */
#define     SD_MMC_CMD25    25      /* Write Multiple Blocks                                            */
#define     SD_MMC_CMD27    27      /* Program CSD                                                      */
#define     SD_MMC_CMD28    28      /* Set Write Protect                                                */
#define     SD_MMC_CMD29    29      /* Clear Write Protect                                              */
#define     SD_MMC_CMD30    30      /* Send Write Protect                                               */
#define     SD_MMC_CMD35    35
#define     SD_MMC_CMD36    36
#define     SD_MMC_CMD38    38      /* Erase selected blocks                                            */
#define     SD_MMC_CMD42    42      /* Lock/Unlock card (set/reset card password)                       */
#define     SD_MMC_CMD55    55
#define     SD_MMC_CMD56    56
/******** Commands specific to Multimedia card ********/

#define     MMC_CMD1         1      /* Reads MMC OCR value */
#define     MMC_CMD11       11
#define     MMC_CMD14       14
#define     MMC_CMD19       19
#define     MMC_CMD20       20 
#define     MMC_CMD23       23
#define     MMC_CMD26       26                                            
/*#define     MMC_CMD34       34*/      /* Untag block (sector for MMC) selected for erase                  */
#define     MMC_CMD39       39    
#define     MMC_CMD40       40      /* Go Interrupt request state */

/******** Commands specific to SD/SDIO cards ********/

/*#define     SD_CMD6          6*/      /* Checks switchable mode & switch card function                    */
#define     SD_CMD32        32      /* Erase write block start                                          */
#define     SD_CMD33        33      /* Erase write block end  */
#define     SD_CMD34        34
#define     SD_CMD37        37
#define     SD_CMD50        50
#define     SD_CMD57        57

#define     SD_ACMD6         6      /* Set bus width                                                    */
#define     SD_ACMD13       13      /* Reads SD memory card status                                      */
#define     SD_ACMD22       22      /* Reads number of written write blocks in SD memory card           */
#define     SD_ACMD41       41      /* Reads SD memory card OCR value                                   */
#define     SD_ACMD42       42      /* Connect/disconnect pullup on DAT3 line                           */
#define     SD_ACMD51       51      /* Reads SD Configuration register                                  */

/******** Commands specific to SDIO cards ********/
#define     SDIO_CMD52      52      /* IO read/write direct - SDIO command                              */
#define     SDIO_CMD53      53      /* Command used for SDIO block operations                           */

/*********************************************************************

SD/SDIO/MM card commands - in terms of command classes

*********************************************************************/

/******** Basic Commands (Command Class 0)********/

#define     SD_MMC_CMD_GO_IDLE_STATE            SD_MMC_CMD0 | NO_RESPONSE    /* Go Idle state                                                    */ 
#define     MMC_CMD_SEND_OP_COND                MMC_CMD1 | R3_RESPONSE        /* Reads MMC OCR value                                              */
#define     SD_MMC_CMD_ALL_SEND_CID             SD_MMC_CMD2 | R2_RESPONSE    /* Get CID of all cards in the bus                                  */ 
#define     SD_CMD_SEND_RELATIVE_ADDR           SD_MMC_CMD3 | R6_RESPONSE     /* Send relative address (SD/SDIO/SDHC), Set relative address (MMC) */
#define     MMC_CMD_SET_RELATIVE_ADDR           SD_MMC_CMD3 | R6_RESPONSE     /* Send relative address (SD/SDIO/SDHC), Set relative address (MMC) */
#define     SD_MMC_CMD_SET_DSR                  SD_MMC_CMD4 | NO_RESPONSE     /* Send relative address (SD/SDIO/SDHC), Set relative address (MMC) */
#define     MMC_CMD_SLEEP_AWAKE                 MMC_CMD5 | R1B_RESPONSE
#define     MMC_CMD_SWITCH                      SD_MMC_CMD6 | R1B_RESPONSE
#define     SD_MMC_CMD_SELECT_DESELECT_CARD     SD_MMC_CMD7 | R1B_RESPONSE     /* Select card                                                      */
#define     SD_CMD_SEND_IF_COND                 SD_MMC_CMD8 | R7_RESPONSE     /* Send SD Card interface condition (SDHC)                          */
#define     MMC_CMD_SEND_EXT_CSD                 SD_MMC_CMD8 | R1_RESPONSE     /* Send SD Card interface condition (SDHC)                          */
#define     SD_MMC_CMD_SEND_CSD                 SD_MMC_CMD9 | R2_RESPONSE   /* Send Card Specific Data (CSD)                                    */
#define     SD_MMC_CMD_SEND_CID                 SD_MMC_CMD10 | R2_RESPONSE   /* Send Card Identification Data (CID)                              */
#define     SD_MMC_CMD_STOP_TRANSMISSION        SD_MMC_CMD12 | R1B_RESPONSE   /* Stop Transmission                                                */
#define     SD_MMC_CMD_SEND_STATUS              SD_MMC_CMD13 | R1_RESPONSE  /* read addressed card's Status register                            */
#define     SD_MMC_CMD_GO_INACTIVE_STATE        SD_MMC_CMD15 | NO_RESPONSE  /* Go Inactive State                                                */


/******** Block Read Commands (Command Class 2)********/
#define     SD_MMC_CMD_SET_BLOCKLEN             SD_MMC_CMD16 | R1_RESPONSE  /* Set Read/Write Block Length                                      */
#define     SD_MMC_CMD_READ_SINGLE_BLOCK        SD_MMC_CMD17 | R1_RESPONSE   /* Read Single Block                                                */
#define     SD_MMC_CMD_READ_MULTIPLE_BLOCK      SD_MMC_CMD18 | R1_RESPONSE   /* Read Multiple Blocks                                             */

/******** Block Write Commands (Command Class 4)********/
#define     SD_MMC_CMD_WRITE_SINGLE_BLOCK       SD_MMC_CMD24 | R1_RESPONSE    /* Write Single Block                                               */
#define     SD_MMC_CMD_WRITE_MULTIPLE_BLOCK     SD_MMC_CMD25 | R1_RESPONSE   /* Write Multiple Blocks                                            */
#define     SD_MMC_CMD_PROGRAM_CSD              SD_MMC_CMD27 | R1_RESPONSE   /* Program CSD                                                      */

/******** Erase Commands (Command Class 5)********/
#define     SD_CMD_ERASE_WR_BLK_START          SD_MMC_CMD32 | R1_RESPONSE    /* Erase write block start                                          */
#define     SD_CMD_ERASE_WR_BLK_END            SD_MMC_CMD33 | R1_RESPONSE   /* Erase write block end                                            */
#define     SD_MMC_CMD_ERASE                    SD_MMC_CMD38 | R1B_RESPONSE   /* Erase selected blocks                                            */
#define     MMC_CMD_UNTAG_BLOCK                 MMC_CMD34       /* Untag block selected for erase                                   */

/******** Write Protection Commands (Command Class 6)********/
#define     SD_MMC_CMD_SET_WRITE_PROT           SD_MMC_CMD28 | R1B_RESPONSE   /* Set Write Protect                                                */
#define     SD_MMC_CMD_CLR_WRITE_PROT           SD_MMC_CMD29 | R1B_RESPONSE   /* Clear Write Protect                                              */
#define     SD_MMC_CMD_SEND_WRITE_PROT          SD_MMC_CMD30 | R1_RESPONSE   /* Send Write Protect                                               */

/******** Lock Card Commands (Command Class 7)********/
#define     SD_MMC_CMD_LOCK_UNLOCK              SD_MMC_CMD42        /* Lock card (set card password)                                    */

/******** Application Specific Commands (Command Class 8)********/

#define     SD_MMC_CMD_APP_CMD                  SD_MMC_CMD55 | R1_RESPONSE        /* Set SD card for application specific command                     */
#define     SD_CMD_SET_BUS_WIDTH                SD_ACMD6 | R1_RESPONSE        /* Set bus width                                                    */
#define     SD_CMD_GET_MEMORY_STATUS            SD_ACMD13 | R1_RESPONSE       /* Reads SD memory card status                                      */
#define     SD_CMD_GET_WRITTEN_BLOCK_COUNT      SD_ACMD22 | R1_RESPONSE       /* Reads number of written write blocks in SD memory card           */
#define     SD_CMD_GET_OCR_VALUE                SD_ACMD41 | R3_RESPONSE       /* Reads SD memory card OCR value                                   */
#define     SD_CMD_DISCONNECT_DAT3_PULLUP       SD_ACMD42 | R1_RESPONSE      /* disconnect pullup on DAT3 line                                   */
#define     SD_CMD_GET_CONFIG_REG               SD_ACMD51 | R1_RESPONSE     /* Reads SD Configuration register                                  */

/******** I/O Mode Commands (Command Class 9)********/

#define     SDIO_CMD_GET_SUPPORT                SDIO_CMD5       /* Enquire SDIO support                                             */
#define     SDIO_CMD_READ_DIRECT                SDIO_CMD52      /* SDIO read direct                                                 */
#define     SDIO_CMD_WRITE_DIRECT               SDIO_CMD52      /* SDIO write direct                                                */
#define     SDIO_CMD_BLOCK                      SDIO_CMD53      /* Command used for SDIO block operations                           */
#define     MMC_CMD_GO_IRQ_STATE                MMC_CMD40       /* Go Interrupt request state                                       */

/******** Switch Commands (Command Class 10)********/
/*#define     SD_CMD_GET_SWITCH_MODE_INFO         SD_CMD6*/         /* Checks switchable mode & switch card function                    */

#define     MMC_BUSTEST_W       MMC_CMD19 | R1_RESPONSE
#define     MMC_BUSTEST_R       MMC_CMD14 | R1_RESPONSE


typedef enum
{
    SD_MMC_CARD_TYPE_SD_V1X,
    SD_MMC_CARD_TYPE_SD_V2X,
    SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY,
    SD_MMC_CARD_TYPE_MMC,
    SD_MMC_CARD_TYPE_MMC_CARD_HIGH_CAPACITY,
    UNUSABLE_CARD,
    CARD_SLOT_NOT_INITIALIZED
} SD_MMC_CARD_TYPE;

typedef enum
{
    MMC_SPEED_GRADE_20MHZ = 20000000,
    MMC_SPEED_GRADE_26MHZ = 26000000,
    MMC_SPEED_GRADE_52MHZ = 52000000,
    SD_SPEED_GRADE_CLASS0 = 25000000,
    SD_SPEED_GRADE_CLASS2 = 30000000,
    SD_SPEED_GRADE_CLASS4 = 40000000,
    SD_SPEED_GRADE_CLASS6 = 50000000,
    SD_MMC_SPEED_GRADE_ERROR
} SD_MMC_SPEED_GRADE;

typedef enum
{
    RSI_DATA_BUS_WIDTH_ERROR = 0,
    RSI_DATA_BUS_WIDTH_1 = 1,
    RSI_DATA_BUS_WIDTH_4 = 4,
    RSI_DATA_BUS_WIDTH_8 = 8
} RSI_DATA_BUS_WIDTH;


/* structure for holding all MMC and SD card details */
typedef struct {
	ADI_RSI_SD_CARD_REGISTERS sd;	/* SD card details */
	SD_MMC_CARD_TYPE type;
	RSI_DATA_BUS_WIDTH bus_width;
	uint32_t cclk;
	uint32_t sclk;
	uint32_t rsiclk;
} ADI_RSI_CARD_REGISTERS;

int      rsi_get_sd_cart_type_and_speed_grade(void*);
int      rsi_setup(void);
uint32_t rsi_write_blocks_to_sd_card(uint32_t, void *, uint32_t);
uint32_t rsi_read_blocks_from_sd_card(uint32_t, void *, uint32_t);
uint32_t rsi_get_sec_count(void);
void     rsi_power_off(void);

void rsi_get_card_timeout(SD_CARD_ERROR_STRUCT*);

#endif /* __RSI_H__ */
