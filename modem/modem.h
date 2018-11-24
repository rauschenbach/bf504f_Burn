#ifndef _MODEM_H
#define _MODEM_H

#include "globdefs.h"

int  modem_init_all_types(void*);
int  modem_set_radio(void*);
int  modem_check_modem_time(void*);
int  modem_check_params(void *);
int  modem_convey_buf(void *, int);

#endif /* modem.h */
