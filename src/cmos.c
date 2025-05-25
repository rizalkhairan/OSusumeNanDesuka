#include "header/cpu/portio.h"
#include "header/cmos/cmos.h"

uint8_t cmos_read(uint8_t reg) {
    out(0x70, reg);
    return in(0x71);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void cmos_read_time(uint8_t* hour, uint8_t* minute, uint8_t* second) {
    // Wait until the RTC is not being updated
    while (cmos_read(0x0A) & 0x80);  // Check Update In Progress flag
    *second = bcd_to_bin(cmos_read(0x00));
    *minute = bcd_to_bin(cmos_read(0x02));
    *hour   = bcd_to_bin(cmos_read(0x04));
}