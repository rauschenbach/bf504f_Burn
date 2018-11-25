#ifndef _IRQ_H
#define _IRQ_H

#include "globdefs.h"

/* Номера векторов */
#define		TIM1_TIM3_VECTOR_NUM			(7)
#define		ADS1282_VECTOR_NUM		(8)
#define		NMEA_VECTOR_NUM			(9)
#define		DEBUG_COM_VECTOR_NUM			(10)
#define		TIM2_VECTOR_NUM			(11)     
#define		TIM4_VECTOR_NUM			(12)
#define		POW_TIM5_VECTOR_NUM		(13)




bool IRQ_register_vector(int);
bool IRQ_unregister_vector(int);

#endif /* irq.h */
