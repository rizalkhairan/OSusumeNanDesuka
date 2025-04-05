#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

// void kernel_setup(void) {
//     load_gdt(&_gdt_gdtr);
//     while (true) {
//         framebuffer_write(5, 0, 'H', 0x0F, 0x00);
//         framebuffer_write(5, 1, 'e', 0x0F, 0x00);
//         framebuffer_write(5, 2, 'l', 0x0F, 0x00);
//         framebuffer_write(5, 3, 'l', 0x0F, 0x00);
//         framebuffer_write(5, 4, 'o', 0x0F, 0x00);
//         framebuffer_write(5, 5, ' ', 0x0F, 0x00);
//         framebuffer_write(5, 6, 'W', 0x0F, 0x00);
//         framebuffer_write(5, 7, 'o', 0x0F, 0x00);
//         framebuffer_write(5, 8, 'r', 0x0F, 0x00);
//         framebuffer_write(5, 9, 'l', 0x0F, 0x00);
//         framebuffer_write(5, 10, 'd', 0x0F, 0x00);
//         framebuffer_write(5, 11, '!', 0x0F, 0x00);

//         framebuffer_clear();
//     }
// }

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    
    uint8_t row = 0, col = 0;
    keyboard_state_activate();
    
    while (true) {
        char c;
        get_keyboard_buffer(&c);
        
        if (c) {
            // Handle arrow keys (scancode untuk QEMU)
            if (c == 0x48) { // Up arrow
                if (row > 0) row--;
            } 
            else if (c == 0x50) { // Down arrow
                if (row < FRAMEBUFFER_COL_LENGTH - 1) row++;
            }
            else if (c == 0x4B) { // Left arrow
                if (col > 0) col--;
            }
            else if (c == 0x4D) { // Right arrow
                if (col < FRAMEBUFFER_ROW_LENGTH - 1) col++;
            }
            else {
                // Tulis karakter biasa
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