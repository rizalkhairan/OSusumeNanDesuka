#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -- PIC constants -- */

// PIC interrupt offset
#define PIC1_OFFSET          0x20
#define PIC2_OFFSET          0x28

// PIC ports
#define PIC1                 0x20
#define PIC2                 0xA0
#define PIC1_COMMAND         PIC1
#define PIC1_DATA            (PIC1 + 1)
#define PIC2_COMMAND         PIC2
#define PIC2_DATA            (PIC2 + 1)

// PIC ACK & mask constant
#define PIC_ACK              0x20
#define PIC_DISABLE_ALL_MASK 0xFF

// PIC remap constants
#define ICW1_ICW4            0x01   /* ICW4 (not) needed */
#define ICW1_SINGLE          0x02   /* Single (cascade) mode */
#define ICW1_INTERVAL4       0x04   /* Call address interval 4 (8) */
#define ICW1_LEVEL           0x08   /* Level triggered (edge) mode */
#define ICW1_INIT            0x10   /* Initialization - required! */

#define ICW4_8086            0x01   /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO            0x02   /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE       0x08   /* Buffered mode/slave */
#define ICW4_BUF_MASTER      0x0C   /* Buffered mode/master */
#define ICW4_SFNM            0x10   /* Special fully nested (not) */


/* -- PICs IRQ list -- */

// PIC Master
#define IRQ_TIMER        0
#define IRQ_KEYBOARD     1
#define IRQ_CASCADE      2
#define IRQ_COM2         3
#define IRQ_COM1         4
#define IRQ_LPT2         5
#define IRQ_FLOPPY_DISK  6
#define IRQ_LPT1_SPUR    7

// PIC Slave
#define IRQ_CMOS         8
#define IRQ_PERIPHERAL_1 9
#define IRQ_PERIPHERAL_2 10
#define IRQ_PERIPHERAL_3 11
#define IRQ_MOUSE        12
#define IRQ_FPU          13
#define IRQ_PRIMARY_ATA  14
#define IRQ_SECOND_ATA   15


/**
 * CPURegister, store CPU registers values.
 * 
 * @param index   CPU index register (di, si)
 * @param stack   CPU stack register (bp, sp)
 * @param general CPU general purpose register (a, b, c, d)
 * @param segment CPU extra segment register (gs, fs, es, ds)
 */
struct CPURegister {
    struct {
        uint32_t edi;
        uint32_t esi;
    } __attribute__((packed)) index;
    struct {
        uint32_t ebp;
        uint32_t esp;
    } __attribute__((packed)) stack;
    struct {
        uint32_t ebx;
        uint32_t edx;
        uint32_t ecx;
        uint32_t eax;
    } __attribute__((packed)) general;
    struct {
        uint32_t gs;
        uint32_t fs;
        uint32_t es;
        uint32_t ds;
    } __attribute__((packed)) segment;
} __attribute__((packed));

/**
 * InterruptStack, data pushed by CPU when interrupt / exception is raised.
 * Refer to Intel x86 Vol 3a: Figure 6-4 Stack usage on transfer to Interrupt.
 * 
 * Note, when returning from interrupt handler with iret, esp must be pointing to eip pushed before 
 * or in other words, CPURegister, int_number and error_code should be pop-ed from stack.
 * 
 * @param error_code Error code that pushed with the exception
 * @param eip        Instruction pointer where interrupt is raised
 * @param cs         Code segment selector where interrupt is raised
 * @param eflags     CPU eflags register when interrupt is raised
 */
struct InterruptStack {
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed));

/**
 * InterruptFrame, entirety of general CPU states exactly before interrupt.
 * When used for interrupt handler, cpu.stack is kernel state before C function called,
 * not user stack when it get called. Check InterruptStack and interprivilege interrupt for more detail.
 * 
 * @param cpu        CPU state
 * @param int_number Interrupt vector value
 * @param int_stack  Hardware-defined (x86) stack state, note: will not access interprivilege ss and esp
 */
struct InterruptFrame {
    struct CPURegister    cpu;
    uint32_t              int_number;
    struct InterruptStack int_stack;
} __attribute__((packed));

// Activate PIC mask for timer only
void activate_timer_interrupt(void);

// Activate PIC mask for keyboard only
void activate_keyboard_interrupt(void);

// I/O port wait, around 1-4 microsecond, for I/O synchronization purpose
void io_wait(void);

// Send ACK to PIC - @param irq Interrupt request number destination, note: ACKED_IRQ = irq+PIC1_OFFSET
void pic_ack(uint8_t irq);

// Shift PIC interrupt number to PIC1_OFFSET and PIC2_OFFSET (master and slave)
void pic_remap(void);

/**
 * Main interrupt handler when any interrupt / exception is raised.
 * DO NOT CALL THIS FUNCTION.
 * 
 * This function will be called first if any INT 0x00 - 0x40 is raised, 
 * and will call proper ISR for respective interrupt / exception.
 * 
 * If inter-privilege interrupt raised, SS and ESP is automatically out of main_interrupt_handler()
 * parameter. Can be checked with ((int*) info) + 4 for user $esp, 5 for user $ss
 * 
 * Again, this function is not for normal function call, all parameter will be automatically set when interrupt is called.
 * @param frame Information about CPU during interrupt is raised
 */
void main_interrupt_handler(struct InterruptFrame frame);

extern struct TSSEntry _interrupt_tss_entry;

/**
 * TSSEntry, Task State Segment. Used when jumping back to ring 0 / kernel
 */
struct TSSEntry {
    uint32_t prev_tss; // Previous TSS 
    uint32_t esp0;     // Stack pointer to load when changing to kernel mode
    uint32_t ss0;      // Stack segment to load when changing to kernel mode
    // Unused variables
    uint32_t unused_register[23];
} __attribute__((packed));

// Set kernel stack in TSS
void set_tss_kernel_current_stack(void);

// Helper for syscall 13, 14, and 15
struct EXT2CopyRequest
{
    uint32_t root_inode;
    char* source;
    char* destination;
}__attribute__((packed));

int8_t copy_cp(struct EXT2CopyRequest* copy_request);
int8_t move_mv(struct EXT2CopyRequest* copy_request);
int8_t exist_ext2(uint32_t base_inode, const char *path, uint32_t *res_inode);
void split_path(const char *path, char *parent_out, char *leaf_out);
int8_t recursive_move_dir_files(uint32_t src_parent, char* src_name, uint32_t src_dir_inode, uint32_t dest_dir_inode);
int8_t recursive_make_dir(uint32_t src_parent, char* src_name, uint32_t src_dir_inode, uint32_t dest_dir_inode);
int8_t delete_recur_dir(uint32_t parent_inode, char* cur_dir_name);

// Helper for syscall 20, 21
void play_sound(uint32_t freq);
void nosound(void);

// Helper for syscall 30

/**
 * Put a string into the framebuffer at a particular row that handles escape character
 * Print into row to endrow inclusive
 * Colors: 0x0 black, 0x1 blue, 0x2 green, 0x3 cyan, 0x4 red, 0x5 magenta, 0x6 brown,
 *      0x7 light gray, 0x8 dark gray, 0x9 light blue, 0xA light green, 0xB light cyan,
 *      0xC light red, 0xD light magenta, 0xE yellow, 0xF white
 * 
 * @param string to be printed into the framebuffer which may contains escape characters
 * @param area provides information where and how many to print the string into the framebuffer
 *  Points to three integers, the first is the start row, the second is the end row inclusive.
 *  Modulo framebuffer row amount. 0 is the first row, -1 is the last row
 *  The third integer is the number of characters in the string. If -1, print until null terminator (does not support color in this mode since i am lazy)
 * @param colors interleaved fg and bg colors of strings' characters.
 *  Of the form fg0bg0fg1bg1...fgNbgN where N is the N-th character's color in the string
 *  If null, the default color will be used for all characters
 */
void print(char *s, int32_t *area, uint8_t *colors);

#endif