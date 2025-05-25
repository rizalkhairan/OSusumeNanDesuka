#include "header/interrupt/interrupt.h"
#include "header/cpu/portio.h"
#include "header/keyboard/keyboard.h"
#include "header/filesystem/ext2.h"
#include "header/cpu/gdt.h"
#include "header/text/framebuffer.h"
#include "header/terminal/terminal.h"
#include "header/process/scheduler.h"
#include "header/cmos/cmos.h"

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
        case PIC1_OFFSET + IRQ_TIMER:
            pic_ack(IRQ_TIMER);
            struct ProcessControlBlock* current_running_pcb = process_get_current_running_pcb_pointer();
            struct Context ctx = {
                .cpu = frame.cpu,
                .eip = frame.int_stack.eip,
                .eflags = frame.int_stack.eflags,
                .page_directory_virtual_addr = current_running_pcb->context.page_directory_virtual_addr,
            };
            if (current_running_pcb->metadata.process_state == NEW) {
                ctx = current_running_pcb->context;
            }
            scheduler_save_context_to_current_running_pcb(ctx);
            scheduler_switch_to_next_process();
            break;
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
    }
}

void activate_timer_interrupt(void) {
    __asm__ volatile("cli");
    // Setup how often PIT fire
    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) (pit_timer_counter_to_fire & 0xFF));
    out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) ((pit_timer_counter_to_fire >> 8) & 0xFF));

    // Activate the interrupt
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
    __asm__ volatile("sti");
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
            framebuffer_write(20,10, 'E', 0xE, 0xE);
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
        case 16:
            memcpy((void*) frame.cpu.general.ebx, _process_list, sizeof(struct ProcessControlBlock)*PROCESS_COUNT_MAX);
            memcpy((void*) frame.cpu.general.ecx, &process_manager_state, sizeof(struct ProcessManagerState));
            break;
        case 17:
        {
            uint8_t* buffer = (uint8_t*) frame.cpu.general.ebx;
            uint8_t h, m, s;
            cmos_read_time(&h, &m, &s);
            buffer[0] = h;
            buffer[1] = m;
            buffer[2] = s;
        } 
        break;
        case 18:
        {
            uint32_t length = frame.cpu.general.ecx;
            char* original_name = frame.cpu.general.ebx;
            char copy_name[length];
            
            for(uint32_t i=0;i<length;i++){
                copy_name[i] = original_name[i];
            }

            struct EXT2DriverRequest requested_process = {
                .buf                   = (uint8_t*) 0,
                .name                  = copy_name,
                .parent_inode          = 2,
                .buffer_size           = 0x100000,
                .name_len              = length,
                .is_directory          = 0
            };
            *((int8_t*) frame.cpu.general.ecx) = process_create_user_process(requested_process);
                for(uint32_t i=0;i<PROCESS_COUNT_MAX;i++){
                    if(_process_list[i].metadata.pid==process_manager_state.latest_pid){
                        struct PCBQueueItem new_process = {.pcb = &_process_list[i]};
                        pcb_enqueue(&scheduling_queue, new_process);
                    }
                }
            }
            break;
            break;
        case 19:
            // ecx = process_destroy(ebx)
            *((bool*) frame.cpu.general.ecx) = process_destroy((uint32_t) frame.cpu.general.ebx);
            break;
    }
}

// void putchar(char a, uint8_t color){
//     // TODO
// }

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
}