#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"
#include "header/interrupt/idt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
   
    int row = 0, col = 0;
    keyboard_state_activate();
    while (true) {
        char c;
        get_keyboard_buffer(&c);
        if (c) {
            // Tulis karakter biasa
            if(c == '\n'){
                row++;
                col = 0;
            } else if(c == '\b'){
                if(col>0){
                    col--;
                    framebuffer_write(row, col, ' ', 0xF, 0x0);
                }
            }
            else{
                framebuffer_write(row, col, c, 0xF, 0x0);
                if (++col >= FRAMEBUFFER_ROW_LENGTH) {
                    col = 0;
                    if (++row >= FRAMEBUFFER_COL_LENGTH) row = 0;
                }
            }
            framebuffer_set_cursor(row, col);
        }
    }
}
