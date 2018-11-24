#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "main.h"
#include "globdefs.h"
#include "version.h"
#include "comport.h"
#include "com_cmd.h"
#include "modem.h"
#include "gps.h"
#include "ads1282.h"
#include "xpander.h"
#include "power.h"
#include "ports.h"
#include "timer1.h"
#include "timer2.h"
#include "timer3.h"
#include "timer4.h"
#include "tests.h"
#include "utils.h"
#include "eeprom.h"
#include "bq32k.h"
#include "dac.h"
#include "irq.h"
#include "pll.h"
#include "led.h"
#include "log.h"


/***********************************************************************************************
 * 	Дефиниции
 ***********************************************************************************************/
#define 	POWER_DROP_COUNT		10	/* За xxx минут подсчитаем. КАК НЕ ДЕЛАТЬ ДЕФИНИЦИЕЙ!!!  */


/* Биты запросов */
#define		LOCK_NONE_REQ			0x00
#define		LOCK_POWER_OFF_REQ		0x01

/* Бит Lock */
#define		MODE_SLEEP_LOCK			0x0100
#define		MODE_REG_LOCK			0x0200
#define		MODE_FIN_LOCK			0x0400
#define		MODE_BURN_ON_LOCK		0x0800
#define		MODE_BURN_OFF_LOCK		0x1000

#define		MODE_HALT_LOCK			0x2000
#define		MODE_ERR_LOCK			0x4000
#define		MODE_CMD_LOCK			0x8000

#define		MODE_EMERGENCY_LOCK		0x20000


/**
 * 	Статические функции - видны тока здесь
 */
static void timer1_callback_func(u32);
static int begin_reg(void);
static void shift_gns_time_after_burn(void);
static void finish_reg(void);
static s16 filt_reg_power(s16);
static void get_atmega_data(DEV_STATE_ENUM *);	/* Передается указатель - можно сменить состояние */
static void count_work_time(DEV_STATE_ENUM);	/* Время работы прибора - должно накапливаться и сбрасываться на flash или eeprom */
#if QUARTZ_CLK_FREQ==(19200000)
static void quartz4_tune(DEV_STATE_ENUM);
#endif
static void blink_all_leds(DEV_STATE_ENUM);
static void exec_uart_cmd(DEV_STATE_ENUM *);
static void get_dev_params(void *, void *);
static void check_magnet_request(DEV_STATE_ENUM *);
static void wait_burn_time(DEV_STATE_ENUM state);

/**
 * 	Статические функции, кот. описывают состояние КА
 */
static int OnPoweronState(void);
static int OnChooseModeState(void);	/* + тестирование модулей  */
static int OnInitState(void);
static int OnTuneQ19State(void);
static int OnTuneWithoutGPS(void);
static int OnSleepAndDiveState(void);
static int OnWakeupState(void);
static int OnRegState(void);
static int OnHaltState(void);
static int OnPowerOffState(void);
static int OnErrorState(void);
static int OnGet3DFixState(void);
static int OnFinishRegState(void);
static int OnCommandModeState(void);
static int OnEmergencyModeState(void);

/**
 * 	Статические структуры и переменные
 */
static DEV_STATE_ENUM dev_state = DEV_POWER_ON_STATE;	/* Состояние автомата изначально */
static DEV_STATUS_STRUCT dev_status;	/* Статус ошибок устроства + параметры среды и напряжения */
static DEV_ADDR_STRUCT dev_addr;	/* Адрес устройства  */
static DEV_DAC_STRUCT dac_data;	/* Для подачи на ЦАПы  */
static DEV_UART_CMD uart_cmd;	/* Внешняя команда пришла с UART */
static DEV_WORK_TIME_STRUCT dwt;	/* Время работы устроства */
static GNS110_PARAM_STRUCT gns110_param;	/* Параметры запуска прибора заполняются в LOG.C и сохраняются здесь */


/* Для фильтра скользящего среднего 20 байт + 4 байта */
static struct {
    s16 volts[POWER_DROP_COUNT];
    s16 count;
    s16 num;
} xVoltArray;


/* Включение модема  */
static struct {
    long lTime0;
    bool bEnter;
    bool bInit;
    u16 rsvd;
} xShiftModemTime;

/* Включение GPS */
static struct {
    long lTime0;
    bool bEnter;
    u8 rsvd[3];
} xNmeaSet;

/**
 * Время RTC - снимать не чаще чем раз в секунду  
 */
static struct {
    s32 iTime0;
    s32 iTime1;
    s32 iSec;
    s32 iLabel;			/* Здесь подсчитаем насколько убегает генератор */
    s32 iWasDrift;

    union {
	struct {
	    c8 cStatus;		/* Статус часов RTC - какое у нас время, точное или нет */
	    c8 cPackCnt;	/* Счетчик достоверных приемов NMEA  */
	    c8 cCheck;
	    c8 cLock;
	} xRtcCheck;
	u32 ctrl;
    } u_check;
} xRtcTicks;



/**
 * Пришли внешние запросы: на пережиг или выключение или переключение режимов 
 * Начало и окончание записи  
 * Ожидание подключения по WUSB или плохой сброс!
 * или Выключение питания при отсутсвии связи ~10 минут
 */
static struct {
    int time;			/* Время  */
    int reset;			/* Причина сброса или выключения */
    u32 mode;			/* Режимы работы, чтобы не плодить static  */

    struct {
	int ExtBurnDur;		/* Продолжительность пережига */
	u8 cBurnCnt;		/* Счетчик пережигов */
	bool bBurnLock;
	u16 rsvd;
    } burn_time;

    /* Команды на переключение магнитом  */
    union {
	struct {
	    bool bPowerOff;	/* Флаг на выключение магнитом */
	    bool bExternBurn;	/* Флаг внешнего пережига  */
	    u8 bInnerBurnOn:4;	/* Флаг внутреннего пережига ВКЛ */
	    u8 bInnerBurnOff:4;	/* Флаг внутреннего пережига  ВЫКЛ */
	    bool bInitCmd;	/* Переключиться в командный режим из ожидания */
	} xCtrlBurn;
	u32 ctrl;
    } u_burn;

    /* Флаги во время регистрации */
    union {
	struct {
	    u8 cLock;
	    bool bStart;
	    bool bFinish;
	    bool bBeginReg;
	} xCtrlReg;
	u32 ctrl;
    } u_reg;

    /* Ожидание режима */
    union {
	struct {
	    u8 bResult;		/* Результат монтирования  */
	    u8 bState;		/* Состояние КА */
	    bool bWusb;
	    bool bSync;
	} xCtrlMode;
	u32 ctrl;
    } u_mode;
} xShooseMode;


/* Ф-ция main()  */
#pragma section("L1_code")
int main(void)
{
    int res;

    /* Иниц. PLL от кварца 19.2 МГц с самого начала! */
    PLL_init();

    /* Настройка FLASH на нашу частоту переферии */
    FLASH_init();

    dev_state = DEV_POWER_ON_STATE;

    /* Основной цыкл - Здесь все круyтица */
    while (1) {

	switch (dev_state) {

	    /* Включение питания и инициализация всех портов */
	case DEV_POWER_ON_STATE:
	    res = OnPoweronState();
	    if (res == 0) {
		dev_state = DEV_CHOOSE_MODE_STATE;
	    } else if (res == 1) {
		dev_state = DEV_INIT_STATE;
	    } else {
		dev_state = DEV_ERROR_STATE;
	    }
	    break;

	    /* Тестирование и ожидание команды от COM порта */
	case DEV_CHOOSE_MODE_STATE:
	    res = OnChooseModeState();
	    if (res == 1) {
		dev_state = DEV_COMMAND_MODE_STATE;
	    } else if (res == 2) {
		dev_state = DEV_INIT_STATE;
	    } else if (res == -1) {
		dev_state = DEV_ERROR_STATE;
	    }
	    break;

	    /* Инициализация всех устройств + проверка, лампочки - все часто */
	case DEV_INIT_STATE:
	    res = OnInitState();
	    if (res == 0) {	/* Все OK? */
		dev_state = DEV_GET_3DFIX_STATE;	/* Если все нормально - изменим состояние устройства на прием NMEA */
	    } else if (res == 1) {
		dev_state = DEV_TUNE_WITHOUT_GPS_STATE;
	    } else if (res < 0) {
		dev_state = DEV_ERROR_STATE;	/* Ошибка */
	    }
	    break;


	    /* Запускаем таймер без подстройки */
	case DEV_TUNE_WITHOUT_GPS_STATE:
	    res = OnTuneWithoutGPS();
	    if (res == 0) {	// Все OK
		dev_state = DEV_SLEEP_AND_DIVE_STATE;
	    } else if (res < 0) {
		dev_state = DEV_ERROR_STATE;	/* Не смогли поймать Запустить таймер 1 */
	    }
	    break;

	    /* Ожидание GPS 1 час. Лампочки - синяя редко. Если получили - то изменим состояние КА */
	case DEV_GET_3DFIX_STATE:
	    res = OnGet3DFixState();
	    if (res > 0) {
		dev_state = DEV_TUNE_Q19_STATE;	/* Получили 3d Fix!!! */
	    } else if (res == -1) {
		dev_state = DEV_ERROR_STATE;	/* Не смогли поймать Запустить таймер 1 */
	    }
	    break;

	    /* Подстройка кварца 19.2 МГц и синхронизация двух таймеров.  Лампочки - зеленая редко */
	case DEV_TUNE_Q19_STATE:
	    res = OnTuneQ19State();
	    if (res == 0)	/* Все в порядке - настроили! */
		dev_state = DEV_SLEEP_AND_DIVE_STATE;	/* Изменяем состояние на SLEEP */
	    else if (res == 1)	/* нет спутников - вернуться на предыдущий щаг */
		dev_state = DEV_GET_3DFIX_STATE;
	    else
		dev_state = DEV_ERROR_STATE;	/* Ошибочка вышла! */
	    break;


	    /* Установим CallBack для таймера 1 и изменим режим PLL. Лампочки - зеленая редко
	     * при достижении времени начала регистрации минус ~2 минуты
	     * переходим в режим регистрации, переход состояние WAKEUP в функции callback PRTC (timer1) */
	case DEV_SLEEP_AND_DIVE_STATE:
	    res = OnSleepAndDiveState();
	    if (res > 0) {
		dev_state = DEV_WAKEUP_STATE;
	    } else if (res < 0) {
		dev_state = DEV_ERROR_STATE;
	    }
	    break;

	    /* Просыпаемся примерно за две минуты и делаем первичную настройку кварца 4 МГц */
	case DEV_WAKEUP_STATE:
	    res = OnWakeupState();
	    if (res == 0) {
		dev_state = DEV_REG_STATE;	/* Перехрдим на начало регистрации  */
	    } else {
		dev_state = DEV_ERROR_STATE;
	    }
	    break;


	    /* Начинаем подстраивать АЦП и подаем SYNC на них. Лампочки - зеленая редко */
	case DEV_REG_STATE:
	    res = OnRegState();
	    if (res != 0) {
		dev_state = DEV_EMERGENCY_WAIT_STATE;
	    }
	    break;

	    /* Окончание регистрации - в это состояние попадаем из таймера 1. Выключаем все оборудование и впадаем в спячку */
	case DEV_FINISH_REG_STATE:
	    res = OnFinishRegState();
	    if (res != 0) {
		dev_state = DEV_EMERGENCY_WAIT_STATE;
	    }
	    break;

	    /* Ожидаем GPS и получаем дрифт - сделать как в ожидании NMEA - 1 час */
	case DEV_WAIT_GPS_STATE:
	    res = OnGet3DFixState();
	    if (res > 0) {	/* Все нормально  */
		dev_state = DEV_HALT_STATE;	/* Выключаем  */
	    } else if (res == -1) {
		log_write_log_file("Error: Can't get 3d fix for % seconds\n", WAIT_3DFIX_TIME);
		dev_state = DEV_EMERGENCY_WAIT_STATE;	/* Ошибка  */
	    }
	    break;


	    /* Останов - ожидание поднятия на палубу */
	case DEV_HALT_STATE:
	    res = OnHaltState();
	    if (res != 0) {
		dev_state = DEV_EMERGENCY_WAIT_STATE;	/* Тогда на ошибку!  */
	    }
	    if (xShooseMode.u_burn.xCtrlBurn.bPowerOff) {
		dev_state = DEV_POWER_OFF_STATE;
	    }
	    break;

	    /* В этом состоянии просто выключаем питание  */
	case DEV_POWER_OFF_STATE:
	    OnPowerOffState();
	    break;

	    /* Управление от компьютера - все лампы моргают раз в 2 секунды */
	case DEV_COMMAND_MODE_STATE:
	    res = OnCommandModeState();
	    if (res != 0) {
		dev_state = DEV_ERROR_STATE;	/* Тогда на ошибку во всех прочих случаях */
	    }
	    break;

	    /* Аварийное ожидание  */
	case DEV_EMERGENCY_WAIT_STATE:
	    res = OnEmergencyModeState();
	    break;

	    /* Ошибка! Красная часто */
	case DEV_ERROR_STATE:
	    res = OnErrorState();
	    if (res != 0) {
		dev_state = DEV_POWER_OFF_STATE;
	    }
	    break;

	default:
	    break;
	}
	/* Всегда во всех состояниях */
#if QUARTZ_CLK_FREQ==(19200000)
	quartz4_tune(dev_state);	/* Подстройка кварца раз в секунду всегда */
#endif
	get_atmega_data(&dev_state);	/* Получить параметры АЦП с атмеги. Во всех состояниях следим за проволкой. Если модем разрешен! */
	check_magnet_request(&dev_state);	/* Следить за магнитом */
	wait_burn_time(dev_state);	/* Следить за пережигом */
	exec_uart_cmd(&dev_state);	/* Выполнить команду, если в режиме PC */
	count_work_time(dev_state);	/* Считать время работы */
	blink_all_leds(dev_state);	/* Моргать лампами в зависимости от режима */
	PLL_sleep(dev_state);	/* Не спать в командном режиме */
    }
    return 0;
}


/**
 * Включение питания и инициализация всех портов, определение версии программы или прошивки
 */
#pragma section("FLASH_code")
static int OnPoweronState(void)
{
    init_bf_ports();		/* Ставим порты BF */
    init_atmega_ports();	/* После таймера конфигурируем все порты на Атмеге */

    /* Затересть структуру */
    memset(&xShooseMode, 0, sizeof(xShooseMode));

    dev_status.st_main |= 0x02;	/*  нет модема или ошиька модема */
    dev_status.st_test0 |= 0x7F;	/* Ставим байт - потом его снимем если нет проблем */
    dev_status.st_test1 = 0xF0;	/* Ставим доп. байт - потом его снимем если нет проблем */
    dev_status.quartz = QUARTZ_CLK_FREQ;
    dev_status.sclk = SCLK_VALUE / TIMER_US_DIVIDER;
    dev_status.cclk = SCLK_VALUE * (PLLDIV_VALUE & 0x000F) / (1 << ((PLLDIV_VALUE >> 4) & 0x000F)) / TIMER_US_DIVIDER;


    if (eeprom_init()) {	/* Инициализация псевдо-eeprom и flash - Нужна И для eeprom И для сохранения буфера при ошибке SD */
	dev_status.st_test0 |= 0x20;	/* Ошибка инициализации EEPROM */
    } else {
	dev_status.st_test0 &= ~0x20;	/* Снимаем ошибку */
    }


    /* Модуль включения - выключения, включаем питание по алгоритму */
    xShooseMode.reset = POWER_on();
    get_dev_params(&dev_addr, &dwt);	/* Прочитаем адрес устройства и время работы */

    LED_on(LED_GREEN);		/* Зажгем лампу */

    POWER_init();


    if (xShooseMode.reset == CAUSE_WDT_RESET) {
	return 1;		/* Причина сброса по WDT - на работу без подстройки */
    } else {
	return 0;		/* На тестирование */
    }
}


/**
 * Ожидание команды от порта для смены режима работы
 */
#pragma section("FLASH_code")
static int OnChooseModeState(void)
{
    long t0;
    int i, j;

    /* начальное состояние-монтируем карту  */
    if (xShooseMode.u_mode.xCtrlMode.bState == 0) {
	xShooseMode.u_mode.xCtrlMode.bState = 1;

	xShooseMode.u_burn.xCtrlBurn.bExternBurn = 0;
	xShooseMode.u_burn.xCtrlBurn.bPowerOff = 0;
	xShooseMode.u_reg.xCtrlReg.cLock = LOCK_NONE_REQ;

	xShooseMode.burn_time.cBurnCnt = 0;
	xShooseMode.burn_time.ExtBurnDur = 0;
	xShooseMode.burn_time.bBurnLock = false;

	LED_set_state(LED_QUICK_STATE, LED_QUICK_STATE, LED_QUICK_STATE, LED_OFF_STATE);	/* Огоньки, все быстро моргают кроме синей */

	memset(&xRtcTicks, 0, sizeof(xRtcTicks));
	xRtcTicks.u_check.xRtcCheck.cStatus = CLOCK_NO_TIME;	/* Пока у нас нету часов  */


	xShooseMode.u_mode.xCtrlMode.bResult = 1;	/* Ошибка монтирования - поставим заранее чтобы не ставить при ошибке */
	dev_status.st_test0 |= 0x40;	/* Ошибка карты */
	dev_status.st_main |= 0x08;	/* Нет файла регистрации */


	i = log_mount_fs();	/* Подключим SD карту к BF и монтируем ФС  */
	if (i == RES_NO_ERROR) {
	    dev_status.st_test0 &= ~0x40;	/* Если монтировали - снимаем ошибку карты и выходим */

	    i = log_check_lock_file();	/* Проблем нету, смотрим, есть ли у нас лок файл  */
	    j = log_check_reg_file();	/* Нет файла регистрации */


	    /* Есть лок и есть param - переходим в автономный режим */
	    if (i == RES_NO_ERROR && j == RES_NO_ERROR) {
		dev_status.st_main &= ~0x08;	/* Снимает бит "нет файла регистрации" */
		xShooseMode.u_mode.xCtrlMode.bResult = 0;
		LED_set_state(LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_SLOW_STATE);	/* Синяя лампа чтобы отличить */
		return 2;	// На выход

		// Выходим с ошибкой - у нас или нет файла параметров
	    } else if (j < 0) {

		// нет лок файла но есть param
	    } else {
		dev_status.st_main &= ~0x08;	/* Снимает бит "нет файла регистрации" */
	    }
	}


	/* Подключить WUSB если ошибка или нет файла */
	if (xShooseMode.u_mode.xCtrlMode.bResult) {
	    xShooseMode.u_mode.xCtrlMode.bWusb = wusb_on();	/* если 1 - подключено по WUSB */

	    if (xShooseMode.u_mode.xCtrlMode.bWusb) {
		LED_set_state(LED_QUICK_STATE, LED_QUICK_STATE, LED_QUICK_STATE, LED_QUICK_STATE);	// Состояние ламп - все мигают
	    }
	}
	xShooseMode.time = get_sec_ticks();	/* Получаем метку времени  */
    }

    t0 = get_sec_ticks();


    /* Открываем UART для отладки если нет файла или ошибка карты */
    if (xShooseMode.u_mode.xCtrlMode.bState == 1 && xShooseMode.u_mode.xCtrlMode.bResult == 1) {
	xShooseMode.u_mode.xCtrlMode.bState = 2;	// меняем состояние

	if (comport_init() < 0) {
	    return -1;
	}
	select_sdcard_to_cr();	/* Подключим SD к CR */
	xShooseMode.time = get_sec_ticks();	/* Получаем время  */
    }


    /* В этом месте ждем, пока не будет получена команда перехода в режим обмена с PC */
    if (xShooseMode.u_mode.xCtrlMode.bState == 2 && xShooseMode.u_mode.xCtrlMode.bResult == 1 && t0 - xShooseMode.time < WAIT_PC_TIME) {

	/* В режим работы от PC не выключая WUSB */
	if (comport_get_command() == UART_CMD_COMMAND_PC)
	    return 1;

	/* Перейти в режим работы если провели магнитом */
	if (xShooseMode.u_burn.xCtrlBurn.bInitCmd)
	    xShooseMode.u_mode.xCtrlMode.bResult = 0;
    }

    /* нормальный режим работы если нет файла или вышло время ожидания или переключили магнитом */
    if (t0 - xShooseMode.time > WAIT_PC_TIME || xShooseMode.u_burn.xCtrlBurn.bInitCmd) {
	wusb_off();		/* Выключить WUSB */
	select_sdcard_to_bf();	/* Подключили к BF  */
	comport_close();	/* Выключить отладочный порт */
	unselect_debug_uart();	/* Отключили UART */
	return 2;
    }

    return 0;
}


/**
 * Инициализация устройств, тестирование модулей, уже ПОСЛЕ ТОГО КАК ВЫЖДАЛИ 2 МИНУТЫ ОЖДИДАНИЯ PC 
 */
#pragma section("FLASH_code")
static int OnInitState(void)
{
    int res, t0, tgns, tmod;
    u16 id[4];
    char buf[32];
    TIME_DATE d;
    short i;
    int running_on = *pDSPID & 0xff;	/* check the part */
    int built_for = __SILICON_REVISION__;	/* check what we built against */
    char sym;


    LED_set_state(LED_TEST_STATE, LED_TEST_STATE, LED_TEST_STATE, LED_TEST_STATE);

    /* Обнулим */
    memset(&xShiftModemTime, 0, sizeof(xShiftModemTime));


    /* Забьем отсчеты фильтра 2-ным напяжением минимума */
    for (i = 0; i < POWER_DROP_COUNT; i++) {
	xVoltArray.volts[i] = POWER_DROP_MIN * 2;
    }
    xVoltArray.count = 0;
    xVoltArray.num = 0;


    /* Задержка */
    t0 = get_msec_ticks();
    while (get_msec_ticks() - t0 < 250) {
	LED_test();
    }


    /* Если у нас карта не монтирована - Подключим SD карту к BF и монтируем ФС  */
    if (!log_check_mounted() || dev_status.st_test0 & 0x40) {
	if (log_mount_fs() != RES_NO_ERROR) {
	    return -1;
	}
    }

    select_analog_power();	/* Включить аналоговую часть */
    test_all(&dev_status);	/*  Тестируем ВСЕ модули здесь!  */


    /* Открыть файл регистрации и получить все времена */
    res = log_read_reg_file(&gns110_param);
    if (res != RES_NO_ERROR) {
	log_write_error_file("Error:   Can't parse reg file. parameters error (%d)\n", res);
	log_write_error_file("Info:    Check \"regparam.cfg\" and try again\n\n");
	if (res == RES_MAX_RUN_ERR || res == RES_DIR_ALREADY_EXIST) {
	    log_write_error_file("Info:   May be start numbers (99) were been exhausted?\n");
	    log_write_error_file("Info:   For normal work try to delete the folder with name ******99\n");
	}
	return res;		/* Can't to find ini-file regparam.cfg */
    }

    if (xShooseMode.reset == CAUSE_WDT_RESET) {
	log_write_log_file("=======DEV_INIT_STATE with WDT reset=======\n");
    } else if (xShooseMode.u_burn.xCtrlBurn.bInitCmd) {
	log_write_log_file("=======DEV_INIT_STATE by MAGNET=======\n");
    } else {
	log_write_log_file("=======DEV_INIT_STATE=======\n");
    }

    /* В самый верх лога! */
    log_write_log_file("WARN: --- This program was compilied for %d MHz VCXO ---\n", QUARTZ_CLK_FREQ / TIMER_US_DIVIDER);

    print_reset_cause(xShooseMode.reset);


#if defined ENABLE_TEST_DATA_PACK
    log_write_log_file("INFO: Test data on 3 channel\n");
#endif


/* Обычно модули R2B > 6 */
#if defined GNS110_R2A_BOARD
    sym = 'a';
#elif defined GNS110_R2B_BOARD
    sym = 'b';
#elif defined GNS110_R2C_BOARD
    sym = 'c';
#endif

    if (dev_addr.time && (sec_to_td((long) dev_addr.time, &d) == 0)) {
	log_write_log_file("INFO: Version %d.%03d.%02d%02d%02d%02d%02d\n", get_version(), get_revision(), d.day, d.mon, d.year - 2000, d.hour, d.min);
    } else {
	log_write_log_file("WARN: Unknown program version. Compilation time: %s %s\n", __DATE__, __TIME__);
    }

    /* Запишем в статус частоты в МГц */
    log_write_log_file("INFO: Periph Clock %d MHz, Core clock %d MHz\n", dev_status.sclk, dev_status.cclk);

    /* Какой у нас CPU */
    log_write_log_file("INFO: Device ID: %04d with %s CPU\n", dev_addr.addr % 9999, (get_cpu_endian() == 1) ? "Big-endian" : "Little-endian");
    log_write_log_file("INFO: Built for CPU ver. %d, running on CPU ver. %d\n", built_for, running_on);

    /* Разобрали параметры */
    log_write_log_file("INFO: Params file was read and parsed OK\n");
    log_write_log_file("INFO: Work DIR: %s\n", gns110_param.gns110_dir_name);	/* Название директории для всех файлов */
    log_write_log_file("INFO: Setting Position: %d\n", gns110_param.gns110_pos);	/* Позиция установки */
    log_write_log_file("INFO: file len %d hour(s), freq %d, pga %d\n", gns110_param.gns110_file_len,	/* Размер файла данных в часах */
		       gns110_param.gns110_adc_freq,	/* Частота АЦП  */
		       gns110_param.gns110_adc_pga);	/* Усиление АЦП  */

    log_write_log_file("INFO: bitmap 0x%02X, consum %s, flt %f\n", gns110_param.gns110_adc_bitmap,	/* какие каналы используются */
		       gns110_param.gns110_adc_consum ? "Hi" : "Lo",	/* энергопотребление сюда */
		       gns110_param.gns110_adc_flt_freq);	/* Фильтр */

    log_write_log_file("INFO: Aquisition mode time: %d hours %02d min \n", dwt.time_work / 3600, dwt.time_work % 60);	/* Время работы в режиме сбора данных */
    log_write_log_file("INFO: Command Mode time: %d hours %02d min \n", dwt.time_cmd / 3600, dwt.time_cmd % 60);	/* Время работы в командном режиме */
    log_write_log_file("INFO: Modem operation time: %d hours %02d min \n", dwt.time_modem / 3600, dwt.time_modem % 60);	/* Время работы модема */
    log_write_log_file("INFO: This program can properly work until 2038\n");

    read_dac_coefs_from_eeprom(&dac_data);	/* Прочитать из flash начальные значения для ДАКов */
    log_write_log_file("INFO: Read DAC(19.2): %d, DAC(4.096): %d\n", dac_data.dac19_data, dac_data.dac4_data);

    /* Если различаются очень сильно - записать начальные */
    if (dac_data.dac19_data > DAC19_MAX_DATA || (int) dac_data.dac19_data <= 0) {
	dac_data.dac19_data = DAC19_INIT_DATA;
	log_write_log_file("WARN: Bad DAC(19.2), write default %d\n", dac_data.dac19_data);
    }
#if QUARTZ_CLK_FREQ==(19200000)
    /* Если различаются очень сильно - записать начальные */
    if (abs(dac_data.dac4_data - DAC4_INIT_DATA) > DAC4_MAX_DIFF) {
	dac_data.dac4_data = DAC4_INIT_DATA;
	log_write_log_file("WARN: Bad DAC coefs for VCXO 4.096, write old %d\n", dac_data.dac4_data);
    }
    // Есть ошибка?
    log_write_log_file("INFO: DAC4 test %s\n", (dev_status.st_test1 & 0x20) ? "FAIL" : "OK");
    log_write_log_file("INFO: tim3[0] = %d, tim3[1] = %d\n", dev_status.st_tim3[0], dev_status.st_tim3[1]);

    /* Заплатка! Есть ошибка?  - пока не можем проверить. */
    if (!(dev_status.st_test1 & 0x10)) {
	log_write_log_file("INFO: DAC19 test OK\n");
	log_write_log_file("INFO: tim4[0] = %d, tim4[1] = %d\n", dev_status.st_tim4[0], dev_status.st_tim4[1]);
	dac_data.dac19_coef = abs(dev_status.st_tim4[1] - dev_status.st_tim4[0]);	/* Коэфициент для прямой */
	log_write_log_file("INFO: delta = %d\n", dac_data.dac19_coef);
    } else {
	log_write_log_file("INFO: DAC19 test Fail! May be we use new GPS module.\n");
	log_write_log_file("INFO: We'll check DAC19 later!\n");
    }
#else
    /* Заплатка! Есть ошибка?  - пока не можем проверить. */
    if (!(dev_status.st_test1 & 0x10)) {
	log_write_log_file("INFO: DAC19 test OK\n");
	log_write_log_file("INFO: tim4 min = %d, tim4 max = %d\n", dev_status.st_tim4[0], dev_status.st_tim4[1]);
	dac_data.dac19_coef = abs(dev_status.st_tim4[1] - dev_status.st_tim4[0]);	/* Коэфициент для прямой */
	log_write_log_file("INFO: delta = %d\n", dac_data.dac19_coef);
    } else {
	log_write_log_file("INFO: DAC19 test Fail! May be we use new GPS module.\n");
	log_write_log_file("INFO: tim4 min = %d, tim4 max = %d\n", dev_status.st_tim4[0], dev_status.st_tim4[1]);
	log_write_log_file("INFO: We'll check DAC19 later!\n");
    }
#endif
    /* Проверим АЦП */
    for (i = 0; i < ADC_CHAN; i++) {
	if (dev_status.st_adc & (1 << i))
	    log_write_log_file("ERROR: ADS1282 # %d fail\n", i);
	else
	    log_write_log_file("INFO: ADS1282 # %d OK\n", i);
    }


    /* Какая то ошибка при настройках DAC */
    if (dev_status.st_test1 & 0x20) {
	log_write_log_file("ERROR: test dac for 4.096 VCXO\n");
	return -1;
    }

    /* количество прерываний */
    res = ADS1282_get_irq_count();
    log_write_log_file("INFO: test ADS1282 OK, num irq: %d\n", res);
    if (dev_status.st_main & 0x10) {
	log_write_log_file("ERROR: test ADS1282 error, no data ready signal!\n");
	return -1;
    }

    /* Сначала подали на все даки - пусть разогреюца */
    DAC_init();
    DAC_write(DAC_19MHZ, dac_data.dac19_data);
#if QUARTZ_CLK_FREQ==(19200000)
    DAC_write(DAC_4MHZ, dac_data.dac4_data);
#endif
    log_write_log_file("INFO: All DACs init OK\n");

    /* АЦП и подключенные устройства */
    adc_init(&dev_status);
    print_status(&dev_status);	/* Статус тестирования */
    print_set_times(&gns110_param);	/* Выведем полученные времена и смотрим грубые ошибки! */
    print_modem_type(&gns110_param);	/* Тип и номер модема */


    /* Если RTC исправны - установим статус часов. Установим номер устройства если не стоит ноль */
    if (!(dev_status.st_main & 0x01) && !(dev_status.st_test0 & 0x01)) {
	xRtcTicks.u_check.xRtcCheck.cStatus = CLOCK_RTC_TIME;	/* Берем время от RTC */

	/* Проверить времена */
	if (check_set_times(&gns110_param) < 0) {
	    return -1;
	}

	/* Проверим, как установили время для модема */
	if (check_modem_times(&gns110_param) < 0) {
	    return -1;
	}

	/* Если был непридвиденный сброс - модем не трогаем! */
	if (xShooseMode.reset != CAUSE_WDT_RESET) {
	    res = modem_init_all_types(&gns110_param);

	    if (res < 0) {
		log_write_log_file("ERROR: init modem!\n");
		return -1;
	    } else if (gns110_param.gns110_modem_type) {
		log_write_log_file("SUCCESS: init modem!\n");
		xShiftModemTime.bInit = true;
	    } else {
		log_write_log_file("INFO: no modem\n");
	    }
	}
    }


    print_ads1282_parms(&gns110_param);	/* Данные АЦП   */
    unselect_debug_uart();	/* Отключаем модем */
    log_write_log_file("INFO: Unselect debug UART OK\n");

    unselect_analog_power();	/* Выключить аналоговую часть */
    log_write_log_file("INFO: Unselect analog power OK\n");

    print_adc_data(&dev_status);	/* Печатаем данные среды, после инициализации еще не успела пройти запуск в adc_init()  */

    if (xShooseMode.reset == CAUSE_WDT_RESET) {
	return 1;
    } else {
	return 0;		/* Все OK */
    }
}

/************************************************************************
 *  Получить 3d fix от GPS, а так же дрифт после всплытия
************************************************************************/
#pragma section("FLASH_code")
static int OnGet3DFixState(void)
{
    char str[128];
    int t0, now, offset;
    int res = 0;

    now = get_sec_ticks();

    /* Установим время и будем ждать наступления 3dfix  */
    if (xRtcTicks.iTime0 == 0) {
	log_write_log_file("=======DEV_GET_3DFIX_STATE=======\n");

	LED_toggle(LED_BLUE);	/* Зажгем лампу */

	/* Синяя лампа медленно пока не получили 3d fix */
	LED_set_state(LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_SLOW_STATE);

	res = gps_init();	/* Откроем UART на новой скорости */
	if (res < 0)
	    return -1;		/* не могу открыть порт GPS */

	xRtcTicks.u_check.xRtcCheck.cPackCnt = 0;
	xRtcTicks.u_check.xRtcCheck.cCheck = 0;
	xRtcTicks.iTime0 = now;	/* Ставим метку на таймаут приема GPS - один час на вот это состояние + следующее */
	xRtcTicks.iSec = now;
	TIMER4_config();
	TIMER4_init_vector();
    }

    if (now - xRtcTicks.iSec == 2 && xRtcTicks.u_check.xRtcCheck.cPackCnt == 0) {
	xRtcTicks.u_check.xRtcCheck.cPackCnt = 1;
	gps_set_grmc();
	offset = gps_get_utc_offset();
	log_write_log_file("INFO: UTC Offset: %d sec\n", offset);
	log_write_log_file("INFO: Try to get valid NMEA for 60 min...\n");	/* Запишем в лог  */
    } else if (now - xRtcTicks.iSec > WAIT_3DFIX_TIME) {	/* Если уже больше часа настраиваем - на ошибку! */
	log_write_log_file("ERROR: Can't get 3dFix %d minutes\n", WAIT_3DFIX_TIME / 60);	/* Запишем в лог  */
	return -1;
    }

    /* Получили время и подождали минимум 16 пакетов подряд, чтобы для следущего состояния КА у нас уже был заполненный буфер  */
    t0 = gps_get_nmea_time();
    if (t0 > 0 && xRtcTicks.iTime1 != t0 && now - xRtcTicks.iTime0 > 4) {
	xRtcTicks.iTime1 = t0;

	LED_set_state(LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_QUICK_STATE);	// Синяя лампа быстро - получили 3d fix
	if (xRtcTicks.u_check.xRtcCheck.cPackCnt < TIM4_BUF_SIZE) {
	    /* Запишем в лог  */
	    if (gps_get_nmea_string(str, sizeof(str))) {
		log_write_log_file("NMEA:->%s\n", str);
	    }

	    xRtcTicks.u_check.xRtcCheck.cPackCnt++;
	} else {
	    long rtc;
	    s64 tim1, tim4;

	    /* Синхронизируем время - в этом случае ни UART ни TIMER4 не выключаем */
	    if (dev_state == DEV_GET_3DFIX_STATE) {
		TIMER1_set_sec(t0);	/* А теперь становим время в PRTC */
		rtc = get_rtc_sec_ticks();	/* Время RTC получили */
		set_rtc_sec_ticks(t0);	/* 3D FIX - Поставили в RTC */
		gps_get_nmea_string(str, sizeof(str));
		log_write_log_file("NMEA:->%s\n", str);	/* Запишем в лог  */
		sec_to_str(t0, str);
		log_write_log_file("NMEA time: %s\n", str);
		sec_to_str(rtc, str);
		log_write_log_file("RTC time:  %s\n", str);
		log_write_log_file("RTC error: %d sec\n", rtc - t0);
		xRtcTicks.u_check.xRtcCheck.cStatus = CLOCK_NO_GPS_TIME;	/* Пока у нас не настроен GPS  */
	    } else {
		tim1 = TIMER1_get_long_time();
		tim4 = TIMER4_get_long_time();
		if (tim4 == 0) {
		    log_write_log_file("ERROR: bad t4 time\n");
		    return 0;
		}

		/* Попробуем определить UTC offset */
		offset = gps_get_utc_offset();
		log_write_log_file("INFO: UTC Offset: %d sec\n", offset);

		rtc = get_rtc_sec_ticks();	/* Время RTC получили */
		xRtcTicks.iLabel = get_sec_ticks() - xRtcTicks.iLabel;
		xRtcTicks.iLabel = (xRtcTicks.iLabel == 0) ? 1 : xRtcTicks.iLabel;
		print_drift_and_work_time(tim1, tim4, rtc, xRtcTicks.iLabel);	/* Время работы и уход часов */
		xRtcTicks.iWasDrift = 1;
		gps_close();
		TIMER4_disable();	/* Выключим таймер */
	    }
	    xRtcTicks.iTime0 = 0;
	    res = 1;		/* Все OK  */
	}
    }

    /* Раз в минуту всегда! */
    else if (t0 == 0 && xRtcTicks.iTime1 != now && now % 60 == 0) {
	int i;
	xRtcTicks.iTime1 = now;
	/*       xRtcTicks.u_check.xRtcCheck.cPackCnt = 0; */
	i = gps_get_nmea_string(str, sizeof(str));
	if (i > 0) {
	    log_write_log_file("NMEA:->%s\n", str);
	    xRtcTicks.u_check.xRtcCheck.cCheck = 0;
	} else {
	    xRtcTicks.u_check.xRtcCheck.cCheck++;
	    /*   log_write_log_file("WARN: Can't get NMEA.\n"); */
	}
	/* нет приема в течении 5-ти минут */
	if (xRtcTicks.u_check.xRtcCheck.cCheck > 5) {
	    log_write_log_file("ERROR: Can't get NMEA, defective GPS module\n");
	    return -1;
	}
    }
    return res;
}

/************************************************************************
 * Функция подстройки кварца 19.2 МГц для таймера 1 (PRTC)
 ************************************************************************/
#pragma section("FLASH_code")
static int OnTuneQ19State(void)
{
    int i;
    s32 t0, t1, diff, sum = 0, res, sec, delta, offset;
    u32 rtc;
    long err[3];
    u64 ns1, ns4;
    char str[NMEA_GPRMC_STRING_SIZE];
    u16 old;

    /* Синяя лампа быстро - получили 3dfix  */
    log_write_log_file("=======DEV_TUNE_Q19_STATE=======\n");
    LED_set_state(LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_QUICK_STATE);

    log_write_log_file("INFO: Stage 1. Primary tuning...\n");	/* Запишем в лог  */
    err[0] = err[1] = err[2] = TIM1_GRANT_ERROR * 5;	/* Поставим ошибки. чтобы там не было нулей  */

    if (dac_data.dac19_coef < 10 || dac_data.dac19_coef > 250) {
	log_write_log_file("WARN: Bad dac19 coeficient. Set to default (98)\n");
	dac_data.dac19_coef = 98;
    }

    /* Проверить здесь и насчет запроса на выключение */
    do {
	old = dac_data.dac19_data;
	DAC_write(DAC_19MHZ, old);	/* Подали настройку  */

	/* Собираем буфер - вместо задержек */
	t0 = get_sec_ticks();

	while (get_sec_ticks() - t0 < TIM4_BUF_SIZE) {
	    LED_blink();	/* Собрали и есть достоверность + 1 секунда */

	    /* Соберем заново */
	    if (!gps_check_for_reliability()) {
		log_write_log_file("WARN: NMEA isn't valid. Reset buffer!\n");	/* Запишем в лог  */
		t0 = get_sec_ticks();
		return 1;	/* Сбросим время и уйдем на поиск NMEA */
	    }

	    if (t0 - xRtcTicks.iSec > WAIT_3DFIX_TIME) {
		log_write_log_file("ERROR: Can't to tune 19.2 MHz generator\n");	/* Запишем в лог  */
		return -1;	/* Если уже больше часа настраиваем - на ошибку! */
	    }
	}

	sum = TIMER4_get_ticks_buf_sum();	/* нашли сумму */
	dac_data.dac19_data = get_dac_ctrl_value(sum, old, dac_data.dac19_coef);

	err[2] = err[1];
	err[1] = err[0];
	err[0] = sum - TIM4_TICKS_FOR_16_SEC;	/* считаем ошибку - поставить еще через расчет ошибки? */

	log_write_log_file("Dac19(%d) Sum(%ld) Per(%d) Err(%d)\n", old, sum, sum / TIM4_BUF_SIZE, err[0]);
	LED_blink();		/* Моргнуть синей лампой примерно раз в полсекунды */

	/* условие выхода - ошибка три раза меньше TIM1_GRANT_ERROR */
    } while (!((abs(err[0]) <= TIM1_GRANT_ERROR) && (abs(err[1]) <= TIM1_GRANT_ERROR)
	       && (abs(err[2]) <= TIM1_GRANT_ERROR)));

    /* Запишем в лог  */
    log_write_log_file("INFO: Stage 2. Verification. Last errors: %d %d %d\n", err[0], err[1], err[2]);

    /* собираем буфер - вместо задержек */
    t0 = get_sec_ticks();
    while (get_sec_ticks() - t0 < TIM4_BUF_SIZE) {
	LED_blink();		/* Моргнуть синей лампой примерно раз в полсекунды */
    }
    sum = TIMER4_get_ticks_buf_sum();	/* нашли сумму */
    log_write_log_file("INFO: 19.2MHz tuned OK. Per(%ld) Err(%ld)\n", sum / TIM4_BUF_SIZE, sum - TIM4_TICKS_FOR_16_SEC);

    /* Настройка по-новому */
    TIMER4_del_vector();	/* Теперь можно смотреть только по флажку прерываний! */
    log_write_log_file("INFO: Stage 3. Synch timer1 with PPS ...\n");
    TIMER1_config();		/* Настриваем секундный таймер со своим PPS, но не запускаем! Разрешим прерывания для первого таймера  */
    LED_toggle(LED_BLUE);	/* Моргнули лампой  */

    for (i = 0; i < 2; i++) {
	while (TIMER4_wait_for_irq() != true);	/* Теперь ждем, когда стработает 4-й таймер, и сразу запускаем 1-й таймер  */
    }
    TIMER1_enable();		/* Таймер1 запущен */
    TIMER4_init_vector();	/* Снова запускаем Т4 */

    t0 = TIMER1_get_drift((u32 *) & err[0], (u32 *) & err[1]);	/* Померим дельту */
    log_write_log_file("INFO: Delta (t1 - t4) = %d ns (%d ticks)\n", t0, abs(err[0] - err[1]));
    log_write_log_file("INFO: Stage 4. shift phase...\n");

    delta = err[1] - err[0];
    if (delta != TIMER_PERIOD)
	TIMER1_shift_phase(delta);	/* Величину дельты t4 - t1 в период для подстройки - наоборот!  */

    /* Подождем сдвига таймера 2 - 3 секунды - использовать задержки а не получать секунды, т.к. таймер работает, но не считает еще! */
    for (i = 0; i < 500; i++) {
	delay_ms(5);
	LED_blink();		/* Моргнуть синей лампой примерно раз в полсекунды */
    }
    log_write_log_file("OK\n");

    log_write_log_file("INFO: Stage 5. Try to sync timer\n");

    /* Подождем пока таймер 1 находится дальше середины цикла.
     * PPS->|__xxxxxxxxxx__NMEA__________________________________|<-PPS
     * Должен быть ^ здесь (где xxx - перед приходом NMEA) чтобы взять нормальное время 
     */
    for (i = 0; i < 5; i++) {
	log_write_log_file("INFO: Try # %d to set timer !\n", i + 1);

	sec = TIMER1_sync_timer();	/* Синхронизируем таймер. Время синхронизации сохраним */
	LED_blink();		/* Моргнуть синей лампой примерно раз в полсекунды */

	if (sec < 5) {
	    log_write_log_file("INFO: Time %d, real %c\n", gps_get_nmea_time(), gps_check_for_reliability()? 't' : 'f');
	    delay_ms(5);	/* Подождем - запрос может случиться когда время сброшено */
	    continue;
	}

	/* Меряем снова значения + RTC */
	diff = TIMER1_get_drift(NULL, NULL);
	ns1 = TIMER1_get_long_time();
	ns4 = TIMER4_get_long_time();
	rtc = get_rtc_sec_ticks();
	print_drift_and_work_time(ns1, ns4, rtc, 0);

	/* Если время в секундах равно */
	if ((ns1 / TIMER_NS_DIVIDER) == (ns4 / TIMER_NS_DIVIDER)) {
	    break;
	}
    }
    if (i >= 5 || sec < 5) {
	log_write_log_file("ERROR: Can't set timer properly!\n");
	return -1;
    } else {
	log_write_log_file("INFO: Timer Start Time!\n");
    }

    /* Попробуем определить UTC offset */
    offset = gps_get_utc_offset();
    log_write_log_file("INFO: UTC Offset: %d sec\n", offset);

    /* Попробуем определить координаты, где получено врем и пр. */
    gps_get_coordinates(&t0, &t1);
    print_coordinates(t0, t1);
    log_write_log_file("-------------------------------------------------\n");

    /* Cоздать заголовок как только получили время:  5541.8005(N) ~ +55418005 */
    log_create_adc_header(ns4, diff, t0, t1);


    log_write_log_file("INFO: Turn off timer2\n");	/* Выключаем второй таймер и более его не используем. Рабочие секунды сохранятся  */
    IRQ_unregister_vector(TIM2_VECTOR_NUM);
    TIMER2_disable();

    res = write_dac19_coef_to_eeprom(dac_data.dac19_data);	/* сбросить на eeprom коэффициент с которым было настроено */
    log_write_log_file("INFO: write DAC19 coef (%d) on EEPROM %s\n", dac_data.dac19_data, res == 0 ? "success" : "error");


    xRtcTicks.iLabel = get_sec_ticks();	// Ставим метку времени - определим убегание генератора
    xRtcTicks.u_check.xRtcCheck.cStatus = CLOCK_PREC_TIME;	/* У нас настроен GPS  */
    gps_close();		/* Закрываем порт gps */
    TIMER4_disable();		/* Выключаем четвертый таймер - он нам долго будет не нужен  */


    return 0;			/* все хорошо? */
}

/*****************************************************************************
 * В случае если у нас произошла перезагрузка под водой
 * Настроим таймер просто по часам RTC
 *****************************************************************************/
#pragma section("FLASH_code")
static int OnTuneWithoutGPS(void)
{
    long t0;

    log_write_log_file("!!!!!!!!!!!!!!!!!! DEV_TUNE_WITHOUT_GPS_STATE !!!!!!!!!!!!!!!!!!\n");

    t0 = get_sec_ticks();	/* Получим секунды от таймера 2. Будем проверять RTC - нормальное у нас время или нет? */

    /* Коэффициент для DAC не может сильно отличаться */
    DAC_write(DAC_19MHZ, dac_data.dac19_data);	/* Подали настройку  */
    TIMER1_quick_start(t0);	/* Настриваем секундный таймер и запускаем */


    log_create_adc_header(t0 * TIMER_NS_DIVIDER, -1, 0, 0);
    log_write_log_file("INFO: Turn off timer2\n");	/* Выключаем второй таймер и более его не используем. Рабочие секунды сохранятся  */
    IRQ_unregister_vector(TIM2_VECTOR_NUM);
    TIMER2_disable();

    xRtcTicks.iLabel = get_sec_ticks();	/* Ставим метку времени - определим убегание генератора */
    xRtcTicks.u_check.xRtcCheck.cStatus = CLOCK_RTC_TIME;	/* У нас НЕ настроен GPS, поэтому время берем от RTC */
    return 0;
}

/*****************************************************************************
 * Переход в спящий режим перед регистрацией-проснуться по таймеру!
 * В случае если у нас опоздание в файле-запускаем все равно
 * Заходим один раз, все остальное время выдаем "0"
 *****************************************************************************/
#pragma section("FLASH_code")
static int OnSleepAndDiveState(void)
{
    long t0;

    if (!(xShooseMode.mode & MODE_SLEEP_LOCK)) {
	char str1[32], str2[32];
	int tgns, tmod;
	TIME_DATE d;
	int i;

	xShooseMode.mode |= MODE_SLEEP_LOCK;

	/* Зеленый редко моргает */
	LED_set_state(LED_SLOW_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE);

	log_write_log_file("=======DEV_SLEEP_AND_DIVE_STATE=======\n");


	if (gns110_param.gns110_modem_type > 0) {
	    tgns = get_sec_ticks();	/* Определим время GNS */
	    sec_to_str(tgns, str1);
	    log_write_log_file("GNS   time: %s\n", str1);
	    tmod = modem_check_modem_time(&gns110_param);	/* Определим время модема */
	    sec_to_str(tmod, str2);
	    log_write_log_file("Modem time: %s\n", str2);
	}


	/* Установим модем если неисправен - сброс был не по WDT */
	if (xShooseMode.reset != CAUSE_WDT_RESET && gns110_param.gns110_modem_type > 0 && xShiftModemTime.bInit && abs(tgns - tmod) > 300) {
	    log_write_log_file("----------------Init modem---------------\n");

	    /* Проверим то что  записали */
	    t0 = modem_init_all_types(&gns110_param);
	    if (t0 < 0)
		return -1;

	    unselect_debug_uart();	/* Отключаем модем */
	}
	/*  Проверить время с уже настроенным генератором */
	if (check_set_times(&gns110_param)) {
	    return -1;
	}
    }

    /* Ждем, когда нужно проснуться для подстройки */
    t0 = get_sec_ticks();
    if ((int) gns110_param.gns110_wakeup_time <= (int) t0) {
	return 1;
    } else {
	return 0;
    }
}

/************************************************************************************ 
 * Проснуться и начать сделать первичную инициализацию кварца на 4 МГц
 * Как только подстроил генератор 4.096 - установить все остальные обработчики
 ************************************************************************************/
#pragma section("FLASH_code")
static int OnWakeupState(void)
{
    s32 t0;
    bool res;

    /* Зеленый редко моргает */
    LED_set_state(LED_SLOW_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE);

    log_write_log_file("=======DEV_WAKEUP_STATE=======\n");

    t0 = get_sec_ticks();

    /* Моргнули зеленым */
    LED_toggle(LED_GREEN);

#if QUARTZ_CLK_FREQ==(19200000)
    log_write_log_file("INFO: 4MHz quart tunning.\n");
#endif
    select_analog_power();	/* Включаем аналоговую часть */
    log_write_log_file("INFO: Analog power for ADS1282 ON\n");

    ADS1282_standby();		/* Выключить АЦП  */

#if QUARTZ_CLK_FREQ==(19200000)

    log_write_log_file("INFO: set %d on DAC for 4MHz VCXO\n", dac_data.dac4_data);

    /* Делаем первичную настройку кварца 4 МГц. Начальный коэффициент кварца 4 у нас есть */
    DAC_write(DAC_4MHZ, dac_data.dac4_data);

    /* Установим 3-й таймер примерно на середине счета таймера 1 */
    while (TIMER1_get_counter() > (TIMER_HALF_PERIOD - TIMER_TUNE_SHIFT)
	   && TIMER1_get_counter() < (TIMER_HALF_PERIOD + TIMER_TUNE_SHIFT));
    TIMER3_init();

    LED_toggle(LED_GREEN);	/* Моргнули */

    /* Получили секунды  */
    t0 = get_sec_ticks();	/* Получили секунды  */
    while ((get_sec_ticks() - t0) < TIM3_DELAY_SEC) {
	LED_blink();		/* На разогрев - 5 секунд, пока не соберется буфер */
    }


    log_write_log_file("INFO: wait for timer3 tune freq and phase\n");
    TIMER3_shift_phase(t0);	/* Поймаем фазу */

    /* Ждем пока таймер 3 подстроиться в течении макс. 10 секунд! */
    t0 = get_sec_ticks();
    do {
	res = TIMER3_is_shift_ok();
	if (res == true)
	    break;
	LED_blink();
    } while (get_sec_ticks() - t0 < WAIT_TIM3_SYNC);

    if (res) {
	log_write_log_file("INFO: timer3 tuned phase and freq OK\n");
    } else {
	log_write_log_file("INFO: timer3 was not tuned correctly. Continue to work\n");
    }
#endif
    return 0;			/* Даже если не подстроили  */
}

/************************************************************************************ 
 *  Регистрация, вызывается 1 раз в секунду
 ************************************************************************************/
#pragma section("FLASH_code")
static int OnRegState(void)
{
    int err;

    /* не заходить больше сюда */
    if (!(xShooseMode.mode & MODE_REG_LOCK)) {
	xShooseMode.mode |= MODE_REG_LOCK;

	/* Зеленый редко моргает */
	LED_set_state(LED_SLOW_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE);
	write_reset_cause_to_eeprom(CAUSE_WDT_RESET);	/* Запишем причину сброса - при сбросе будет работать без подстройки */
	log_write_log_file("=======DEV_REG_STATE=======\n");
    }

    /* Если запустились - вывести в лог! */
    if (xShooseMode.u_reg.xCtrlReg.bStart && !xShooseMode.u_reg.xCtrlReg.bBeginReg) {
	xShooseMode.u_reg.xCtrlReg.bBeginReg = true;
	log_write_log_file(">>>>>>>>>>>>>>>>>>Begin record<<<<<<<<<<<<<<<<<<<<<\n");
	LED_set_state(LED_QUICK_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_OFF_STATE);	/* Зеленый часто моргает */
    }


    /* После настройки частоты и фазы - 1 раз подаем SYNC, очередность внутри if именно такая!
     * Конфигурировать АЦП на частоту freq , pga и формат слова... */
    if (!xShooseMode.u_mode.xCtrlMode.bSync
#if QUARTZ_CLK_FREQ==(19200000)
	&& TIMER3_is_tuned_ok()
#endif
	) {
	if (begin_reg() < 0) {
	    return -1;
	}
#if QUARTZ_CLK_FREQ==(19200000)
	err = write_dac4_coef_to_eeprom(dac_data.dac4_data);	/* сбросить на eeprom коэффициент с которым было настроено */
	log_write_log_file("INFO: %s: write DAC 4.096 coef(%d) on EEPROM\n", err == 0 ? "SUCCESS" : "ERROR", dac_data.dac4_data);
#endif

	xShooseMode.u_mode.xCtrlMode.bSync = true;

	/* Сдвинуть время старта и финиша, а если время окончания меньше чем "сейчас" - выйти */
	if (check_start_time(&gns110_param) != 0) {
	    log_write_log_file("ERROR: checking time\n");
	    return -1;
	}

	TIMER1_set_callback(timer1_callback_func);	/* Установить будильники на все времена: работа, окончание, всплытие и пр. */
	log_write_log_file("INFO: Timers were tuned and handlers were installed\n");
	print_set_times(&gns110_param);
    }

    /* Моргаем желтым - SIGNAL handler  */
    if (ADS1282_get_handler_flag()) {
	ADS1282_reset_handler_flag();
	LED_toggle(LED_SIGNAL);
    }

    return 0;
}

/************************************************************************
 * Функция окончания регистрации
 ************************************************************************/
#pragma section("FLASH_code")
static int OnFinishRegState(void)
{
    /* не заходить больше сюда */
    if (!(xShooseMode.mode & MODE_FIN_LOCK)) {
	char str[32];
	xShooseMode.mode |= MODE_FIN_LOCK;

	/* если не было окончания */
	if (!(xShooseMode.mode & MODE_HALT_LOCK)) {
	    LED_set_state(LED_OFF_STATE, LED_SLOW_STATE, LED_OFF_STATE, LED_OFF_STATE);	/* Огоньки, желтый редко */
	}
	xShooseMode.u_reg.xCtrlReg.bFinish = true;
	write_reset_cause_to_eeprom(CAUSE_UNKNOWN_RESET);	/* поставить причину сброса - непредвиденный (если вытащим питание) */
	log_write_log_file("=======DEV_FINISH_REG_STATE=======\n");
	finish_reg();		/* Закончить регистрацию */
    }
    return 0;
}



/*************************************************************************************** 
 * Завершение работы
 ****************************************************************************************/
#pragma section("FLASH_code")
static int OnHaltState(void)
{
    /* не заходить больше сюда */
    if (!(xShooseMode.mode & MODE_HALT_LOCK)) {
	xShooseMode.mode |= MODE_HALT_LOCK;

	/* Ожидание подъема на борт - моргают две лампы */
	log_write_log_file("=======DEV_HALT_STATE=======\n");
	LED_set_state(LED_OFF_STATE, LED_QUICK_STATE, LED_OFF_STATE, LED_QUICK_STATE);

	if (gps_init() < 0)
	    return -1;

	IRQ_unregister_vector(NMEA_VECTOR_NUM);	/* Отрегистрируем прерывание порт gps */
	log_write_log_file("INFO: Unregister GPS vector\n");


	/* Всегда делать!  */
	if (gns110_param.gns110_modem_type > 0) {
	    if (modem_set_radio(&gns110_param) < 0)
		return -1;
	}
    }
    return 0;			/* Всегда  */
}


/*************************************************************************************** 
 * Выключение питания
 ****************************************************************************************/
#pragma section("FLASH_code")
static int OnPowerOffState(void)
{
    char str[32];
    log_write_log_file("=======DEV_POWER_OFF_STATE=======\n");

    print_timer_and_sd_card_error();

    /* Завершим регистрацию - если не было завершения */
    if (!xShooseMode.u_reg.xCtrlReg.bFinish) {
	finish_reg();
    }

    POWER_off(CAUSE_POWER_OFF, xShooseMode.u_burn.xCtrlBurn.bPowerOff);
    return 0;			/* Не обязательно! */
}

/************************************************************************************ 
 *  Ошибка
 ************************************************************************************/
#pragma section("FLASH_code")
static int OnErrorState(void)
{
    int res = 0;
    int sec = get_sec_ticks();

    /* не заходить больше сюда */
    if (!(xShooseMode.mode & MODE_ERR_LOCK)) {
	xShooseMode.mode |= MODE_ERR_LOCK;

	LED_all_off();		/* Выключить все для начала */
	xShooseMode.u_reg.xCtrlReg.cLock = LOCK_NONE_REQ;

	/* Красная часто */
	LED_set_state(LED_OFF_STATE, LED_OFF_STATE, LED_QUICK_STATE, LED_OFF_STATE);
	log_write_log_file("=======DEV_ERROR_STATE=======\n");
	TIMER1_del_callback();	/* Если есть установленные времена - убрать их */
	xShooseMode.time = sec;	/* Ставим секунды */
    }

    /* Выключаем, если в ошибке > 10 минут */
    if (sec - xShooseMode.time > POWEROFF_NO_LINK_TIME) {
	log_write_log_file("INFO: 10 minutes expired. Power off\n");
	xShooseMode.u_burn.xCtrlBurn.bPowerOff = false;	/* без модема */
	res = -1;
    }

    return res;
}

/****************************************************************************
 * Режим аварийного ожидания
 ****************************************************************************/
#pragma section("FLASH_code")
static int OnEmergencyModeState(void)
{
    /* Не заходить сюда больше  */
    if (!(xShooseMode.mode & MODE_EMERGENCY_LOCK)) {
	xShooseMode.mode |= MODE_EMERGENCY_LOCK;
	LED_set_state(LED_OFF_STATE, LED_SLOW_STATE, LED_SLOW_STATE, LED_OFF_STATE);	/* Желтый редко, красный редко */
    }
    return 0;
}

/****************************************************************************
 * Командный режим (работа от PC)
 ****************************************************************************/
#pragma section("FLASH_code")
static int OnCommandModeState(void)
{
    int temp;
    DEV_DAC_STRUCT dac;
    u8 buf[256];
    u16 freq, par;
    int i;
    u8 res;
    int ret = 0;

    /* Не заходить сюда больше и сделать тесты */
    if (!(xShooseMode.mode & MODE_CMD_LOCK)) {
	xShooseMode.mode |= MODE_CMD_LOCK;


	LED_set_state(LED_QUICK_STATE, LED_OFF_STATE, LED_OFF_STATE, LED_QUICK_STATE);	/* Синий часто, зеленый часто  */

	select_analog_power();

	eeprom_test(&dev_status);	/* Тест eeprom  */
	test_gps(&dev_status);	/* Тест GPS  */
	test_reset(&dev_status);	/* Получили причину сброса */
	rtc_test(&dev_status);	/* Test RTC: Прочитаем время из RTC, если ошибка - не используем */
	test_bmp085(&dev_status);	/* Test Датчик температуры и давления:  Прочитаем коэффициенты из bmp085 */
	test_acc(&dev_status);	/* Test Акселерометра: */
	test_cmps(&dev_status);	/* Test Компаса: */
	test_adc(&dev_status);	/* Чтобы появились коэффициенты АЦП */

	DAC_init();
	dac.dac19_data = DAC19_INIT_DATA;
	dac.dac4_data = DAC4_INIT_DATA;	/* настроим ЦАПЫ */
	DAC_write(DAC_19MHZ, dac.dac19_data);
	DAC_write(DAC_4MHZ, dac.dac4_data);
	delay_ms(50);		/* раскачали */
	dev_status.st_main |= 0x40;	/* Ставим статус: остановлен */

	i = get_rtc_sec_ticks();	/* время RTC */
	xShooseMode.time = i;
	set_sec_ticks(i);
    }

    return 0;
}

/**
 * Выполнить внешнюю команду от UART.!
 */
#pragma section("FLASH_code")
static void exec_uart_cmd(DEV_STATE_ENUM * state)
{
    int res;
    s64 t1;

    if (*state != DEV_COMMAND_MODE_STATE)
	return;

    /* Выполняем только в командном режиме  */
    if (uart_cmd.cmd != 0) {

	xShooseMode.time = get_sec_ticks();	/* отодвигаем время выключения */


	/* Смотрим параметр */
	switch (uart_cmd.cmd) {

	    // данные в eeprom по умолчанию
	case UART_CMD_ZERO_ALL_EEPROM:
	    write_default_data_to_eeprom(&dev_status);
	    break;

	case UART_CMD_INIT_TEST:
	    test_all(&dev_status);
	    break;

	    /* Выключение питания БЕЗ выключения модема */
	case UART_CMD_POWER_OFF:
	    xShooseMode.u_burn.xCtrlBurn.bPowerOff = false;
	    *state = DEV_POWER_OFF_STATE;
	    break;

	    /* Сброс DSP. Запишем и причину сброса */
	case UART_CMD_DSP_RESET:
	    delay_ms(50);
	    write_reset_cause_to_eeprom(CAUSE_EXT_RESET);
	    PLL_reset();
	    break;

	    /* Управление реле пережиг ВКЛ */
	case UART_CMD_BURN_ON:
	    burn_wire_on();
	    break;

	    /* Управление реле пережиг ВЫКЛ */
	case UART_CMD_BURN_OFF:
	    burn_wire_off();
	    break;

	    /* Управление GPS ВКЛ */
	case UART_CMD_GPS_ON:
	    res = gps_init();
	    if (res == 0) {
		dev_status.st_test0 &= ~0x10;
		dev_status.st_test1 |= 0x01;	// Включили GPS 
		xNmeaSet.bEnter = true;
		xNmeaSet.lTime0 = get_sec_ticks();
	    } else {
		dev_status.st_test0 |= 0x10;
	    }
	    break;

	    /* Управление GPS ВЫКЛ */
	case UART_CMD_GPS_OFF:
	    gps_close();
	    xNmeaSet.bEnter = false;
	    xNmeaSet.lTime0 = 0;
	    dev_status.st_test1 &= ~0x01;	// вЫключили GPS
	    break;

	    /* Управление: включить реле модема. И Сдвинуть ВРЕМЯ! */
	case UART_CMD_MODEM_ON:
	    modem_on();
	    dev_status.st_test1 |= 0x06;	// Включили модем  + тест модема
	    xShiftModemTime.bEnter = true;
	    xShiftModemTime.lTime0 = get_sec_ticks();
	    break;

	    /* Управление: вЫключить реле модема   */
	case UART_CMD_MODEM_OFF:
	    modem_off();
	    dev_status.st_test1 &= ~0x02;	// вЫключили модем 
	    xShiftModemTime.bEnter = false;
	    xShiftModemTime.lTime0 = 0;
	    break;


	    /* Управление: ретранслировать команду модему  */
	case UART_CMD_MODEM_REQUEST:
	//vvvv:    delay_ms(50);
	    dev_status.st_main |= 0x02;	// Идет тестирование модема - ставим ошибку модема
	    if (modem_convey_buf((char *) uart_cmd.u.cPar, uart_cmd.len % MODEM_BUF_LEN) == 0) {
		dev_status.st_main &= ~0x02;	// Сняли ошибку
	    }
	    break;

	    /* Старт осцилограф */
	case UART_CMD_DEV_START:
	    start_adc_osciloscope((char *) uart_cmd.u.cPar);
	    dev_status.st_main &= ~0x02;	// Сняли ошибку
	    break;

	    /* Старт осцилограф */
	case UART_CMD_DEV_STOP:
	    stop_adc_osciloscope();
	    dev_status.st_main &= ~0x02;	/* Сняли ошибку */
	    break;


	default:
	    break;

	}
	uart_cmd.cmd = UART_CMD_NONE;
    }
    // через 5 секунд подаем команду на UART0
    if (xNmeaSet.bEnter == true) {
	long t1 = get_sec_ticks();
	if (t1 - xNmeaSet.lTime0 > 5) {

	    // ААА..иначе забьет без этого другими строками
	    gps_set_grmc();
	    xNmeaSet.bEnter = false;
	}
    }
    // включаем и ждем 5 секунды
    // Ждать ~5 секунд (подобрать!)
    if (xShiftModemTime.bEnter == true) {

	// Включаем модем после ожидания и обнуляем команду и enter в конце
	if (get_sec_ticks() - xShiftModemTime.lTime0 > MODEM_POWERON_TIME_SEC) {

/////vvvvv:
	    gns110_param.gns110_modem_type = GNS110_MODEM_AM3;
	    if (modem_check_params(&gns110_param) == 0) {
		dev_status.st_main &= ~0x02;
	    } else {
		dev_status.st_main |= 0x02;
	    }
	    dev_status.st_test1 &= ~0x04;
	    xShiftModemTime.bEnter = false;
	}
    }
}


/** 
 * Выполнить внешнюю команду от UART!
 * которая пришла в прерывании порта UART
 */
#pragma section("FLASH_code")
void make_extern_uart_cmd(void *i, void *o)
{
    if (i != NULL && o != NULL) {
	memcpy(&uart_cmd, i, sizeof(uart_cmd));	/* Какую команду исполнять */
	memcpy(o, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	*(u8 *) o = 2;
    }
}


/* Получить время ИЗ RTC не чаще раза в секунду  */
#pragma section("FLASH_code")
void cmd_get_gns110_rtc(void *buf)
{
    int time;

    time = get_sec_ticks();
    if (xRtcTicks.iTime0 != time) {
	xRtcTicks.iTime0 = time;
	xRtcTicks.iSec = get_rtc_sec_ticks();
    }

    if (buf != NULL) {
	*(u8 *) buf = 0x04;	/* Длина в первом байте */
	memcpy((u8 *) buf + 1, &xRtcTicks.iSec, 4);
    }
}


/* Запуск всех АЦП в командном режиме */
section("FLASH_code")
void start_adc_osciloscope(void *in)
{
    ADS1282_Params par;
    u32 i, freq;
    f32 flt;

    dev_status.st_main |= 0x04;	// ошибка в команде

    if (!ADS1282_is_run()) {

	freq = get_long_from_buf(in, 0);	// частота
	if (freq == 4000)
	    par.sps = SPS4K;
	else if (freq == 2000)
	    par.sps = SPS2K;
	else if (freq == 1000)
	    par.sps = SPS1K;
	else if (freq == 500)
	    par.sps = SPS500;
	else if (freq == 250)
	    par.sps = SPS250;
	else if (freq == 125)
	    par.sps = SPS125;
	else 
	   par.sps = SPS62;

	i = get_long_from_buf(in, 4);	// Получили PGA
	if (i == 64)
	    par.pga = PGA64;
	else if (i == 32)
	    par.pga = PGA32;
	else if (i == 16)
	    par.pga = PGA16;
	else if (i == 8)
	    par.pga = PGA8;
	else if (i == 4)
	    par.pga = PGA4;
	else if (i == 1)
	    par.pga = PGA1;
	else
	    par.pga = PGA2;

	par.mode = CMD_MODE;	// 8

	i = get_long_from_buf(in, 12);	// 12 HI or LO res
	par.res = (i > 0) ? 1 : 0;

	i = get_long_from_buf(in, 16);	// Получили CHOP
	par.chop = (i > 0) ? 1 : 0;


	flt = get_float_from_buf(in, 20);
	par.hpf = get_hpf_from_freq(flt, freq);

	if (ADS1282_config(&par) == false) {
	    dev_status.st_main |= 0x04;	// ошибка в команде
	} else {
	    ADS1282_start();	/* Запускаем АЦП с PGA  */
	    ADS1282_start_irq();	/* Разрешаем IRQ  */
	    dev_status.st_main &= ~0x44;	// Сняли бит
	}
    }
}


/* Стоп всех АЦП */
#pragma section("FLASH_code")
void stop_adc_osciloscope(void)
{
    ADS1282_stop();
    dev_status.st_main |= 0x40;	// остановлен
}

/* Очистить буфер данных*/
#pragma section("FLASH_code")
void cmd_clear_adc_buf(void *out)
{
    if (out != NULL) {
	ADS1282_clear_adc_buf();
	dev_status.st_main &= ~0x20;	// Переполнение памяти снимаем
    }
    memcpy(out, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина  */
    *(u8 *) out = 2;		/* Отвечаем: 2 байта статуса */
}


/* Получить данные */
#pragma section("FLASH_code")
void cmd_get_adc_data(void *buf)
{
    if (buf != NULL) {

	if (ADS1282_get_pack(buf) == true) {
	    dev_status.st_main &= ~0x20;	/* снимаем переполнение буфера */
	    *(u8 *) buf = sizeof(ADS1282_PACK_STRUCT) - 3;	/* Передаем столько байт (252 -3) */
	} else {
	    memcpy(buf, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	    *(u8 *) buf = 2;	/* Отвечаем: 2 байта статуса */
	}
    }
}


/* Обнулить eeprom - поставить данные по-умолчанию  */
#pragma section("FLASH_code")
void cmd_zero_all_eeprom(void *buf)
{
    if (buf != NULL) {
	memcpy(buf, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	*(u8 *) buf = 2;	/* Отвечаем: 2 байта статуса */
    }
}

/* Записать константы во все каналы */
#pragma section("FLASH_code")
void cmd_set_adc_const(void *in, void *out)
{
    u32 i;
    bool res;
    ADS1282_Regs regs;


    /* Данные упорядочены как в структуре ADS1282_regs. Первое число - magic */
    if (in != NULL && out != NULL) {
	memcpy(&regs, in, sizeof(ADS1282_Regs));

	for (i = 0; i < ADC_CHAN; i++)
	    ADS1282_set_adc_const(i, regs.chan[i].offset, regs.chan[i].gain);	// Ставим константы


	/* Записать константы */
	res = ADS1282_set_adc_const(0xff, 0, 0);

	if (regs.magic == MAGIC && res == true) {
	    dev_status.st_test1 &= ~0x08;	// Снимаем статус - нет констант 
	} else {
	    dev_status.st_main |= 0x04;	// Посылаем статус - Ошибка в команде? */
	}

	memcpy(out, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина  */
	*(u8 *) out = 2;	/* Отвечаем: 2 байта статуса */
    }
}


/* Получить константы всех каналов  */
#pragma section("FLASH_code")
void cmd_get_adc_const(void *buf)
{
    ADS1282_Regs regs;		// константы каналов
    u32 a, b;
    int i;

    if (buf != NULL) {
	for (i = 0; i < ADC_CHAN; i++) {
	    ADS1282_get_adc_const(i, &a, &b);	// Получаем константы
	    regs.chan[i].offset = a;
	    regs.chan[i].gain = b;
	}
	/* Скопируем в буфер со второго байта (в первом будет длина) regs.magic = MAGIC; */
	memcpy((u8 *) buf + 1, &regs, sizeof(ADS1282_Regs));
	*(u8 *) buf = sizeof(ADS1282_Regs);	/* Передаем столько байт - в структуре нету других полей! */
    } else {
	memcpy(buf, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	*(u8 *) buf = 2;	/* Отвечаем: 2 байта статуса */
    }
}

/* Получить адрес и версию ПО модуля */
#pragma section("FLASH_code")
void cmd_get_dsp_addr(void *par)
{
    dev_state = DEV_COMMAND_MODE_STATE;
    dev_addr.len = sizeof(DEV_ADDR_STRUCT) - 3;	/* xxx байт передаем  */

    if (par != NULL)
	memcpy(par, &dev_addr, sizeof(dev_addr));
}

/**
 * Установить номер модуля (как имя или адрес )
 */
#pragma section("FLASH_code")
void cmd_set_dsp_addr(u16 addr, void *buf)
{
    write_mod_id_to_eeprom(addr);

    dev_status.st_main &= ~0x04;	/* снимаем "ошибка" */

    if (buf != NULL)
	memcpy(buf, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
    *(u8 *) buf = 2;		/* Отвечаем: 2 байта статуса */
}


/* Получить время работы прибора */
#pragma section("FLASH_code")
void cmd_get_work_time(void *buf)
{
    if (buf != NULL) {
	dwt.len = sizeof(DEV_WORK_TIME_STRUCT) - 3;
	memcpy(buf, &dwt, sizeof(DEV_WORK_TIME_STRUCT));
    }
}

/* Записать время работы прибора */
#pragma section("FLASH_code")
void cmd_set_work_time(u32 t1, u32 t2, u32 t3, void *buf)
{
    u32 res = 0;

    if (buf != NULL) {
	res = write_time_work_to_eeprom(t1);
	res |= write_time_cmd_to_eeprom(t2);
	res |= write_time_modem_to_eeprom(t3);

	if (res == 0) {
	    dev_status.st_main &= ~0x04;
	    dwt.time_work = t1;	/* Время работы в режиме сбора данных */
	    dwt.time_cmd = t2;	/* Время работы в командном режиме */
	    dwt.time_modem = t3;	/* Время работы модема */
	} else {
	    dev_status.st_main |= 0x04;
	}

	memcpy(buf, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	*(u8 *) buf = 2;	/* Отвечаем: 2 байта статуса */
    }
}


/* синхронизация */
#pragma section("FLASH_code")
void cmd_set_gns110_rtc(u32 sec, void *par)
{
    dev_status.st_main &= ~0x01;	/* Сбрасываем. "нет времени" */
    set_rtc_sec_ticks(sec);

    /* Поставим и таймер TIM1 */
    set_sec_ticks(sec);


    /* Отодвигаем время сброса */
    xShooseMode.time = sec;

    if (par != NULL) {
	memcpy(par, &dev_status, 3);	/* Отвечаем: OK, 2 байта статуса (не полным!) + длина */
	*(u8 *) par = 2;	/* 2 младших байта статуса */
    }
}

/* Выставить статус */
#pragma section("FLASH_code")
void cmd_set_dsp_status(void *par)
{
    if (par != NULL)
	memcpy(&dev_status, par, sizeof(dev_status));
}

/**
 * Моргать лампами в зависимости от режима работы
 * пока просто передадим управление
 * Если пережиг - быстро моргать желтым
 */
#pragma section("FLASH_code")
static void blink_all_leds(DEV_STATE_ENUM state)
{
    if (true == xShooseMode.u_burn.xCtrlBurn.bExternBurn) {
	LED_toggle(LED_YELLOW);
	delay_ms(100);
    } else {
	LED_blink();
    }
}

/**
 * Фильтруем напряжение
 */
#pragma section("FLASH_code")
static s16 filt_reg_power(s16 u)
{
    int volt = 0, i;

    /* Забиваем напряжение в отсчеты */
    xVoltArray.volts[xVoltArray.count % POWER_DROP_COUNT] = u;	/* Собираем напряжение */
    xVoltArray.count++;

    for (i = 0; i < POWER_DROP_COUNT; i++) {
	volt += xVoltArray.volts[i];
    }

    volt /= POWER_DROP_COUNT;

    return (s16) volt;
}



/**
 * Считаем время работы каждую секунду
 */
#pragma section("FLASH_code")
static void count_work_time(DEV_STATE_ENUM state)
{
    static long time = 0;
    long ticks, wt;

    ticks = get_sec_ticks();

    if (time != ticks) {	/* раз в секунду */
	time = ticks;

	if (state == DEV_COMMAND_MODE_STATE) {
	    dwt.time_cmd++;	/* Получаем время работы и увеличиваем */

	} else if (state > DEV_INIT_STATE) {
	    dwt.time_work++;	/* Получаем время работы и увеличиваем */

	    if (gns110_param.gns110_modem_type > 0) {
		dwt.time_modem++;	/* Время работы модема  */
	    }
	}

	/* Сбросим на eeprom раз в ***минуту. Не в начале минуты!!! */
	if (time % 60 == 0) {
	    write_work_time();
	}
    }
}

/**
 * Записать в eeprom время работы----посмотреть чтобы не накладывалось с пред. функцией!
 */
#pragma section("FLASH_code")
void write_work_time(void)
{
    write_time_cmd_to_eeprom(dwt.time_cmd);
    write_time_work_to_eeprom(dwt.time_work);
    write_time_modem_to_eeprom(dwt.time_modem);
}

/**
 * Выдать статус часов:
 * N - нет времени
 * R - время RTC
 * G - GPS не подстроен
 * P - точное время
 */
#pragma section("FLASH_code")
int get_clock_status(void)
{
    return (int) xRtcTicks.u_check.xRtcCheck.cStatus;
}

/**
 * Установить статус часов
 */
#pragma section("FLASH_code")
void set_clock_status(int s)
{
    xRtcTicks.u_check.xRtcCheck.cStatus = s;
}


/*************************************************************************************
 *    Функции подстройки кварца 4.096 МГц живут здесь 
 *    будет вызыватся в бесконечном цикле,
 *    так же опрашиваем АЦП
 *************************************************************************************/
#if QUARTZ_CLK_FREQ==(19200000)
#pragma section("FLASH_code")
static void quartz4_tune(DEV_STATE_ENUM state)
{
    static int tick = 0;
    int t0;

    t0 = TIMER3_get_sec_ticks();

    /* Подстраивать кварц 4 МГц если работает таймер3 и находимся в режиме регистрации */
    if (TIMER3_is_run() && tick != t0 && state >= DEV_REG_STATE) {
	int dac, freq, phase, per;

	tick = t0;

	/* Попробуем отрегулировать период каждую секунду:
	 * Если ошибка частоты < 0 - таймер 3 идет быстрее чем таймер 1
	 * т.е он быстрее снимает метки
	 * если ошибка  частоты > 0 - таймер 3 идет медленнее чем таймер 1   
	 * Если фаза  < 0 - таймер 3 опережает таймер 1
	 * Если фаза  > 0 - таймер 3 опаздывает по отношению к таймеру 1    */
	freq = TIMER3_get_freq_err();
	phase = TIMER3_get_phase_err();
	per = TIMER3_get_period();

	dac = freq - phase / 10 + dac_data.dac4_data;	/* Уравнение регулятора для 48 МГц */

	/* против переполнения, ЦАП работает на пределе разрядной величины!!! 
	 * Какую проверку добавить, если фаза не будет подстраиваца? */
	if (dac > 0x3fff)
	    dac = 0x3fff;
	if (dac < 0)
	    dac = 0;

	dac_data.dac4_data = dac;
	DAC_write(DAC_4MHZ, dac_data.dac4_data);	/* Подать на ЦАП, и после подачи будем делать выдержку - кварц инерционный!  */

	/* Пишем в лог только при превышении ОБОИХ пределов */
	if ((abs(freq) > TIM3_GRANT_ERROR) && (abs(phase) > TIM3_GRANT_ERROR)) {
	    log_write_log_file("dac4 =  %d: phase = %d, per = %ld, err = %ld\n", dac, phase, per, freq);
	}
    }
}
#endif

/**
 * Начать регистрацию в рабочем режиме
 */
#pragma section("FLASH_code")
static int begin_reg(void)
{
    ADS1282_Params par;
    u16 freq;

    freq = gns110_param.gns110_adc_freq;

    switch (freq) {
    case 4000:
	par.sps = SPS4K;
	break;

    case 2000:
	par.sps = SPS2K;
	break;

    case 1000:
	par.sps = SPS1K;
	break;

    case 500:
	par.sps = SPS500;
	break;

    case 250:
	par.sps = SPS250;
	break;
#if QUARTZ_CLK_FREQ==(8192000)
    case 125:
	par.sps = SPS125;
	break;

    default:
	par.sps = SPS62;	/* По умолчанию - всегда 62.5 */
	break;
#endif
    }


    par.pga = (gns110_param.gns110_adc_pga == 1) ? PGA1 : (gns110_param.gns110_adc_pga == 2) ?
	PGA2 : (gns110_param.gns110_adc_pga == 4) ? PGA4 : (gns110_param.gns110_adc_pga == 8) ?
	PGA8 : (gns110_param.gns110_adc_pga == 16) ? PGA16 : (gns110_param.gns110_adc_pga == 32) ? PGA32 : PGA64;

    par.file_len = gns110_param.gns110_file_len;
    par.mode = WORK_MODE;	/* Рабочий режим */
    par.res = gns110_param.gns110_adc_consum;
    par.hpf = get_hpf_from_freq(gns110_param.gns110_adc_flt_freq, gns110_param.gns110_adc_freq);
    par.bitmap = gns110_param.gns110_adc_bitmap;

    /* Файлы на 24 часа, пока нет параметра  */
    if (ADS1282_config(&par) == false) {
	return -1;
    }

    /* Запускаем АЦП и синхронизируем АЦП */
    ADS1282_start();
    return 0;
}


/**
 *  Завершить регистрацию в рабочем режиме
 */
#pragma section("FLASH_code")
static void finish_reg(void)
{
    log_write_log_file(">>>>>>>>>>>>>>>>>>>Finish record<<<<<<<<<<<<<<<<<<<\n");
    ADS1282_stop();		/* Выключаем АЦП и в Powerdown его */
    TIMER3_disable();		/* Выключим 3-1 таймер   */
    log_close_data_file();	/* Закрываем файл данных */
    delay_ms(50);
    unselect_analog_power();	/* Выключить аналоговую часть */
}

/**
 * Ставим время компиляции,  номер устройства и время работы
 */
#pragma section("FLASH_code")
static void get_dev_params(void *p0, void *p1)
{
    char date[32], time[32];
    DEV_ADDR_STRUCT *par;
    DEV_WORK_TIME_STRUCT *wt;
    TIME_DATE d;

    dev_status.eeprom = read_all_data_from_eeprom();	/* получить  параметры из EEPROM иначе имени не будет */

    if (p0 != NULL) {
	par = (DEV_ADDR_STRUCT *) p0;
	sprintf(date, "%s", __DATE__);
	sprintf(time, "%s", __TIME__);

	/* Определим и поставим время компиляции  */
	par->time = get_comp_time();
	par->addr = read_mod_id_from_eeprom();	/* Получим адрес устройства */
	par->ver = get_version();	/* Версия ПО */
	par->rev = get_revision();	/* Ревизию ПО */
    }

    /* Получим время работы  */
    if (p1 != NULL) {
	wt = (DEV_WORK_TIME_STRUCT *) p1;
	wt->time_work = read_time_work_from_eeprom();
	wt->time_cmd = read_time_cmd_from_eeprom();
	wt->time_modem = read_time_modem_from_eeprom();
    }
}

/* Получить полный статус - в командном режиме */
#pragma section("FLASH_code")
void cmd_get_dsp_status(void *par)
{
    dev_status.len = sizeof(DEV_STATUS_STRUCT) - 3;	/*  Размер полного статуса */
    dev_status.st_main &= ~0x04;	/* Снимаем "ошибка" если была */
    xShooseMode.time = get_sec_ticks();
    if (par != NULL) {
	memcpy(par, &dev_status, sizeof(dev_status));
    }
}

/**
 * Выдает параметры наружу
 */
#pragma section("FLASH_code")
void get_gns110_start_params(void *par)
{
    if (par != NULL) {		/* Передает в параметр Эту структуру */
	memcpy(par, &gns110_param, sizeof(GNS110_PARAM_STRUCT));
    }
}


/**
 * Копируют всю структуру за раз
 */
#pragma section("FLASH_code")
void set_gns110_start_params(void *par)
{
    if (par != NULL) {		/* Ставит эту структуру из параметра  */
	memcpy(&gns110_param, par, sizeof(GNS110_PARAM_STRUCT));
    }
}


/** 
 * Получить uart_cmd 
 */
#pragma section("FLASH_code")
void get_uart_cmd_buf(void *par)
{
    if (par != NULL) {
	memcpy(par, &uart_cmd, sizeof(DEV_UART_CMD));	/* Получить буфер */
    }
}


/** 
 * Записать uart_cmd 
 */
#pragma section("FLASH_code")
void set_uart_cmd_buf(void *par)
{
    if (par != NULL) {
	memcpy(&uart_cmd, par, sizeof(DEV_UART_CMD));	/* Записать буфер */
    }
}

/**
 * Вернуть, в каком состоянии мы находимся 
 */
#pragma section("FLASH_code")
int get_dev_state(void)
{
    return dev_state;
}

/**
 * Выдать время компиляции,  номер устройства и время работы
 */
#pragma section("FLASH_code")
long get_comp_time(void)
{
    long t0 = 0;
    TIME_DATE d;
    char date[32], time[32];

    sprintf(date, "%s", __DATE__);
    sprintf(time, "%s", __TIME__);

    if (parse_date_time(date, time, &d) == true)
	t0 = td_to_sec(&d);

    return t0;
}


/**
 * Получаем напряжения и параметры среды с АЦП
 * В зависимости от состояния - раз в минуту или реже
 * Возможно добавить несколько секунд, чтобы записи в заголовок не пересекались из ADS1282.c!
 */
#pragma section("FLASH_code")
static void get_atmega_data(DEV_STATE_ENUM * state)
{
    static long time = 0;
    static int num = 0;
    long ticks;
    short u, i;
    int ma, mv;
    int volt;
    SD_CARD_ERROR_STRUCT ts;

    /* Опрос - каждую секунду или минуту */
    ticks = get_sec_ticks();
    if (time != ticks) {
	time = ticks;

	adc_get(&dev_status);

	/* В режиме регистрации меняем данные в заголовке раз в минуту
	 * Чтобы не попадало на запись заголовка */
	if (*state == DEV_REG_STATE && time % 60 == 30) {
	    log_change_adc_header(&dev_status);
	}


	/* При регистрации (когда в воде)- не выходить на ошибку! */
	if (!check_sd_card()) {
	    if (*state != DEV_COMMAND_MODE_STATE && *state >= DEV_INIT_STATE && *state < DEV_REG_STATE) {
		*state = DEV_ERROR_STATE;
	    } else {
		/* Выключаем АЦП и перейдем в режим "Аварийное ожидание"  */
		dev_status.st_test0 |= 0x40;
	    }
	}

	/* Моргать красной лампой в случае таймаута или ошибке карты */
	get_sd_card_timeout(&ts);
	if (ts.any_error != num) {
	    num = ts.any_error;
	    LED_toggle(LED_RED);
#if 1
	    log_write_log_file("INFO: SD Card error occured: %d\n", num);
	    log_write_log_file("INFO: cmd(%d), read(%d) write(%d)\n", ts.cmd_error, ts.read_timeout, ts.write_timeout);
#endif
	}
#if !defined	JTAG_DEBUG	/* Не проверяем при подключении из студии */
	if (*state == DEV_COMMAND_MODE_STATE && ticks - xShooseMode.time > POWEROFF_NO_LINK_TIME) {
	    POWER_off(CAUSE_NO_LINK, true);	/* Если нет связи - выключить питание и модем */
	}
#endif

	/* Печатать параметры раз в 1 минуту или раз в 10 минут или в час */
	if ((*state > DEV_INIT_STATE) && (*state <= DEV_HALT_STATE) && (time % ADC_DATA_PRINT_COEF == 0))
#if !defined	ENABLE_NEW_SIVY
	    print_adc_data(&dev_status);
////vvvvv:	    log_get_free_space();
#endif

	mv = dev_status.burn_ext_volt;
	ma = dev_status.iburn_sense;

	/* 
	 * Ставим флаг на пережиг - чтобы не писать все это в условии.  Внешний пережиг - иногда дает ошибку: ток 250 ма,
	 * и напряжение неск. милиампер. по неизвестной причине.  Не будем смотреть на ток! 
	 */
	xShooseMode.u_burn.xCtrlBurn.bExternBurn = false;
	if (mv > 6000) {
	    xShooseMode.u_burn.xCtrlBurn.bExternBurn = true;

	    /* Метка начала внешнего пережига */
	    if (false == xShooseMode.burn_time.bBurnLock) {
		xShooseMode.burn_time.bBurnLock = true;
		xShooseMode.burn_time.ExtBurnDur = get_sec_ticks();
		xShooseMode.burn_time.cBurnCnt++;

		/* если внутреннего нет */
		log_write_log_file("[-------- U burn %#5d mv appeared ---------]\n", mv);
	    }
	}

	/* Если внешний пережиг закончился - смещаем времена по первому пережигу! */
	if (true == xShooseMode.burn_time.bBurnLock && false == xShooseMode.u_burn.xCtrlBurn.bExternBurn) {
	    xShooseMode.burn_time.bBurnLock = false;
	    log_write_log_file("[-------- U burn disapeared ----------------]\n");
	    log_write_log_file("[-------- Burning was %d time(s) %#3d sec ----]\n", xShooseMode.burn_time.cBurnCnt,
			       get_sec_ticks() - xShooseMode.burn_time.ExtBurnDur);

	    /* Смещаем времена после первого пережига */
	    if (xShooseMode.burn_time.cBurnCnt == 1) {
		shift_gns_time_after_burn();
	    }
	}

	/* Проверяем раз в минуту напряжение питания */
	if (time % 60 == 0) {
	    u = filt_reg_power(dev_status.regpwr_volt);
	    xVoltArray.num++;

	    /* Если в течении 10 - минут подряд */
	    if (u < POWER_DROP_MIN && *state != DEV_COMMAND_MODE_STATE) {

		/* В командном режиме не выключать */
		if (xVoltArray.num > POWER_DROP_COUNT) {
		    log_write_log_file("WARN: average voltage %d is less %d mv during 10 min.\n", u, POWER_DROP_MIN);
		    POWER_off(CAUSE_BROWN_OUT, false);	/* Не выключаем модем!!! */
		}
	    } else {
		xVoltArray.num = 0;	/* Сбрасываем счетчик */
	    }
	}
    }
}

/**
 * Поднос магнита "Быстрая" функция - запрос на выключение прибора - вызывается из ISR!
 * Нужно перебирать число подносов магнита...!
 */
#pragma section("FLASH_code")
void send_magnet_request(void)
{
    /* принудительно выключить, если уже был запрос и GNS "всплывает"! */
    if (dev_state > DEV_CHOOSE_MODE_STATE || dev_state == DEV_ERROR_STATE) {
	xShooseMode.u_burn.xCtrlBurn.bPowerOff = true;	/* Можно выключать  */

	/* Выключать в этих режимах! */
	if (dev_state == DEV_COMMAND_MODE_STATE || dev_state == DEV_TUNE_Q19_STATE || dev_state == DEV_WAIT_GPS_STATE || dev_state == DEV_EMERGENCY_WAIT_STATE) {
	    POWER_off(CAUSE_POWER_OFF, true);
	}
    } else {
	xShooseMode.u_burn.xCtrlBurn.bInitCmd = true;	/* Переключение в рабочий режим */
    }
}

/**
 * Проверять запросы от магнита и пережига
 */
#pragma section("FLASH_code")
static void check_magnet_request(DEV_STATE_ENUM * state)
{
    long t0;

    if (xShooseMode.u_burn.xCtrlBurn.bPowerOff && !(xShooseMode.u_reg.xCtrlReg.cLock & LOCK_POWER_OFF_REQ)) {
	xShooseMode.u_reg.xCtrlReg.cLock |= LOCK_POWER_OFF_REQ;

	t0 = get_sec_ticks();

	log_write_log_file("INFO: Power off request\n");
	if (*state >= DEV_REG_STATE) {

	    gns110_param.gns110_burn_on_time = 0;	// Не пережигаем
	    gns110_param.gns110_burn_off_time = t0 + 1;
	    gns110_param.gns110_gps_time = t0 + 3;

	    /* Если регистрация идет */
	    if (xShooseMode.u_reg.xCtrlReg.bBeginReg && !xShooseMode.u_reg.xCtrlReg.bFinish) {
		gns110_param.gns110_finish_time = t0 + 2;
		log_write_log_file("INFO: We need to shift times at 2 seconds\n");
		print_set_times(&gns110_param);
	    }
	} else {
	    *state = DEV_POWER_OFF_STATE;	/* Сменили состояние */
	}
    }
}

/**
 * Сдвинуть времена регистратра, после сигнала пережига
 */
#pragma section("FLASH_code")
static void shift_gns_time_after_burn(void)
{
    int shift;
    int ticks;
    ticks = get_sec_ticks();	/* Время сейчас */
    log_write_log_file("WARN: we need to shift gns times after burn\n");
    shift = gns110_param.gns110_gps_time - gns110_param.gns110_burn_off_time;	/* Продолжтельность всплытия */

    /* Если регистрация начата */
    gns110_param.gns110_start_time = 0;
    gns110_param.gns110_finish_time = ticks + 2;	/* время окончания регистрации */
    gns110_param.gns110_gps_time = gns110_param.gns110_finish_time + shift;	/* время включения gps после времени всплытия */
    print_set_times(&gns110_param);
}

/************************************************************************
 * Пережигание проволоки с помошью релюшки
 ************************************************************************/
#pragma section("FLASH_code")
static void wait_burn_time(DEV_STATE_ENUM state)
{
    // Если есть бит внутреннео пережига - включить реле
    if ((xShooseMode.u_burn.xCtrlBurn.bInnerBurnOn) && (!(xShooseMode.mode & MODE_BURN_ON_LOCK))) {
	xShooseMode.mode |= MODE_BURN_ON_LOCK;
	log_write_log_file("[-------- burn rele ON! --------------------]\n");
	burn_wire_on();
    }
    // если наступило время выключения реле - выключить
    if ((xShooseMode.u_burn.xCtrlBurn.bInnerBurnOff) && (!(xShooseMode.mode & MODE_BURN_OFF_LOCK))) {
	xShooseMode.mode |= MODE_BURN_OFF_LOCK;	// бит выключения
	log_write_log_file("[-------- burn rele OFF! -------------------]\n");
	burn_wire_off();
    }
}

/************************************************************************
 * Функция callback для таймера 1 - просыпание
 * По будильникам ТОЛЬКО меняем состояние КА, за исключением старта
 ************************************************************************/
section("L1_code")
static void timer1_callback_func(u32 sec)
{
    if (sec == gns110_param.gns110_start_time) {
	ADS1282_start_irq();
	xShooseMode.u_reg.xCtrlReg.bStart = true;
    } else if (sec == gns110_param.gns110_finish_time) {
	dev_state = DEV_FINISH_REG_STATE;
    } else if (sec == gns110_param.gns110_burn_on_time) {
	xShooseMode.u_burn.xCtrlBurn.bInnerBurnOn = 1;
    } else if (sec == gns110_param.gns110_burn_off_time) {
	xShooseMode.u_burn.xCtrlBurn.bInnerBurnOff = 1;
    } else if (sec == gns110_param.gns110_gps_time) {
	dev_state = DEV_WAIT_GPS_STATE;
    }
}

/****************************************************************************************
 *    			FINIS CORONAT OPVS
 ****************************************************************************************/
