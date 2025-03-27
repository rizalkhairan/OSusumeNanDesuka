#include "header/interrupt/interrupt.h"

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
    // TODO: recheck implementation
    // First, determine if this is an exception (0-31) or hardware interrupt (32-47)
    if (frame.int_number < PIC1_OFFSET) {
        // Handle CPU exceptions
        switch (frame.int_number) {
            case 0:  // Divide Error
                panic("Divide by zero at EIP=%x", frame.int_stack.eip);
                break;
            case 6:  // Invalid Opcode
                panic("Invalid opcode at EIP=%x", frame.int_stack.eip);
                break;
            case 8:  // Double Fault
                panic("Double fault! System halted. Error code: %x", frame.int_stack.error_code);
                break;
            case 13: // General Protection Fault
                panic("GPF at EIP=%x, error code: %x", frame.int_stack.eip, frame.int_stack.error_code);
                break;
            case 14: // Page Fault
                handle_page_fault(frame.int_stack.error_code, frame.int_stack.eip);
                break;
            default:
                // For unhandled exceptions, just panic with the interrupt number
                panic("Unhandled exception %d at EIP=%x", frame.int_number, frame.int_stack.eip);
        }
    } 
    else if (frame.int_number >= PIC1_OFFSET && frame.int_number < PIC2_OFFSET + 8) {
        // Handle hardware interrupts from PIC1 and PIC2
        uint8_t irq = frame.int_number - PIC1_OFFSET;
        
        switch (irq) {
            case IRQ_TIMER:
                timer_handler();
                break;
            case IRQ_KEYBOARD:
                keyboard_handler();
                break;
            // Add other IRQ handlers as needed
            default:
                log("Unhandled IRQ %d", irq);
        }
        
        // Acknowledge the interrupt to the PIC
        pic_ack(irq);
    }
    else {
        // Handle other interrupts (e.g., software interrupts)
        switch (frame.int_number) {
            case 0x80: // System call
                handle_syscall(frame.cpu.general.eax, 
                              &frame.cpu.general.ebx,
                              &frame.cpu.general.ecx,
                              &frame.cpu.general.edx);
                break;
            default:
                log("Unknown interrupt %d at EIP=%x", 
                    frame.int_number, frame.int_stack.eip);
        }
    }
}