#include <stdint.h>
#include "header/interrupt/interrupt.h"
#include "header/terminal/terminal.h"
#include "header/text/framebuffer.h"

Path absolute_path[8] = {0};
uint8_t depth = 0;
char fullpath[2040];
uint32_t fullpath_length = 1;

uint32_t cwd_inode = 2;
char cwd_name[255];
uint16_t cwd_name_len;
uint32_t filepath_len = 22;
static InputBuffer terminal_buffer;

void get_absolute_path(){
    uint32_t offset = 0;
    for(uint8_t i=0;i<=depth;i++){
        if(i>0){
            memcpy((char*)((uint8_t*)fullpath + offset), "/", 1);
            offset++;
        }
        memcpy((char*)((uint8_t*)fullpath + offset), absolute_path[i].name, absolute_path[i].length);
        offset += absolute_path[i].length;
    }
}

uint32_t get_absolute_path_length(){
    uint32_t res = 0;
    for(uint8_t i=0;i<=depth;i++){
        if(i>0){
            res++; // hitung '/'
        }
        res+= absolute_path[i].length;
    }
    return res;
}


void terminal_initialize(){
    syscall2(8, 0, 0, 0);
    
    // Set cursor correct position
    terminal_buffer.hist_length = 0;
    terminal_buffer.current_line = 0;
    terminal_buffer.viewed_line = 0;
    terminal_buffer.current_line_row = 0;
    terminal_buffer.current_line_col = 0;
    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
    InputLine* line = &terminal_buffer.history[0];
    line->length = 0;
    
    syscall2(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);
    syscall2(10, (uint32_t)&terminal_buffer, 0, 0);
}

void syscall2(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
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
        // syscall2(5, 0, 0, 0);
        syscall2(17, (uint32_t)time, 0, 0);  // SYSCALL_CMOS_READ_TIME

        // Format HH:MM:SS into buffer
        char buffer[9] = {0};
        buffer[0] = '0' + time[0] / 10;
        buffer[1] = '0' + time[0] % 10;
        buffer[2] = ':';
        buffer[3] = '0' + time[1] / 10;
        buffer[4] = '0' + time[1] % 10;
        buffer[5] = ':';
        buffer[6] = '0' + time[2] / 10;
        buffer[7] = '0' + time[2] % 10;
        buffer[8] = '\0';

        terminal_buffer.current_line_col = 80-8;
        terminal_buffer.current_line_row = 24; // TODO: VALIDASI TEMBUS LAYAR
        syscall2(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall2(6, (uint32_t)buffer, 8, 0x0F);  // SYSCALL_WRITE_TEXT

        // Wait for second to change
        uint8_t last_second = time[2];
        do {
            syscall2(17, (uint32_t)time, 0, 0);
        } while (time[2] == last_second);
    }

    return 0;
}