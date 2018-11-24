/*******************************************************************************
 * В этом файле описываются инициализация PLL для нормального и спящего режима
 *******************************************************************************/
#include <ccblkfn.h>
#include <bfrom.h>
#include "config.h"
#include "main.h"
#include "pll.h"


/*  Инициализация PLL, на вариант в файле заголовка */
#pragma section("FLASH_code")
void PLL_init(void)
{
	u32 SIC_IWR1_reg;	/* backup SIC_IWR1 register */

	/* use Blackfin ROM SysControl() to change the PLL */
	ADI_SYSCTRL_VALUES sysctrl = {
		PLLVRCTL_VALUE,   	/* Просыпаться по прерываниям на ногах PF8 и PF9 */
		PLLCTL_VALUE,		/* MSEL[5:0] = 10 - получили VCO = 192МГц из 19.2 */
		PLLDIV_VALUE,		/* CSEL[1:0] = 0  - получили CCLK = VSO/ 1 = 192МГц, SSEL[3:0] = 6  - получили SCLK = VSO/4 = 48МГц */
		PLLLOCKCNT_VALUE,	/* Через 512 тактов заснуть */
		PLLSTAT_VALUE		/* NB: Только чтение!!!  */
	};

	SIC_IWR1_reg = *pSIC_IWR1;	/* save SIC_IWR1 due to anomaly 05-00-0432 */
	*pSIC_IWR1 = 0;		/* disable wakeups from SIC_IWR1 */

	/* use the ROM function */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_PLLCTL | SYSCTRL_PLLDIV, &sysctrl, NULL);
	*pSIC_IWR1 = SIC_IWR1_reg;	/* restore SIC_IWR1 due to anomaly 05-00-0432 */
}



/* Перевести процессор в спящий режим  */
#pragma section("L1_code")
void PLL_sleep(DEV_STATE_ENUM state)
{

     if(state != DEV_COMMAND_MODE_STATE && state != DEV_POWER_ON_STATE && state != DEV_CHOOSE_MODE_STATE) {
	ADI_SYSCTRL_VALUES sleep;

	/* прочитать */
	bfrom_SysControl(SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL | SYSCTRL_READ, &sleep, NULL);
        sleep.uwPllCtl |= STOPCK;       /* Изменим на Sleep режим */

         /* События, которые будут нас будить */
	*pSIC_IWR0 = IRQ_UART0_ERR | IRQ_UART1_ERR | IRQ_PFA_PORTF;

#if QUARTZ_CLK_FREQ==(19200000)
	*pSIC_IWR1 = IRQ_TIMER0 | IRQ_TIMER1 | IRQ_TIMER2 | IRQ_TIMER3 | IRQ_TIMER4 | IRQ_TIMER5 | IRQ_PFA_PORTG;
#else
        /* Нет 3-го таймера */
	*pSIC_IWR1 = IRQ_TIMER0 | IRQ_TIMER1 | IRQ_TIMER2 | IRQ_TIMER4 | IRQ_TIMER5 | IRQ_PFA_PORTG;
#endif

	/* и записать - все равно проснеца */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &sleep, NULL);
    }
}

/* Перевести процессор в рабочий режим */
#pragma section("L1_code")
void PLL_fullon(void)
{
	ADI_SYSCTRL_VALUES fullon;

	/* прочитать */
	bfrom_SysControl (SYSCTRL_READ | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &fullon, NULL);
        fullon.uwPllCtl &= ~STOPCK; 

	/* и записать, нет нужды избегать аномальности - все равно проснеца */
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_EXTVOLTAGE | SYSCTRL_PLLCTL, &fullon, NULL);
}


/* Гибернация */
#pragma section("L1_code")
void PLL_hibernate(DEV_STATE_ENUM v)
{
	ADI_SYSCTRL_VALUES hibernate;

	hibernate.uwVrCtl = 
	    WAKE_EN0 |		/* PH0 Wake-Up Enable */
	    WAKE_EN1 |		/* PF8 Wake-Up Enable */
	    WAKE_EN2 |		/* PF9 Wake-Up Enable */
	    CANWE |		/* CAN Rx Wake-Up Enable */
	    HIBERNATE;
	bfrom_SysControl(SYSCTRL_WRITE | SYSCTRL_VRCTL | SYSCTRL_EXTVOLTAGE| SYSCTRL_HIBERNATE , &hibernate, NULL);
}


/* сбросить процессор */
#pragma section("L1_code")
void PLL_reset(void)
{
	bfrom_SysControl(SYSCTRL_SYSRESET, NULL, NULL); /* either */
	bfrom_SysControl(SYSCTRL_SOFTRESET, NULL, NULL); /* or */
}



