#ifndef _UTILS_H
#define _UTILS_H

#include "globdefs.h"
#include "timer0.h"
#include "lsm303.h"


long   td_to_sec(TIME_DATE *);
int    sec_to_td(long, TIME_DATE*);
void   sec_to_str(u32, char *);
void   nsec_to_str(u64, char *);

int    get_cpu_endian(void);
void   get_cpu_id(u16 *);
s64    get_long_time(void);
s64    get_usec_ticks(void);
s64    get_msec_ticks(void);
long   get_sec_ticks(void);
long   get_min_ticks(void);

void   set_sec_ticks(long);

void   td_to_str(TIME_DATE *, char *);
void   str_to_cap(char*, int);
bool   parse_date_time(char *, char *, TIME_DATE *);
bool   make_ads1282_data(void *, u64, u16);
void   get_sd_card_timeout(SD_CARD_ERROR_STRUCT*);

u32    get_buf_sum(u32*, int);
u16    get_dac_ctrl_value(u32, u16, u16);
u32    get_buf_average(u32*, int);
u16    get_hpf_from_freq(f32 freq, int dr);
bool   calc_angles(void *, void *);
//bool calc_angles(lsm303_data *acc, lsm303_data *mag);
f32 inv_sqrt(f32);

void   print_adc_data(void*);
void   print_set_times(void *);
void   print_ads1282_parms(void*);
void   print_modem_type(void*);

int    check_set_times(void *); 
int    check_modem_times(void *);
int    check_start_time(void *);
void   print_status(void *);
void   print_drift_and_work_time(s64, s64, s32, u32);
void   print_coordinates(s32, s32);
void   print_timer_and_sd_card_error(void);
void   print_reset_cause(u32);

// первый байт - команда  
// вернуть num число из буфера
// вернуть num число из буфера
u16 get_char_from_buf(void*, int);
u16 get_short_from_buf(void*, int);
u32 get_long_from_buf(void*, int);
f32 get_float_from_buf(void*, int);



#endif				/* utils.h */

