#ifndef _AM3_H
#define _AM3_H

#include "globdefs.h"


int  am3_init(void);
void am3_close(void);
int  am3_prg_modem(void*);
int  am3_set_radio(void*);
int  am3_check_modem(void*);
int  am3_set_dev_num(s16);
int  am3_set_curr_time(TIME_DATE*);
int  am3_set_alarm_time(TIME_DATE*);
int  am3_set_cal_time(u8, u8, u8, u8);
int  am3_set_popup_len(u16);
int  am3_set_file_len(u32);
int  am3_set_gps_radio(void);
int  am3_set_burn_len(u16);
int  am3_set_all_params(void);

s16  am3_get_dev_num(void);
int  am3_get_curr_time(TIME_DATE*);
int  am3_get_alarm_time(TIME_DATE*);
int  am3_get_cal_time(u8 *, u8 *, u8 *, u8 *);
u16  am3_get_popup_len(void);
u16  am3_get_burn_len(void);
int  am3_convey_buf(void *, int);

#endif /* am3.h */
