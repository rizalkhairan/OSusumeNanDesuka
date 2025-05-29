#include <stdint.h>
#include "header/interrupt/interrupt.h"
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
    while (true){
        uint8_t time[3];  // [hours, minutes, seconds]
        syscall(17, (uint32_t)time, 0, 0);  // SYSCALL_CMOS_READ_TIME

        // Format HH:MM:SS into buffer
        uint16_t offset = 24 * 80 + 70;
        syscall(5, '0' + time[0] / 10, offset, (0xFF << 8) | 0x0);
        syscall(5, '0' + time[0] % 10, offset + 1, (0xFF << 8) | 0x0);
        syscall(5, ':', offset + 2, (0xFF << 8) | 0x0);
        syscall(5, '0' + time[1] / 10, offset + 3, (0xFF << 8) | 0x0);
        syscall(5, '0' + time[1] % 10, offset + 4, (0xFF << 8) | 0x0);
        syscall(5, ':', offset + 5, (0xFF << 8) | 0x0);
        syscall(5, '0' + time[2] / 10, offset + 6, (0xFF << 8) | 0x0);
        syscall(5, '0' + time[2] % 10, offset + 7, (0xFF << 8) | 0x0);

        // Wait for second to change
        uint8_t last_second = time[2];
        do {
            syscall(17, (uint32_t)time, 0, 0);
        } while (time[2] == last_second);
    }

    return 0;
}