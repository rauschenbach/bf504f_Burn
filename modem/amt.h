#ifndef _AMT_H
#define _AMT_H

#include "globdefs.h"

int  amt_prg_modem(void *);
int  amt_init(void);
void amt_close(void);
int  amt_prg_modem(void *);
int  amt_check_modem(void);

int amt_reset_modem(void);
int amt_set_radio(void*);
int amt_set_time_date(s32);
int amt_set_modem_params(void *);

#endif /* amt.h */
