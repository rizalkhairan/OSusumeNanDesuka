#include <stdint.h>
#include <stdbool.h>
#include "header/text/framebuffer.h"

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

#define TIMEZONE 7

char day_string[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

int main() {
    char bar[FRAMEBUFFER_ROW_LENGTH];
    uint8_t colors[FRAMEBUFFER_ROW_LENGTH * 2];
    int32_t area[3] = {-1,-1, FRAMEBUFFER_ROW_LENGTH};  // Start row, end row, length
    for (uint8_t i = 0; i < FRAMEBUFFER_ROW_LENGTH; i++) {
        bar[i] = ' ';
        colors[i * 2] = 0x0F;  // White foreground
        colors[i * 2 + 1] = 0x02;  // Green background
    }
    uint8_t date_offset = FRAMEBUFFER_ROW_LENGTH - 11;
    uint8_t day_offset = date_offset - 4;
    uint8_t clock_offset = day_offset - 8;
    uint8_t timezone_offset = clock_offset - 7;

    memcpy(bar + timezone_offset, "UTC", 3);
    bar[timezone_offset + 3] = '+';
    if (TIMEZONE < 0) bar[timezone_offset + 3] = '-';
    bar[timezone_offset + 4] = '0' + (TIMEZONE / 10) % 10;
    bar[timezone_offset + 5] = '0' + TIMEZONE % 10;
    
    while (true){
        // syscall2(5, 0, 0, 0);
        uint8_t time[8];  // [seconds, minutes, hours, weekday, day of the month, month, year, century]
        syscall(17, (uint32_t)time, 0, 0);  // SYSCALL_CMOS_READ_TIME

        time[2] = (time[2] + TIMEZONE) % 24;  // Adjust hours for timezone
        
        // Format HH:MM:SS into buffer
        bar[clock_offset] = '0' + (time[2] / 10)%10;
        bar[clock_offset+1] = '0' + time[2] % 10;
        bar[clock_offset+2] = ':';
        bar[clock_offset+3] = '0' + time[1] / 10;
        bar[clock_offset+4] = '0' + time[1] % 10;
        bar[clock_offset+5] = ':';
        bar[clock_offset+6] = '0' + time[0] / 10;
        bar[clock_offset+7] = '0' + time[0] % 10;

        // Format Day of the week into buffer
        memcpy(bar + day_offset + 1, day_string[(time[3]-1)], 3);

        // Format YYYY/MM/DD into buffer
        if (time[4] != 0) { // Century register
            bar[date_offset+1] = '0' + (time[7] / 10) % 10;
            bar[date_offset+2] = '0' + time[7] % 10;
        }
        bar[date_offset+3] = '0' + (time[6] / 10) % 10;
        bar[date_offset+4] = '0' + time[6] % 10;
        bar[date_offset+5] = '/';
        bar[date_offset+6] = '0' + (time[5] / 10) % 10;
        bar[date_offset+7] = '0' + time[5] % 10;
        bar[date_offset+8] = '/';
        bar[date_offset+9] = '0' + (time[4] / 10) % 10;  // Century
        bar[date_offset+10] = '0' + time[4] % 10;  // Year

        syscall(30, (uint32_t)bar, (uint32_t)area, (uint32_t)colors);  // SYSCALL_WRITE_TEXT

        // Wait for second to change
        uint8_t last_second = time[0];
        do {
            syscall(17, (uint32_t)time, 0, 0);
        } while (time[0] == last_second);
    }

    return 0;
}