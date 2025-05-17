#include <stdint.h>
#include "header/filesystem/ext2.h"

#define BLOCK_COUNT 16

uint32_t cwd_inode = 2;
char cwd_name[255];
uint16_t cwd_name_len;

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

int main(void) {
    cwd_name[0] = '.';
    cwd_name_len = 1;

    syscall(7, 0, 0, 0);
    while(true){
        char c;
        syscall(4, &c, 0, 0);
        if(c){
            syscall(6, (uint32_t)&c, 1, 0xF);
        }
    }

    return 0;
}
