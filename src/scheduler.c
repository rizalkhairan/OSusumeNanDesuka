#include "header/process/scheduler.h"

// Scheduling PCB Queue
void pcb_create_queue(struct PCBQueue* queue) {
    queue->head = 0;
    queue->tail = 0;
}
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
    *pcbi = queue->items[queue->head];
    queue->head = (queue->head + 1) % PROCESS_QUEUE_SIZE;
    return true;
}

struct PCBQueue scheduling_queue;

void scheduler_init(void) {
    // Set timer for preemptive scheduling
    activate_timer_interrupt();
    pcb_create_queue(&scheduling_queue);

    struct PCBQueueItem init_process;
    for (uint8_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (_process_list[i].metadata.process_state == READY) {
            init_process.pcb = &_process_list[i];
            break;
        }
    }
    pcb_enqueue(&scheduling_queue, init_process);
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
    struct ProcessControlBlock* current_running_pcb = process_get_current_running_pcb_pointer();
    if (current_running_pcb) {
        current_running_pcb->context = ctx;
    }
    if (!current_running_pcb->metadata.process_state == TERMINATED) {
        current_running_pcb->metadata.process_state = READY;
        struct PCBQueueItem current_running_pcb_item = {.pcb = current_running_pcb};
        pcb_enqueue(&scheduling_queue, current_running_pcb_item);
    }
}

void scheduler_switch_to_next_process(void) {
    struct PCBQueueItem next_pcb;
    pcb_dequeue(&scheduling_queue, &next_pcb);
    next_pcb.pcb->metadata.process_state = RUNNING;
    // paging_use_page_directory(next_pcb.pcb->context.page_directory_virtual_addr);
    process_context_switch(next_pcb.pcb->context);
}
    
    