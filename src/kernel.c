#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"
#include "header/interrupt/idt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"
#include "header/terminal/terminal.h"
#include "header/filesystem/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include "header/memory/paging.h"

void kernel_setup(void) {
    map_identity_vga(&_paging_kernel_page_directory);
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();
    gdt_install_tss();
    uint32_t a = (uint32_t)&_interrupt_tss_entry;
    set_tss_register();

    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // Write shell into memory
    struct EXT2DriverRequest request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "shell",
        .parent_inode                 = 2,
        .buffer_size           = 0x100000,
        .name_len              = 5,
    };
    read(request);

    // Set TSS $esp pointer and jump into shell 
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (true);
}