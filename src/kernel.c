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
#include"header/memory/paging.h"

void kernel_setup(void) {
    map_identity_vga(&_paging_kernel_page_directory);
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_ext2();
    

    // Write File
    // char file[] = "FileXXXX has been written";
    // for (int i=0;i<1;i++) {
    //     struct BlockBuffer file_buf;
    //     memset(file_buf.buf, 0, BLOCK_SIZE);
    //     memcpy(file_buf.buf, file, sizeof(file));
    //     memset(file_buf.buf+4, (char)(i/1000+48), 1);
    //     memset(file_buf.buf+5, (char)((i/100)%10+48), 1);
    //     memset(file_buf.buf+6, (char)((i/10)%10+48), 1);
    //     memset(file_buf.buf+7, (char)(i%10+48), 1);
    //     struct EXT2DriverRequest req = {
    //         .buf = file_buf.buf,
    //         .name = "File    ",
    //         .name_len = 8,
    //         .parent_inode = 2,
    //         .buffer_size = sizeof(file),
    //         .is_directory = false,
    //     };
    //     memset(req.name+4, (char)(i/1000+48), 1);
    //     memset(req.name+5, (char)((i/100)%10+48), 1);
    //     memset(req.name+6, (char)((i/10)%10+48), 1);
    //     memset(req.name+7, (char)(i%10+48), 1);
    //     int8_t result = write(&req);
    // }

    // struct EXT2Inode root;
    // read_inode(2, &root);
    

    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // Set TSS $esp pointer and jump into shell 
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (true);
}

