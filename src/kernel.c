#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    while (true) {
        framebuffer_write(5, 0, 'H', 0x0F, 0x00);
        framebuffer_write(5, 1, 'e', 0x0F, 0x00);
        framebuffer_write(5, 2, 'l', 0x0F, 0x00);
        framebuffer_write(5, 3, 'l', 0x0F, 0x00);
        framebuffer_write(5, 4, 'o', 0x0F, 0x00);
        framebuffer_write(5, 5, ' ', 0x0F, 0x00);
        framebuffer_write(5, 6, 'W', 0x0F, 0x00);
        framebuffer_write(5, 7, 'o', 0x0F, 0x00);
        framebuffer_write(5, 8, 'r', 0x0F, 0x00);
        framebuffer_write(5, 9, 'l', 0x0F, 0x00);
        framebuffer_write(5, 10, 'd', 0x0F, 0x00);
        framebuffer_write(5, 11, '!', 0x0F, 0x00);
    }
}
