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

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    terminal_initialize();
    keyboard_state_activate();

    initialize_filesystem_ext2();

    // struct BlockBuffer b;
    // for (int i = 0; i < 512; i++) b.buf[i] = i % 16;
    // write_blocks(&b, 17, 1);
    // while (true);

    while (true) {
        framebuffer_write(0, 0, (char) GROUPS_COUNT + '0', 0x07, 0x00);
        framebuffer_write(1, 0, (char) INODES_PER_GROUP + '0', 0x07, 0x00);
        framebuffer_write(2, 0, (char) BLOCKS_PER_GROUP + '0', 0x07, 0x00);
        framebuffer_write(3, 0, (char) bgdt.table[7].bg_used_dirs_count + '0', 0x07, 0x00);
    }
}