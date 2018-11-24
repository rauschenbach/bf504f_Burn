#ifndef _MAIN_H
#define _MAIN_H

#include "globdefs.h"

/* Статус внутренниъ часов */
#define		CLOCK_NO_TIME			(-1)
#define		CLOCK_RTC_TIME                  (0)
#define		CLOCK_NO_GPS_TIME               (1)
#define		CLOCK_PREC_TIME                 (2)



void get_gns110_start_params(void* par);
void set_gns110_start_params(void* par);
int  get_dev_state(void);
int  get_clock_status(void);
void set_clock_status(int);

void make_extern_uart_cmd(void *, void *);

void cmd_get_gns110_rtc(void *);
void cmd_set_gns110_rtc(u32, void *);
void cmd_set_dsp_status(void *);
void cmd_set_dsp_addr(u16, void *);
void cmd_get_dsp_status(void *);
void cmd_get_dsp_addr(void *);

void cmd_get_adc_data(void *);
void cmd_clear_adc_buf(void *);

void start_adc_osciloscope(void *);
void stop_adc_osciloscope(void);


void cmd_start_adc_aquis(void *, void *);
void cmd_stop_adc_aquis(void *);
void cmd_set_adc_const(void *, void *);
void cmd_get_adc_const(void *);
void cmd_get_work_time(void *);
void cmd_set_work_time(u32, u32, u32, void *);
void cmd_zero_all_eeprom(void *);
long get_comp_time(void);
void send_magnet_request(void);
u16  get_voltage_data(void);
u16  get_temperature_data(void);
void get_uart_cmd_buf(void *);
void set_uart_cmd_buf(void *);
void write_work_time(void);

#endif				/* main.h  */
