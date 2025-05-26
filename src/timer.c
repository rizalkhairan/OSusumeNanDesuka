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

int main() {
    char bar[FRAMEBUFFER_ROW_LENGTH];
    uint8_t colors[FRAMEBUFFER_ROW_LENGTH * 2];
    int32_t area[3] = {-1,-1, FRAMEBUFFER_ROW_LENGTH};  // Start row, end row, length
    for (uint8_t i = 0; i < FRAMEBUFFER_ROW_LENGTH; i++) {
        bar[i] = ' ';
        colors[i * 2] = 0x0F;  // White foreground
        colors[i * 2 + 1] = 0x08;  // Dark gray background
    }
    
    while (true){
        // syscall2(5, 0, 0, 0);
        uint8_t time[3];  // [hours, minutes, seconds]
        syscall(17, (uint32_t)time, 0, 0);  // SYSCALL_CMOS_READ_TIME
        
        // Format HH:MM:SS into buffer
        uint8_t bar_offset = FRAMEBUFFER_ROW_LENGTH - 8;
        bar[bar_offset] = '0' + (time[0] / 10)%10;
        bar[bar_offset+1] = '0' + time[0] % 10;
        bar[bar_offset+2] = ':';
        bar[bar_offset+3] = '0' + time[1] / 10;
        bar[bar_offset+4] = '0' + time[1] % 10;
        bar[bar_offset+5] = ':';
        bar[bar_offset+6] = '0' + time[2] / 10;
        bar[bar_offset+7] = '0' + time[2] % 10;

        syscall(30, (uint32_t)bar, (uint32_t)area, (uint32_t)colors);  // SYSCALL_WRITE_TEXT

        // Wait for second to change
        uint8_t last_second = time[2];
        do {
            syscall(17, (uint32_t)time, 0, 0);
        } while (time[2] == last_second);
    }

    return 0;
}