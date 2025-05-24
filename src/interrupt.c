#include "header/interrupt/interrupt.h"
#include "header/cpu/portio.h"
#include "header/keyboard/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/cpu/gdt.h"
#include "header/text/framebuffer.h"
#include "header/terminal/terminal.h"

static InputBuffer terminal_buffer;

void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); 
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case 0xD: // general protection fault
            __asm__("hlt");
            break;
        case 0xE: // page fault
            __asm__("hlt");
            break;
        case 0x30: // syscall interrupt
            syscall(frame);
            break;
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
    }
}

void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

// Definisi variabel global TSS (hanya satu instance untuk seluruh sistem)
struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8; 
}

void syscall(struct InterruptFrame frame) {
    switch (frame.cpu.general.eax) {
        case 0:
            // read file
            *((int8_t*) frame.cpu.general.ecx) = read(*((struct EXT2DriverRequest*) frame.cpu.general.ebx));
            break;
        case 1:
            // read directory
            *((int8_t*) frame.cpu.general.ecx) = read_directory(((struct EXT2DriverRequest*) frame.cpu.general.ebx));
            break;
        case 2:
            // write
            *((int8_t*) frame.cpu.general.ecx) = write(((struct EXT2DriverRequest*) frame.cpu.general.ebx));
            break;
        case 3:
            // delete
            *((int8_t*) frame.cpu.general.ecx) = delete(*((struct EXT2DriverRequest*) frame.cpu.general.ebx));
            break;
        case 4:
            get_keyboard_buffer((char*) frame.cpu.general.ebx);
            break;
        case 5:
            // text output via putchar()
            break;
        case 6:
            // text output via puts()
            puts(
                (char*) frame.cpu.general.ebx, 
                frame.cpu.general.ecx, 
                frame.cpu.general.edx
            );
            break;
        case 7:
            keyboard_state_activate();
            break;
        case 8:
            framebuffer_clear();
            break;
        case 9:
            framebuffer_set_cursor((uint8_t) frame.cpu.general.ebx, (uint8_t) frame.cpu.general.ecx);
            break;
        case 10:
            InputBuffer* src = (InputBuffer*) frame.cpu.general.ebx;
            terminal_buffer = *src;
            break;
        case 11:
            struct EXT2Inode new_inode_1;
            read_inode(
                ((uint32_t) frame.cpu.general.ebx), 
                &new_inode_1
            );
            *((int32_t*) frame.cpu.general.ecx) = new_inode_1.i_size;
            break;
        case 12:
            // find directory entry
            struct EXT2Inode new_inode;
            read_inode(
                ((struct EXT2DriverRequest*) frame.cpu.general.ebx)->parent_inode,
                &new_inode
            );
            *((int8_t*) frame.cpu.general.edx) = find_directory_entry(
                (struct EXT2DriverRequest*) frame.cpu.general.ebx, 
                &new_inode,
                (struct EXT2DirectoryEntry*) frame.cpu.general.ecx
            );
            // char* a = get_entry_name((struct EXT2DirectoryEntry*) frame.cpu.general.ecx);
            break;
        case 13:
            existEXT2Arg *param = (struct existEXT2Arg*) frame.cpu.general.ebx;
            *((int8_t*) frame.cpu.general.ecx) = exist_ext2(param->base_inode, param->path, param->res_inode);
            break;
        case 14:
            *((int8_t*) frame.cpu.general.ecx) = copy_cp((struct EXT2CopyRequest*) frame.cpu.general.ebx);
            break;
        case 15:
            *((int8_t*) frame.cpu.general.ecx) = move_mv((struct EXT2CopyRequest*) frame.cpu.general.ebx);
            break;
    }
}

void putchar(char a, uint8_t color){
    // TODO
}

void puts(char* buf, uint32_t count, uint8_t color) {
    int r = terminal_buffer.current_line_row;
    int c = terminal_buffer.current_line_col;

    for (uint32_t i = 0; i < count; i++) {
        framebuffer_write(r, c, buf[i], color, 0x00);
        if (++c >= FRAMEBUFFER_ROW_LENGTH) {
            c = 0;
            ++r;
        }
    }
    
    for (int i = count; i < MAX_LINE_LENGTH; ++i) {
        framebuffer_write(r, c, ' ', 0xF, 0x0);
        if (++c >= FRAMEBUFFER_ROW_LENGTH) {
            c = 0;
            ++r;
        }
    }
    InputLine* line = &terminal_buffer.history[terminal_buffer.current_line];
    int linelen = line->length;
    framebuffer_write(20, 0, count + 65, 0x2, 0x0);
    framebuffer_write(21, 0, line->length + '0', 0x2, 0x0);
}