#ifndef _CMOS_H
#define _CMOS_H

#include <stdint.h>

uint8_t cmos_read(uint8_t reg);
void cmos_read_time(uint8_t* hour, uint8_t* minute, uint8_t* second);

#endif