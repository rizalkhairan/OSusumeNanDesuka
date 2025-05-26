#include "header/cpu/portio.h"
#include "header/cmos/cmos.h"

uint8_t cmos_read(uint8_t reg) {
    out(0x70, reg);
    return in(0x71);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void cmos_read_time(uint8_t* time) {    // Time is an array of 7 uint8_t
    // Wait until the RTC is not being updated
    while (cmos_read(0x0A) & 0x80);  // Check Update In Progress flag
    // Note: The time array is in the format [seconds, minutes, hours, weekday, day of the month, month, year, century]
    *(time) = bcd_to_bin(cmos_read(0x00));
    *(time + 1) = bcd_to_bin(cmos_read(0x02));
    *(time + 2) = bcd_to_bin(cmos_read(0x04));
    *(time + 3) = bcd_to_bin(cmos_read(0x06));  // Weekday (1-7, where 1 is Sunday)
    *(time + 4) = bcd_to_bin(cmos_read(0x07));  // Day of the month (1-31)
    *(time + 5) = bcd_to_bin(cmos_read(0x08));  // Month (1-12)
    *(time + 6) = bcd_to_bin(cmos_read(0x09));  // Year (0-99)
    *(time + 7) = bcd_to_bin(cmos_read(0x32));  // Century (19-20)
}