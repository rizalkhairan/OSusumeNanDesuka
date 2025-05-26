#ifndef _CMOS_H
#define _CMOS_H

#include <stdint.h>

uint8_t cmos_read(uint8_t reg);
void cmos_read_time(uint8_t *time); // Time is an array of 8 unit8_t [seconds, minutes, hours, weekday, day of the month, month, year, century]

#endif