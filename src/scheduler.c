#include "header/process/scheduler.h"

void scheduler_init(void) {
    // Set timer for preemptive scheduling
    activate_timer_interrupt();


    // Initialize queue for scheduling
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock* current_running_pcb = process_get_current_running_pcb_pointer();
    if (current_running_pcb) {
        current_running_pcb->context = ctx;
    }
}

void scheduler_switch_to_next_process(void) {
    // Pop from queue

    pic_ack(IRQ_TIMER);
}