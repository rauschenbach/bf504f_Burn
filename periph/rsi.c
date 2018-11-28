/************************************************************************************************************
 * Analog Devices, Inc. All Rights Reserved.
 * Description:    This file tests the SD interface on the EZ-KIT.
 ***********************************************************************************************************/
#include <stdlib.h>
#include <string.h>
#include "timer0.h"
#include "rsi.h"
#include "led.h"


#define CARD_TIMER_TIMEOUT	12000000

/* local function declarations */
static void SDBlockRead(void *, uint32_t);
static void SDBlockWrite(void *, uint32_t);
static int rsi_wait_for_card(void);
static void rsi_set_optimum_identification_speed(void);
static void rsi_get_pll_clocks(void);
static void rsi_set_optimum_sd_speed(void);
static uint32_t rsi_send_command(uint16_t, uint32_t);	/* sends a command to over the RSI */

static SD_MMC_CARD_TYPE rsi_identification(void);
static SD_MMC_CARD_TYPE rsi_identification_sdv2(uint32_t);	/* performs SD Version 2.0 or later card detection */
static SD_MMC_CARD_TYPE rsi_identification_sdv1(void);	/* performs SD Version 1.x or earlier card detection */
static uint32_t rsi_set_bus_width_4_bits(void);
static uint32_t rsi_get_sd_cid(void);
static uint32_t rsi_get_sd_scr_register(void);
static uint32_t rsi_get_sd_ssr_register(void);
static uint32_t rsi_get_wait_until_ready(void);
static uint32_t rsi_send_csd(void);

static uint32_t rsi_write_multi_blocks_dma(uint32_t, void *, uint32_t);
static uint32_t rsi_write_single_block_dma(uint32_t, void *);
static uint32_t rsi_read_multi_blocks_dma(uint32_t, void *, uint32_t);
static uint32_t rsi_read_single_block_dma(uint32_t, void *);




/* Структура держит параметры SD карты   */
static ADI_RSI_CARD_REGISTERS adi_rsi_card_registers;
static uint8_t dma_buffer[64];


static SD_CARD_ERROR_STRUCT sd_card_error;



/* Выдать параметры SD карты */
section("L1_code")
int rsi_get_sd_cart_type_and_speed_grade(void *par)
{
    /* Скопируем параметры  */
    memcpy(par, &adi_rsi_card_registers, sizeof(ADI_RSI_CARD_REGISTERS));
    return sizeof(ADI_RSI_CARD_REGISTERS);
}

/**
 * Перещитать адрес карты, если она невысокой плотности
 */
section("L1_code")
static uint32_t card_address(uint32_t block_num)
{
    return (adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY) ? block_num : block_num * 512;
}

/**
 * Найти оптимальный CLK карты 
 */
section("L1_code")
static void rsi_set_optimum_sd_speed(void)
{
    uint32_t clkdiv = 0;
    uint32_t card_speed = 25000000;
    uint32_t rsi_freq;
    uint32_t sclk = adi_rsi_card_registers.sclk;

    /* Setting RSI clock frequency to %dHz, by enabling CLKDIV_BYPASS */
    if (sclk <= card_speed) {
	*pRSI_CLK_CONTROL |= CLKDIV_BYPASS;
    } else {
	do {
	    rsi_freq = (sclk / (2 * (clkdiv + 1)));
	    clkdiv++;
	} while (rsi_freq > card_speed);

	clkdiv--;
	*pRSI_CLK_CONTROL = (*pRSI_CLK_CONTROL & 0xFF00) | clkdiv;
	adi_rsi_card_registers.rsiclk = rsi_freq;
    }
}



/**
 * Определить, какая карта у нас вставлена. От определения зависит способ работы с ней.
 */
section("L1_code")
static SD_MMC_CARD_TYPE rsi_identification(void)
{
    uint32_t error;
    uint32_t response;

    /* Sending GO_IDLE_STATE command before performing card identification */
    error = rsi_send_command(SD_MMC_CMD_GO_IDLE_STATE, 0);

    /* Sending SEND_IF_COND command for 2.7V to 3.6V compatibility. Starting SD Version 2.0 identification. */
    /* SD_MMC_CMD8 | R7_RESPONSE  */
    error = rsi_send_command(SD_CMD_SEND_IF_COND, 0x000001AA);

    if (!error) {
	/* Card is compatible, likely an SD Version 2.0 compliant card. It's a version 2.0 or later SD memory card */
	response = *pRSI_RESPONSE0;	/* get the response to send to the identification routine */
	adi_rsi_card_registers.type = rsi_identification_sdv2(response);
    } else {
	/* Card is not compatible or did not respond. Starting SD Version 1.x identification */
	adi_rsi_card_registers.type = rsi_identification_sdv1();	/* check if it's a version 1.X SD memory card */
    }

    /* Для всех карт всех версий  */
    if (adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V1X || adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X ||
	adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY) {

	error = rsi_get_sd_cid();	/* Requesting CID register */
	error = rsi_send_command(SD_CMD_SEND_RELATIVE_ADDR, 0);	/* Requesting RCA */
	if (!error) {
	    *pRSI_PWR_CONTROL &= 0x3;
	    adi_rsi_card_registers.sd.rca = (*pRSI_RESPONSE0 & 0xFFFF0000) >> 16;

	    /* Requesting Card Status register */
	    error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, (adi_rsi_card_registers.sd.rca << 16));
	    adi_rsi_card_registers.sd.csr = *pRSI_RESPONSE0;

	    error = rsi_send_csd();	/* Requesting CSD register */
	    if (error) {
		adi_rsi_card_registers.type = CARD_SLOT_NOT_INITIALIZED;
		goto met;
	    }
	    // здесь ощибка на одной из карт!!!
	    error = rsi_get_sd_scr_register();	/* Requesting SCR register */
	    if (error) {
		adi_rsi_card_registers.type = CARD_SLOT_NOT_INITIALIZED;
		goto met;
	    }

	    rsi_set_optimum_sd_speed();	/* Optimizing RSI interface speed */

	    adi_rsi_card_registers.bus_width = RSI_DATA_BUS_WIDTH_4;
	    error = rsi_set_bus_width_4_bits();	/* Setting RSI bus width to 4-bit */
	    if (error) {
		adi_rsi_card_registers.type = CARD_SLOT_NOT_INITIALIZED;
		goto met;
	    }

	    error = rsi_get_sd_ssr_register();	/* Requesting SSR register */
	    if (error) {
		adi_rsi_card_registers.type = CARD_SLOT_NOT_INITIALIZED;
		goto met;
	    }
	}
    }
  met:
    return adi_rsi_card_registers.type;
}

/* Проверить эту функцию!!! На одной черной карте не работает!!!   */
section("L1_code")
static uint32_t rsi_get_sd_scr_register(void)
{
    uint32_t error = 0;
    int t0;

    *pRSI_DATA_LGTH = 8;
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT;	/* таймаут в тиках тактовой частоты = полсекунды */

    /* SD_MMC_CMD13 | R1_RESPONSE - read addressed card's Status register  */
    error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, adi_rsi_card_registers.sd.rca << 16);

    if ((*pRSI_RESPONSE0 & SD_CSR_CURRENT_STATE) == SD_CSR_CURRENT_STATE_STANDBY) {
	error = rsi_send_command(SD_MMC_CMD_SELECT_DESELECT_CARD, adi_rsi_card_registers.sd.rca << 16);
    }

    SDBlockRead((void *) &dma_buffer[0], 8);

    /* SD_MMC_CMD55 | R1_RESPONSE - Set SD card for application specific command */
    error = rsi_send_command(SD_MMC_CMD_APP_CMD, adi_rsi_card_registers.sd.rca << 16);

    /* SD_ACMD51 | R1_RESPONSE */
    error = rsi_send_command(SD_CMD_GET_CONFIG_REG, 0);

    if (!error) {
	int t0 = 50;
	*pRSI_DATA_CONTROL = 0x3B;

/* Убрал эту проверку */
	while (!(*pRSI_STATUS & DAT_BLK_END) && t0--) {
		delay_ms(25);
	}
#if 0	
	if (t0 <= 0) {
	   return CMD_TIMEOUT;
	}
#endif
	
	*pRSI_STATUSCL = DAT_BLK_END_STAT | DAT_END_STAT;

	/* clear the DMA Done IRQ */
	*pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

	adi_rsi_card_registers.sd.scr.scr_structure = (SD_SCR_STRUCTURE) ((dma_buffer[0] & 0xF0) >> 4);
	adi_rsi_card_registers.sd.scr.sd_spec = (SD_SCR_SD_SPEC) (dma_buffer[0] & 0x0F);
	adi_rsi_card_registers.sd.scr.data_stat_after_erase = (dma_buffer[1] & 0x80) >> 7;
	adi_rsi_card_registers.sd.scr.sd_security = (SD_SCR_SD_SECURITY) ((dma_buffer[1] & 0x70) >> 4);
	adi_rsi_card_registers.sd.scr.sd_bus_widths = (SD_SCR_SD_BUS_WIDTHS) (dma_buffer[1] & 0x0F);
    }

    return error;
}


/**
 * Проверить что наша карта SD Version 2.0
 */
section("L1_code")
static SD_MMC_CARD_TYPE rsi_identification_sdv2(uint32_t response)
{
    uint32_t error;
    SD_MMC_CARD_TYPE type = UNUSABLE_CARD;

    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT * 2;	/* таймаут в тиках тактовой частоты секунда */
    ssync();


    if (response & 0x00000100) {

	if ((response & 0x000000FF) == 0xAA) {

	    /* Echo back patter correct */
	    type = SD_MMC_CARD_TYPE_SD_V2X;
	    do {
	         /* SD_MMC_CMD55 | R1_RESPONSE  */
		error = rsi_send_command(SD_MMC_CMD_APP_CMD, 0);	/* card with compatible voltage range */

		/* SD_ACMD41 | R3_RESPONSE */
		error = rsi_send_command(SD_CMD_GET_OCR_VALUE, 0x40FF8000);	/* Requesting OCR register */

		if (((error & CMD_TIMEOUT) == CMD_TIMEOUT) || (!(*pRSI_RESPONSE0 & 0x00FF8000))) {
		    type = UNUSABLE_CARD;	/* Card not responding or invalid operating condition, setting card type to UNUSABLE_CARD */
		    break;
		} else {
		    adi_rsi_card_registers.sd.ocr = *pRSI_RESPONSE0;	/* OCR register received */
		}
	    } while (!(adi_rsi_card_registers.sd.ocr & (unsigned int) SD_OCR_CARD_POWER_UP_STATUS));

	    /* SD Version 2.0 or later Standard Capacity или Высокой плотности? */
	    if (type == SD_MMC_CARD_TYPE_SD_V2X) {
		if (adi_rsi_card_registers.sd.ocr & SD_OCR_CARD_CAPACITY_STATUS) {
		    type = SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY;	/* SD Version 2.0 or later High Capacity card detected successfully! */
		}
	    }
	}
    }
    return type;
}

/**
 * Проверить, что карта или SD ver 1.x или вообще непонятно какая карта
 * В этом месте виснет!!!
 */
section("L1_code")
static SD_MMC_CARD_TYPE rsi_identification_sdv1(void)
{
    uint32_t error;

    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT * 2;	/* таймаут в тиках тактовой частоты секунда */
    ssync();

    SD_MMC_CARD_TYPE type = SD_MMC_CARD_TYPE_SD_V1X;

    do {
	/* It's either a:                                               
	 *  version 2.00 or later memory card with a voltage mismatch   
	 *  version 1.x SD memory card                                  
	 *  MMC card */
	error = rsi_send_command(SD_MMC_CMD_APP_CMD, 0);
	error = rsi_send_command(SD_CMD_GET_OCR_VALUE, 0x00FF8000);	/* Requesting OCR register */

	/* Не sd карта а х.з.ч. Если случился таймаут */
	if ((error & CMD_TIMEOUT) == CMD_TIMEOUT) {
	    type = UNUSABLE_CARD;
	    break;
	} else {
	    adi_rsi_card_registers.sd.ocr = *pRSI_RESPONSE0;	/* OCR register received */
	}
    } while (!(adi_rsi_card_registers.sd.ocr & (unsigned int) SD_OCR_CARD_POWER_UP_STATUS));

    return type;		/* SD Version 1.x card detected successfully or not an SD card  */
}

section("L1_code")
static uint32_t rsi_get_sd_cid(void)
{
    uint32_t error;
    error = rsi_send_command(SD_MMC_CMD_ALL_SEND_CID, 0);
    if (!error) {
	adi_rsi_card_registers.sd.cid.mid = (*pRSI_RESPONSE0 & 0xFF000000) >> 24;
	adi_rsi_card_registers.sd.cid.oid[0] = (*pRSI_RESPONSE0 & 0x00FF0000) >> 16;
	adi_rsi_card_registers.sd.cid.oid[1] = (*pRSI_RESPONSE0 & 0x0000FF00) >> 8;
	adi_rsi_card_registers.sd.cid.pnm[0] = (*pRSI_RESPONSE0 & 0x000000FF);
	adi_rsi_card_registers.sd.cid.pnm[1] = (*pRSI_RESPONSE1 & 0xFF000000) >> 24;
	adi_rsi_card_registers.sd.cid.pnm[2] = (*pRSI_RESPONSE1 & 0x00FF0000) >> 16;
	adi_rsi_card_registers.sd.cid.pnm[3] = (*pRSI_RESPONSE1 & 0x0000FF00) >> 8;
	adi_rsi_card_registers.sd.cid.pnm[4] = (*pRSI_RESPONSE1 & 0x000000FF);
	adi_rsi_card_registers.sd.cid.prv = (*pRSI_RESPONSE2 & 0xFF000000) >> 24;
	adi_rsi_card_registers.sd.cid.psn = ((*pRSI_RESPONSE2 & 0x00FFFFFF) << 8) | ((*pRSI_RESPONSE3 & (0x7F8000000 >> 1)) >> 22);
	adi_rsi_card_registers.sd.cid.mdt = (*pRSI_RESPONSE3 & (0x0007FF80 >> 1)) >> 6;
	adi_rsi_card_registers.sd.cid.crc = (*pRSI_RESPONSE3 & (0x0000007F >> 1));
    }
    return error;
}

section("L1_code")
static uint32_t rsi_send_csd(void)
{
    uint32_t error;

    if (adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V1X || adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X ||
	adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY) {
	error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, adi_rsi_card_registers.sd.rca << 16);

	if ((*pRSI_RESPONSE0 & SD_CSR_CURRENT_STATE) == SD_CSR_CURRENT_STATE_TRANSFER) {
	    error = rsi_send_command(SD_MMC_CMD_SELECT_DESELECT_CARD, ~adi_rsi_card_registers.sd.rca << 16);
	}

	error = rsi_send_command(SD_MMC_CMD_SEND_CSD, (adi_rsi_card_registers.sd.rca << 16));

	if (error == 0) {
	    adi_rsi_card_registers.sd.csd.csd_structure = (SD_CSD_STRUCTURE) ((*pRSI_RESPONSE0 & 0xC0000000) >> 30);
	    adi_rsi_card_registers.sd.csd.taac = (*pRSI_RESPONSE0 & 0x00FF0000) >> 16;
	    adi_rsi_card_registers.sd.csd.nsac = (*pRSI_RESPONSE0 & 0x0000FF00) >> 8;
	    adi_rsi_card_registers.sd.csd.tran_speed = *pRSI_RESPONSE0 & 0x000000FF;
	    adi_rsi_card_registers.sd.csd.ccc = (*pRSI_RESPONSE1 & 0xFFF00000) >> 20;
	    adi_rsi_card_registers.sd.csd.read_bl_len = (*pRSI_RESPONSE1 & 0x000F0000) >> 16;
	    adi_rsi_card_registers.sd.csd.read_bl_partial = (*pRSI_RESPONSE1 & 0x00008000) >> 15;
	    adi_rsi_card_registers.sd.csd.write_blk_misalign = (*pRSI_RESPONSE1 & 0x00004000) >> 14;
	    adi_rsi_card_registers.sd.csd.read_blk_misalign = (*pRSI_RESPONSE1 & 0x00002000) >> 13;
	    adi_rsi_card_registers.sd.csd.dsr_imp = (*pRSI_RESPONSE1 & 0x00001000) >> 12;

	    if (adi_rsi_card_registers.sd.csd.csd_structure == SD_CSD_STRUCTURE_VERSION_1_0) {
		adi_rsi_card_registers.sd.csd.c_size = ((*pRSI_RESPONSE1 & 0x000003FF) << 2) | ((*pRSI_RESPONSE2 & 0xC0000000) >> 30);
		adi_rsi_card_registers.sd.csd.vdd_r_curr_min = (SD_CSD_VDD_CURR_MIN) (*pRSI_RESPONSE2 & 0x38000000) >> 27;
		adi_rsi_card_registers.sd.csd.vdd_r_curr_max = (SD_CSD_VDD_CURR_MAX) (*pRSI_RESPONSE2 & 0x07000000) >> 24;
		adi_rsi_card_registers.sd.csd.vdd_w_curr_min = (SD_CSD_VDD_CURR_MIN) (*pRSI_RESPONSE2 & 0x00E00000) >> 21;
		adi_rsi_card_registers.sd.csd.vdd_w_curr_max = (SD_CSD_VDD_CURR_MAX) (*pRSI_RESPONSE2 & 0x001C0000) >> 18;
		adi_rsi_card_registers.sd.csd.c_size_mult = (*pRSI_RESPONSE2 & 0x00038000) >> 15;
	    } else if (adi_rsi_card_registers.sd.csd.csd_structure == SD_CSD_STRUCTURE_VERSION_2_0) {
		adi_rsi_card_registers.sd.csd.vdd_r_curr_min = SD_CSD_VDD_CURR_MIN_NOT_APPLICABLE;
		adi_rsi_card_registers.sd.csd.vdd_r_curr_max = SD_CSD_VDD_CURR_MAX_NOT_APPLICABLE;
		adi_rsi_card_registers.sd.csd.vdd_w_curr_min = SD_CSD_VDD_CURR_MIN_NOT_APPLICABLE;
		adi_rsi_card_registers.sd.csd.vdd_w_curr_max = SD_CSD_VDD_CURR_MAX_NOT_APPLICABLE;
		adi_rsi_card_registers.sd.csd.c_size_mult = 0;
		adi_rsi_card_registers.sd.csd.c_size = ((*pRSI_RESPONSE1 & 0x0000003F) << 16) | ((*pRSI_RESPONSE2 & 0xFFFF0000) >> 16);
	    } else {
		/* error. Непонятная версия карты */
		return (u32) - 1;
	    }
	    adi_rsi_card_registers.sd.csd.erase_blk_en = (*pRSI_RESPONSE2 & 0x00004000) >> 14;
	    adi_rsi_card_registers.sd.csd.sector_size = (*pRSI_RESPONSE2 & 0x00003F80) >> 7;
	    adi_rsi_card_registers.sd.csd.wp_grp_size = *pRSI_RESPONSE2 & 0x0000007F;
	    adi_rsi_card_registers.sd.csd.wp_grp_enable = (*pRSI_RESPONSE3 & (0x80000000 >> 1)) >> 30;
	    adi_rsi_card_registers.sd.csd.r2w_factor = (*pRSI_RESPONSE3 & (0x1C000000 >> 1)) >> 25;
	    adi_rsi_card_registers.sd.csd.write_bl_length = (*pRSI_RESPONSE3 & (0x03C00000 >> 1)) >> 21;
	    adi_rsi_card_registers.sd.csd.write_bl_partial = (*pRSI_RESPONSE3 & (0x00200000 >> 1)) >> 20;
	    adi_rsi_card_registers.sd.csd.file_format_grp = (*pRSI_RESPONSE3 & (0x00008000 >> 1)) >> 14;
	    adi_rsi_card_registers.sd.csd.copy = (*pRSI_RESPONSE3 & (0x00004000 >> 1)) >> 13;
	    adi_rsi_card_registers.sd.csd.perm_write_protect = (*pRSI_RESPONSE3 & (0x00002000 >> 1)) >> 12;
	    adi_rsi_card_registers.sd.csd.tmp_write_protect = (*pRSI_RESPONSE3 & (0x00001000 >> 1)) >> 11;
	    adi_rsi_card_registers.sd.csd.file_format = (SD_CSD_FILE_FORMAT) (*pRSI_RESPONSE3 & (0x00000C00 >> 1)) >> 9;
	    adi_rsi_card_registers.sd.csd.crc = (*pRSI_RESPONSE3 & (0x000000FE >> 1)) >> 0;
	}
    }
    return error;
}


/**
 * Установить 4-битную шину
*/
section("L1_code")
static uint32_t rsi_set_bus_width_4_bits(void)
{
    uint32_t error = CARD_SLOT_NOT_INITIALIZED;
    uint32_t argument;
    uint32_t clock_control_reg = *pRSI_CLK_CONTROL & 0xE7FF;

    if ((adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V1X) || (adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X)
	|| (adi_rsi_card_registers.type == SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY)) {

	error = rsi_send_command(SD_MMC_CMD_APP_CMD, adi_rsi_card_registers.sd.rca << 16);
	error = rsi_send_command(SD_CMD_DISCONNECT_DAT3_PULLUP, 0);
	error = rsi_send_command(SD_MMC_CMD_APP_CMD, adi_rsi_card_registers.sd.rca << 16);
	error = rsi_send_command(SD_CMD_SET_BUS_WIDTH, 2);
	if (!error) {
	    *pRSI_CLK_CONTROL = clock_control_reg | 0x0800;
	}
    }
    return error;
}

section("L1_code")
static uint32_t rsi_get_wait_until_ready(void)
{
    uint32_t error = 0;
    uint32_t rca = 0;
    uint32_t status = 0;

    switch (adi_rsi_card_registers.type) {
    case SD_MMC_CARD_TYPE_SD_V1X:
    case SD_MMC_CARD_TYPE_SD_V2X:
    case SD_MMC_CARD_TYPE_SD_V2X_HIGH_CAPACITY:
	rca = adi_rsi_card_registers.sd.rca << 16;
	break;
    case UNUSABLE_CARD:
    case CARD_SLOT_NOT_INITIALIZED:
	error = 1;
	break;
    default:
	error = 1;
    }

    if (!error) {
	while ((status & 0x00000100) != 0x00000100) {
	    error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, rca);
	    status = *pRSI_RESPONSE0;
	}
    }

    return error;
}


/* performs a 256 byte DMA read operation from the NAND flash */
section("L1_code")
static void SDBlockRead(void *pDestination, uint32_t size)
{
    /* initialize the DMA registers */
    *pDMA1_CONFIG = 0x0000;
    *pDMA1_START_ADDR = pDestination;
    *pDMA1_X_COUNT = (size / 4);	/* 512 byte transfers */
    *pDMA1_X_MODIFY = 4;	/* 4 byte modifier */

    /* clear the DMA Done IRQ */
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

    /* enable the DMA channel, this does not cause the transfer to start */
    /* the transfer actually starts when we write to the page program bit in NFC_PGCTL */
    *pDMA1_CONFIG = DMAEN | WNR | WDSIZE_32 | DI_EN | SYNC;
    ssync();
}

/* performs a 256 byte DMA write operation to the NAND flash */
section("L1_code")
static void SDBlockWrite(void *pSource, uint32_t size)
{
    /* initialize the DMA registers */
    *pDMA1_CONFIG = 0x0000;
    *pDMA1_START_ADDR = pSource;
    *pDMA1_X_COUNT = size / 4;	/* 512 byte transfers */
    *pDMA1_X_MODIFY = 4;	/* 4 byte modifier */

    /* clear the DMA Done IRQ */
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

    /* enable the DMA channel, this does not cause the transfer to start */
    /* the transfer actually starts when we write to the page program bit in NFC_PGCTL */
    *pDMA1_CONFIG = DMAEN | WDSIZE_32 | DI_EN | SYNC;
    ssync();
}

section("L1_code")
static uint32_t rsi_get_sd_ssr_register(void)
{
    uint32_t error = 0;

    *pRSI_DATA_LGTH = 64;
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT;	/* таймаут в тиках тактовой частоты = полсекунды */

    SDBlockRead((void *) &dma_buffer[0], 64);

    error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, adi_rsi_card_registers.sd.rca << 16);

    if ((*pRSI_RESPONSE0 & SD_CSR_CURRENT_STATE) == SD_CSR_CURRENT_STATE_STANDBY) {
	error = rsi_send_command(SD_MMC_CMD_SELECT_DESELECT_CARD, adi_rsi_card_registers.sd.rca << 16);
    }
    if (!error) {
	error = rsi_send_command(SD_MMC_CMD_APP_CMD, adi_rsi_card_registers.sd.rca << 16);

	if (!error) {
	    *pRSI_DATA_CONTROL = 0x6B;
	    error = rsi_send_command(SD_MMC_CMD_SEND_STATUS, adi_rsi_card_registers.sd.rca << 16);

	    if (!error) {
		/* *pRSI_DATA_CONTROL = 0x6B; */
		/* *pRSI_DATA_CONTROL = 0x3B; */

		while (!(*pRSI_STATUS & DAT_BLK_END));
		*pRSI_STATUSCL = DAT_BLK_END_STAT | DAT_END_STAT;
		*pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

		adi_rsi_card_registers.sd.ssr.dat_bus_width = (SD_SSR_DAT_BUS_WIDTH) ((dma_buffer[0] & 0xC0) >> 6);
		adi_rsi_card_registers.sd.ssr.secured_mode = (SD_SSR_SECURED_MODE) ((dma_buffer[0] & 0x20) >> 5);
		adi_rsi_card_registers.sd.ssr.sd_card_type = (SD_SSR_SD_CARD_TYPE) ((dma_buffer[2] << 8) | (dma_buffer[3]));
		adi_rsi_card_registers.sd.ssr.size_of_protected_area = (dma_buffer[4] << 24) | (dma_buffer[5] << 16) | (dma_buffer[6] << 8) | (dma_buffer[7]);
		adi_rsi_card_registers.sd.ssr.speed_class = (SD_SSR_SPEED_CLASS) dma_buffer[8];
		adi_rsi_card_registers.sd.ssr.performance_move = (SD_SSR_SPEED_CLASS) dma_buffer[9];
		adi_rsi_card_registers.sd.ssr.au_size = (SD_SSR_AU_SIZE) ((dma_buffer[10] & 0xF0) >> 4);
		adi_rsi_card_registers.sd.ssr.erase_size = (dma_buffer[12] << 8) | dma_buffer[13];
		adi_rsi_card_registers.sd.ssr.erase_timeout = (dma_buffer[14] & 0xFC) >> 2;
		adi_rsi_card_registers.sd.ssr.erase_offset = dma_buffer[14] & 0x3;
	    }
	}
    }
    return error;
}

/**
 * Инициализация интерфейса SD карты. Порты, прерывания, DMA и проч.
 * Защиту от записи и наличие карты смотрим НЕ по 3-му пину!
 */
section("L1_code")
int rsi_setup(void)
{
    int t0, t1;
    SD_MMC_CARD_TYPE type = UNUSABLE_CARD;

    /* Конфигурить порты для RSI: Разрешаем функцию на PG6...PG9 - DATA[3:0], PG10 - RSI_CMD и PG11 - RSI_CLK */
    *pPORTG_FER |= (PG6 | PG7 | PG8 | PG9 | PG10 | PG11);
    *pPORTG_MUX |= (1 << 8) | (1 << 10);	/* MUX на 2-й функции-биты [9:8] - 01, [11:10] - 01   */
    *pPORTG_FER &= ~(PG0 | PG1);	/* CD и WP - отключаем функции */
    ssync();


    *pPORTGIO_DIR &= ~(PG0 | PG1);	/* Делаем их на вход  */
    *pPORTGIO_INEN |= (PG0 | PG1);
    *pPORTGIO_POLAR |= (PG0 | PG1);	/* пин будет читаться как 1 при перепаде в 0 */
    *pPORTGIO_EDGE &= ~(PG0 | PG1);
    ssync();

    /* Получить клоки из  настроек PLL */
    rsi_get_pll_clocks();

    /* Если карты WP или карты нету. Ждем ~1 секунду */
    for(t0 = 0; t0 < 100; t0++) {
    
    	if(*pPORTGIO & PG1)
	   break;    	
	delay_ms(5);
    }
   
    if(t0 > 9)
       return type;	/* Таймаут - 1 секунда */

    /*Если карта отпределилась */
    *pRSI_CONFIG = RSI_CLK_EN | PU_DAT | PU_DAT3;	/* Разрешим клок и подтянем ноги */
    *pRSI_PWR_CONTROL = PWR_ON;
    ssync();

    *pRSI_CLK_CONTROL = (CLK_EN | PWR_SV_EN);	/* enable the RSI clocks + power save */
    rsi_set_optimum_identification_speed();	/* На время инициализации частота д.б. < 400 кГц   */
    ssync();

    delay_ms(50);		/* Поставил задержку и sync(), иногда! бывает ошибка стартового бита! */


    /* Определить тип карты */
    t0 = 0;
    do {
	type = rsi_identification();
	delay_ms(10);
	LED_toggle(LED_YELLOW);
    } while ((type == UNUSABLE_CARD || type == CARD_SLOT_NOT_INITIALIZED) && t0++ < 100);

    LED_off(LED_YELLOW);

    if (type == UNUSABLE_CARD || type == CARD_SLOT_NOT_INITIALIZED)
	return -1;

    return 0;
}

/**
 * Выключить питание с RSI
 */
section("L1_code")
void rsi_power_off(void)
{
    *pRSI_CONFIG = 0;	/* Разрешим клок и подтянем ноги */
    *pRSI_DATA_CONTROL = 0;	/* Разрешим клок и подтянем ноги */
    *pRSI_PWR_CONTROL = 0;	/* Выключим питание */
     ssync();
}

/**
 * Какая частота стоит в PLL?
 * VCO не должно превышать 300 МГц для нашего CPU (д.б. даже меньше!!!)
 */
section("L1_code")
static void rsi_get_pll_clocks(void)
{
    /* VCO = QUARTZ_CLK_FREQ * MSEL */
    unsigned long vco = 0;
    unsigned short pll_ctl = *pPLL_CTL;
    unsigned short cclk_div = (*pPLL_DIV & 0x0030) >> 4;
    unsigned short sclk_div = (*pPLL_DIV & 0x000F);

    if (pll_ctl & 0x0100) {
	adi_rsi_card_registers.cclk = QUARTZ_CLK_FREQ;
	adi_rsi_card_registers.sclk = 0x0;
    } else {
	if (!(pll_ctl & 0x7E00)) {
	    vco = QUARTZ_CLK_FREQ * 64;
	} else {
	    vco = QUARTZ_CLK_FREQ * ((pll_ctl & 0x7E00) >> 9);
	}

	/* B PLL стоит делитель?  */
	if (pll_ctl & 0x0001) {
	    vco >>= 1;
	}

	/* Получили клоки */
	adi_rsi_card_registers.cclk = vco >> cclk_div;
	adi_rsi_card_registers.sclk = vco / sclk_div;
    }
}

/**
 * Установка частоты RSI на время инициализации 
 */
section("L1_code")
static void rsi_set_optimum_identification_speed(void)
{
    uint32_t clkdiv = 0;
    uint32_t card_speed = 400000;	/* Card identification must be run at 400000 Hz or less */
    uint32_t sclk = adi_rsi_card_registers.sclk;
    uint32_t rsi_freq;

    /* Setting RSI clock frequency by enabling CLKDIV_BYPASS */
    if (sclk <= card_speed) {
	*pRSI_CLK_CONTROL |= CLKDIV_BYPASS;
    } else {
	do {
	    rsi_freq = (sclk / (2 * (clkdiv + 1)));	/* Подбираем частоту, чтобы она была ниже 400 кГц  */
	    clkdiv++;
	} while (rsi_freq > card_speed);

	clkdiv--;
	*pRSI_CLK_CONTROL = (*pRSI_CLK_CONTROL & 0xFF00) | clkdiv;
	adi_rsi_card_registers.rsiclk = rsi_freq;
    }
}

/**
 *  Число секторов на sd карте 
 */
section("L1_code")
uint32_t rsi_get_sec_count(void)
{
    return adi_rsi_card_registers.sd.csd.c_size;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//              Внутри L1
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Читает 1 или несколько блоков из SD по DMA  */
section("L1_code")
uint32_t rsi_read_blocks_from_sd_card(uint32_t block_num, void *pReadBuffer, uint32_t num_blocks)
{
    uint32_t error = 0;
    if (num_blocks == 1)
	error = rsi_read_single_block_dma(card_address(block_num), pReadBuffer);
    else
	error = rsi_read_multi_blocks_dma(card_address(block_num), pReadBuffer, num_blocks);
    return error;
}

/* Пишет 1 или много блоков на SD карту */
section("L1_code")
uint32_t rsi_write_blocks_to_sd_card(uint32_t block_num, void *pWriteBuffer, uint32_t num_blocks)
{
    uint32_t error = 0;
    if (num_blocks == 1)
	error = rsi_write_single_block_dma(card_address(block_num), pWriteBuffer);
    else
	error = rsi_write_multi_blocks_dma(card_address(block_num), pWriteBuffer, num_blocks);
    return error;
}



/**
 *  Послать команду контролеру внутри SD карты
 *  Ошибки, кот. можем получить: таймаут + CRC err + startbit err
 */
section("L1_code")
static uint32_t rsi_send_command(uint16_t command, uint32_t argument)
{
    uint32_t result = 0;
    uint32_t error = CMD_CRC_FAIL | CMD_TIMEOUT | START_BIT_ERR;
    uint32_t success = CMD_RESP_END;

    if (!(command & CMD_RSP)) {
	error = 0;
	success = CMD_SENT;
    }

    *pRSI_ARGUMENT = argument;	/* write the Argument register that is to be sent with the command */
    *pRSI_COMMAND = command | CMD_E;	/* write the command register and start the command transfer */

    /* if we are required to wait for an error or success notification */
    while (!(*pRSI_STATUS & (error | success))) {	/* wait for at least one of the error or success flags to be set */
	ssync();
    }

    /* Если есть ошибка-возвращаем ее, если нет то 0 */
    if (*pRSI_STATUS & error) {
	result = *pRSI_STATUS;
    }
    *pRSI_STATUSCL = error | success;	/* Очищаем статус  */
    ssync();

    return (result);		/* Если ошибки нет - возвращаем 0 (OK) */
}



/**
 *  Чтение только одного блока
 */
section("L1_code")
static uint32_t rsi_read_single_block_dma(uint32_t card_address, void *pDestination)
{
    uint32_t error = 0;		/* assume no error */
    uint32_t status = 0;

    *pRSI_DATA_LGTH = 512;	/* length of data transfer */
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT;	/* таймаут в тиках тактовой частоты = полсекунды */

    SDBlockRead(pDestination, 512);	/* configures the DMA for a 512 byte read */
    *pRSI_DATA_CONTROL = 0x9B;	/* start the data path state machine so it's ready to accept data */
    error = rsi_send_command(SD_MMC_CMD_READ_SINGLE_BLOCK, card_address);	/* issue the block read command */


    /* Ожидаем передачи блока с выводом ошибки или таймаута */
    if (!error) {
	do {
	    status = *pRSI_STATUS;
	} while ((status & (DAT_BLK_END | DAT_END | DAT_CRC_FAIL | DAT_TIMEOUT)) == 0);

	if (status & DAT_END) {
	    *pRSI_STATUSCL = DAT_BLK_END_STAT | DAT_END_STAT;
	    error = 0;
	} else {
	    error = status;
	    *pRSI_STATUSCL = DAT_CRC_FAIL | DAT_TIMEOUT;
	}
    }

    while ((*pDMA1_IRQ_STATUS & DMA_DONE) != DMA_DONE);	/* Передача завершилась? */
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;	/* Очистим регистр DMA IRQ */
    return error;
}

/**
 *  Чтение нескольких блоков. Отличается командой от чтения одного блока.
 */
section("L1_code")
static uint32_t rsi_read_multi_blocks_dma(uint32_t card_address, void *pDestination, uint32_t num_blocks)
{
    uint32_t error = 0;

    *pRSI_DATA_LGTH = num_blocks * 512;
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT;	/* таймаут в тиках тактовой частоты = полсекунды */

    SDBlockRead(pDestination, num_blocks * 512);
    *pRSI_DATA_CONTROL = 0x9B;

    error = rsi_send_command(SD_MMC_CMD_READ_MULTIPLE_BLOCK, card_address);
    do {
	while (!(*pRSI_STATUS & DAT_BLK_END));
    } while (!(*pRSI_STATUS & DAT_END));

    *pRSI_STATUSCL = DAT_BLK_END_STAT | DAT_END_STAT;
    error = rsi_send_command(SD_MMC_CMD_STOP_TRANSMISSION, 0);

    /* ensure the dma has completed */
    while ((*pDMA1_IRQ_STATUS & DMA_DONE) != DMA_DONE);
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;
    return error;
}

/**
 *  Запись только одного блока
 */
section("L1_code")
static uint32_t rsi_write_single_block_dma(uint32_t card_address, void *pSource)
{
    uint32_t error = 0;

    *pRSI_DATA_LGTH = 512;
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT;	/* таймаут в тиках тактовой частоты = полсекунды */

    SDBlockWrite(pSource, 512);
    error = rsi_send_command(SD_MMC_CMD_WRITE_SINGLE_BLOCK, card_address);
    *pRSI_DATA_CONTROL = 0x99;

    while (!(*pRSI_STATUS & DAT_BLK_END));
    *pRSI_STATUSCL = DAT_BLK_END_STAT | DAT_END_STAT;

    /* ensure DMA has completed */
    while ((*pDMA1_IRQ_STATUS & DMA_DONE) != DMA_DONE);
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

    /* wait until the device becomes ready */
    rsi_get_wait_until_ready();
    return error;
}

/**
 *  Запись нескольких блоков. Отличается командой от записи одного блока.
 */
section("L1_code")
static uint32_t rsi_write_multi_blocks_dma(uint32_t card_address, void *pSource, uint32_t num_blocks)
{
    u32 error;
    u32 status;

    *pRSI_DATA_LGTH = num_blocks * 512;
    *pRSI_DATA_TIMER = CARD_TIMER_TIMEOUT * 2;	/* таймаут в тиках тактовой частоты = полсекунды */

    SDBlockWrite(pSource, num_blocks * 512);
    error = rsi_send_command(SD_MMC_CMD_WRITE_MULTIPLE_BLOCK, card_address);
    *pRSI_DATA_CONTROL = 0x99;

    /* Ожидаем передачи блока с выводом ошибки или таймаута */
    while (!((status = *pRSI_STATUS) & (DAT_END | DAT_TIMEOUT)));
    *pRSI_STATUSCL = DAT_END_STAT | DAT_BLK_END_STAT | DAT_TIMEOUT_STAT;

    error = rsi_send_command(SD_MMC_CMD_STOP_TRANSMISSION, 0);

    while ((*pDMA1_IRQ_STATUS & DMA_DONE) != DMA_DONE);	/* ensure DMA has completed */
    *pDMA1_IRQ_STATUS = *pDMA1_IRQ_STATUS & DMA_DONE;

    rsi_get_wait_until_ready();	/* wait for the device to become ready */
    return error;
}


#pragma section("FLASH_code")
void rsi_get_card_timeout(SD_CARD_ERROR_STRUCT * ts)
{
    memcpy(ts, &sd_card_error, sizeof(SD_CARD_ERROR_STRUCT));
}
