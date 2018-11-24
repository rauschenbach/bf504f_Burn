#ifndef _TESTS_H
#define _TESTS_H

#include "globdefs.h"

bool eeprom_test(void*);
bool rtc_test(void*);
bool test_bmp085(void*);
bool test_cmps(void*);
bool test_acc(void*);
bool test_gps(void*);
void test_reset(void*);
bool test_adc(void *);
bool test_dac19(void *);
bool test_dac4(void *);
void test_all(void *); 



#endif /* tests.h  */
