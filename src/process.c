// process.c
#include "header/process/process.h"
#include "header/process/scheduler.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

// Kernel stack for each process
// static uint8_t g_kernel_memory_pool[PROCESS_COUNT_MAX][KERNEL_STACK_SIZE];
// static bool g_kernel_memory_pool_used[PROCESS_COUNT_MAX] = {false};

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];
struct ProcessManagerState process_manager_state = {
    .active_process_count = 0,
    .latest_pid = 0,
    .process_used = {false}
};

uint32_t ceil_div(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

uint32_t process_generate_new_pid() {
    for(uint32_t i=0;i<PROCESS_COUNT_MAX;i++){
        if(process_manager_state.process_used[i]==false){
            return i;
        }
    }
    return -1;
}

uint32_t process_list_get_inactive_index() {
    for (int i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (!process_manager_state.process_used[i]) {
            return i;
        }
    }
    return -1;
}

struct ProcessControlBlock* process_get_current_running_pcb_pointer() {
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        struct ProcessControlBlock debug = _process_list[i];
        bool debug2 = process_manager_state.process_used[i];
        if (process_manager_state.process_used[i] && 
            (_process_list[i].metadata.process_state == RUNNING || _process_list[i].metadata.process_state == NEW)) {
            return &_process_list[i];
        }
    }
    return NULL;
}

int32_t process_create_user_process(struct EXT2DriverRequest request) {
    int32_t retcode = PROCESS_CREATE_SUCCESS;
    
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    if ((uint32_t)request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE) {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }

    uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
    if (!paging_allocate_check(page_frame_count_needed) || 
        page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    int32_t p_index = process_list_get_inactive_index();
    if (p_index == -1) {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    struct ProcessControlBlock* new_pcb = &_process_list[p_index];
    memset(new_pcb, 0, sizeof(struct ProcessControlBlock));

    // Copy process name (ensure null termination)
    memcpy(new_pcb->metadata.name, request.name, PROCESS_NAME_LENGTH_MAX - 1);
    if (request.name_len >= PROCESS_NAME_LENGTH_MAX) {
        new_pcb->metadata.length = PROCESS_NAME_LENGTH_MAX - 1;
    } else {
        new_pcb->metadata.length = request.name_len;
    }
    new_pcb->metadata.name[PROCESS_NAME_LENGTH_MAX - 1] = '\0';

    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.process_state = NEW;

    struct PageDirectory* current_pd = paging_get_current_page_directory_addr();
    struct PageDirectory* new_pd = paging_create_new_page_directory();
    if (!new_pd) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }
    // Allocate memory for program
    if (!paging_allocate_user_page_frame(new_pd, request.buf)) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }
    new_pcb->memory.virtual_addr_used[0] = request.buf;

    // Allocate stack
    if (!paging_allocate_user_page_frame(new_pd, (void*)0xBFFFFFFC)) {
        paging_free_user_page_frame(new_pd, new_pcb->memory.virtual_addr_used[0]);
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }
    new_pcb->memory.virtual_addr_used[1] = (void*)0xBFFFFFFC;

    new_pcb->memory.page_frame_used_count = 2;
    new_pcb->context.page_directory_virtual_addr = new_pd;
    // Kernel stack allocation
    // int32_t kernel_stack_index = -1;
    // for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
    //     if (!g_kernel_memory_pool_used[i]) {
    //         kernel_stack_index = i;
    //         break;
    //     }
    // }
    // if (kernel_stack_index == -1) {
    //     paging_free_user_page_frame(new_pd, new_pcb->memory.virtual_addr_used[0]);
    //     paging_free_user_page_frame(new_pd, new_pcb->memory.virtual_addr_used[1]);
    //     retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
    //     goto exit_cleanup;
    // }
    // new_pcb->kernel_stack_base = &g_kernel_memory_pool[kernel_stack_index][0];
    // new_pcb->kernel_esp = (uint32_t)new_pcb->kernel_stack_base + KERNEL_STACK_SIZE;
    // new_pcb->kernel_stack_pool_index = kernel_stack_index;
    // g_kernel_memory_pool_used[kernel_stack_index] = true;

    // Switch to new page directory to load the program
    paging_use_page_directory(new_pd);

    // Load the executable
    int8_t retval_read = read(request);
    if (retval_read!=0){
        paging_use_page_directory(current_pd);
        retcode = PROCESS_CREATE_FAIL_FS_READ_FAILURE;
        goto exit_cleanup;
    }

    paging_use_page_directory(current_pd);

    // Setup context
    new_pcb->context.cpu.segment.ds = 0x23; // User data segment with PL 3
    new_pcb->context.cpu.segment.es = 0x23;
    new_pcb->context.cpu.segment.fs = 0x23;
    new_pcb->context.cpu.segment.gs = 0x23;
    
    new_pcb->context.eip = (uint32_t)request.buf;//  <----------------------------------
    new_pcb->context.cpu.stack.ebp = 0xBFFFFFFC;
    new_pcb->context.cpu.stack.esp = 0xBFFFFFFC;
    
    // new_pcb->context.cs = 0x1B; // User code segment with PL 3
    // new_pcb->context.esp = 0xBFFFFFFC;
    // new_pcb->context.ss = 0x23;
    
    new_pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;

    // Mark process as used
    process_manager_state.process_used[p_index] = true;
    process_manager_state.active_process_count++;

    // update global metadata
    process_manager_state.latest_pid = new_pcb->metadata.pid;
exit_cleanup:
    if (retcode != PROCESS_CREATE_SUCCESS && new_pcb) {
        memset(new_pcb, 0, sizeof(struct ProcessControlBlock));
    }
    return retcode;
}

bool process_destroy(uint32_t pid) { 
    for (uint32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (process_manager_state.process_used[i] && _process_list[i].metadata.pid == pid) {
            // Clear PCB
            _process_list[i].metadata.process_state = TERMINATED;
            
            // Update process manager state
            process_manager_state.process_used[i] = false;
            process_manager_state.active_process_count--;
            
            // // Free all allocated pages
            // for (uint32_t j = 0; j < _process_list[i].memory.page_frame_used_count; j++) {
            //     if (_process_list[i].memory.virtual_addr_used[j]) {
            //         paging_free_user_page_frame(
            //             _process_list[i].context.page_directory_virtual_addr,
            //             _process_list[i].memory.virtual_addr_used[j]
            //         );
            //     }
            // }

            // // Free page directory
            // paging_free_page_directory(_process_list[i].context.page_directory_virtual_addr);
            
            
            return true;
        }
    }
    return false;
}

void process_init(void) {
    // make dummy process list and process manager state
    struct ProcessControlBlock pcsA = {
        .metadata = {
            .pid = 1,
            .process_state = READY,
            .name = "Process A",
            .length = 9
        },
        .context = {
            .eip = 0x00000000,
            .eflags = CPU_EFLAGS_BASE_FLAG,
            .page_directory_virtual_addr = NULL
        },
        .memory = {
            .virtual_addr_used = {NULL, NULL, NULL, NULL},
            .page_frame_used_count = 0
        }
    };

    struct ProcessControlBlock pcsB = {
        .metadata = {
            .pid = 2,
            .process_state = RUNNING,
            .name = "Process B",
            .length = 9
        },
        .context = {
            .eip = 0x00000000,
            .eflags = CPU_EFLAGS_BASE_FLAG,
            .page_directory_virtual_addr = NULL
        },
        .memory = {
            .virtual_addr_used = {NULL, NULL, NULL, NULL},
            .page_frame_used_count = 0
        }
    };

    struct ProcessControlBlock pcsC = {
        .metadata = {
            .pid = 3,
            .process_state = RUNNING,
            .name = "Process C",
            .length = 9
        },
        .context = {
            .eip = 0x00000000,
            .eflags = CPU_EFLAGS_BASE_FLAG,
            .page_directory_virtual_addr = NULL
        },
        .memory = {
            .virtual_addr_used = {NULL, NULL, NULL, NULL},
            .page_frame_used_count = 0
        }
    };

    _process_list[3] = pcsA;
    _process_list[1] = pcsB;
    _process_list[2] = pcsC;

    process_manager_state.active_process_count = 3;
    process_manager_state.latest_pid = 3;
    for (uint32_t i = 0; i < 4; i++) {
        process_manager_state.process_used[i] = true;
    }
}