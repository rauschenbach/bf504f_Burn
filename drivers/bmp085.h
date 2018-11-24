#ifndef _BMP085_H
#define _BMP085_H

#include "globdefs.h"

bool bmp085_init(void);
bool bmp085_data_get(int*, int*);

#endif /* bmp085.h  */
