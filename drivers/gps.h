#ifndef _GPS_H
#define _GPS_H

#include "globdefs.h"

#define 	NMEA_GPRMC_STRING_SIZE 			128	/* Максимальное значение строки GPRMC > 82 символа */
#define 	NMEA_PMTK_STRING_SIZE 			64	/* Максимальное значение строки PMTK > 32 символа */


int  gps_init(void);
int  gps_change_baud(void);
void gps_close(void);
void gps_set_grmc(void);
void gps_wake_up(void);
void gps_standby(void);
int  gps_get_nmea_string(char *, int);
int  gps_get_pmtk_string(char *, int);
u8   gps_nmea_exist(void);
bool gps_check_for_reliability(void);
int  gps_get_nmea_time(void);
s64  gps_get_dif_time(void);
void gps_get_coordinates(s32*, s32 *);
int  gps_get_utc_offset(void);
void gps_set_zda(void);

#endif /* gps.h */
