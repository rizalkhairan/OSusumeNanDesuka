#include "header/process/scheduler.h"

// Scheduling PCB Queue
bool pcb_enqueue(struct PCBQueue* queue, struct PCBQueueItem pcbi) {
    if ((queue->tail + 1) % PROCESS_QUEUE_SIZE == queue->head) {
        return false; // Queue is full (which it shouldn't be)
    }
    queue->tail = (queue->tail + 1) % PROCESS_QUEUE_SIZE;
    queue->items[queue->tail] = pcbi;
    return true;
}
bool pcb_dequeue(struct PCBQueue* queue, struct PCBQueueItem* pcbi) {
    if (queue->head == queue->tail) {
        return false; // Queue is empty (which it shouldn't be)
    }
    queue->head = (queue->head + 1) % PROCESS_QUEUE_SIZE;
    *pcbi = queue->items[queue->head];
    return true;
}

struct PCBQueue scheduling_queue = {
    .head = 0,
    .tail = 0,
};

void scheduler_init(void) {
    // Set timer for preemptive scheduling
    struct PCBQueueItem init_process;
    for (uint8_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.process_state == NEW) {
            // _process_list[i].metadata.process_state = RUNNING;
            init_process.pcb = &_process_list[i];
            break;
        }
    }
    pcb_enqueue(&scheduling_queue, init_process);
    activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock* current_running_pcb = process_get_current_running_pcb_pointer();
    if (current_running_pcb) {
        current_running_pcb->context = ctx;

        if (current_running_pcb->metadata.process_state != TERMINATED) {
            current_running_pcb->metadata.process_state = READY;
            struct PCBQueueItem current_running_pcb_item = {.pcb = current_running_pcb};
            pcb_enqueue(&scheduling_queue, current_running_pcb_item);
        } else {
            uint32_t i = current_running_pcb->metadata.pid;
            // Free all allocated pages
            for (uint32_t j = 0; j < _process_list[i].memory.page_frame_used_count; j++) {
                if (_process_list[i].memory.virtual_addr_used[j]) {
                    paging_free_user_page_frame(
                        _process_list[i].context.page_directory_virtual_addr,
                        _process_list[i].memory.virtual_addr_used[j]
                    );
                }
            }

            // Free page directory
            paging_free_page_directory(_process_list[i].context.page_directory_virtual_addr);
        }
    }
}

void scheduler_switch_to_next_process(void) {
    struct PCBQueueItem next_pcb;
    
    bool retVal = pcb_dequeue(&scheduling_queue, &next_pcb);
    if (!retVal){
        for(uint32_t i=0;i<PROCESS_COUNT_MAX;i++){
            if(_process_list[i].metadata.pid==process_manager_state.latest_pid){
                next_pcb.pcb = &_process_list[i];
                break;
            }
        }
    }
    
    next_pcb.pcb->metadata.process_state = RUNNING;

    // _interrupt_tss_entry.esp0 = next_pcb.pcb->kernel_esp;
    paging_use_page_directory(next_pcb.pcb->context.page_directory_virtual_addr);

    // pic_ack(IRQ_TIMER);
    process_context_switch(next_pcb.pcb->context);
}
    
    