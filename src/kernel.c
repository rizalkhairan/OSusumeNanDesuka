#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"
#include "header/interrupt/idt.h"
#include "header/kernel-entrypoint.h"

// Kernel setup untuk tes interrupt
void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    // framebuffer_clear();
    // framebuffer_set_cursor(0, 0);
    __asm__("int $0x4");
    while (true);
}