#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
        [0x300] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
    }
};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0]                            = true,
        [1 ... PAGE_FRAME_MAX_COUNT-1] = false
    },
    // TODO: Initialize page manager state properly
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT-1
    // The above was added
};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr, 
    void *virtual_addr, 
    struct PageDirectoryEntryFlag flag
) {
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag          = flag;
    page_dir->table[page_index].lower_address = ((uint32_t) physical_addr >> 22) & 0x3FF;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr): "memory");
}



/* --- Memory Management --- */
// TODO: Implement
bool paging_allocate_check(uint32_t amount) {
    // TODO: Check whether requested amount is available
    if (page_manager_state.free_page_frame_count < amount) return false;
    // The above was added
    return true;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /**
     * TODO: Find free physical frame and map virtual frame into it
     * - Find free physical frame in page_manager_state.page_frame_map[] using any strategies
     * - Mark page_manager_state.page_frame_map[]
     * - Update page directory with user flags:
     *     > present bit    true
     *     > write bit      true
     *     > user bit       true
     *     > pagesize 4 mb  true
     */ 

    // FIRST FIT STRATEGY
    int free_physical_frame_idx = -1;
    for (int i = 1; i < PAGE_FRAME_MAX_COUNT; i++) {
        if (!page_manager_state.page_frame_map[i]) {
            page_manager_state.page_frame_map[i] = true;
            free_physical_frame_idx = i;
            page_manager_state.free_page_frame_count--;
            break;
        }
    }
    if (free_physical_frame_idx == -1) return false;

    uint32_t physical_addr = free_physical_frame_idx * 0x400000;
    struct PageDirectoryEntryFlag flag = {
        .present_bit       = 1,
        .write_bit         = 1,
        .user_bit          = 1,
        .use_pagesize_4_mb = 1
    };
    update_page_directory_entry(page_dir, (void *)physical_addr, virtual_addr, flag);

    return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /* 
     * TODO: Deallocate a physical frame from respective virtual address
     * - Use the page_dir.table values to check mapped physical frame
     * - Remove the entry by setting it into 0
     */
    uint32_t page_directory_idx = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    if (!page_dir->table[page_directory_idx].flag.present_bit) return false;

    uint32_t physical_addr = (page_dir->table[page_directory_idx].lower_address << 22);
    int physical_frame_idx = physical_addr / 0x400000;

    page_manager_state.page_frame_map[physical_frame_idx] = false;
    page_manager_state.free_page_frame_count++;

    struct PageDirectoryEntryFlag empty_flag = {0};
    update_page_directory_entry(page_dir, NULL, virtual_addr, empty_flag);

    return true;
}

void map_identity_vga(struct PageDirectory *page_dir) {
    struct PageDirectoryEntryFlag flag = {
        .present_bit       = 1,
        .write_bit         = 1,
        .user_bit          = 1, // Kernel-only
        .use_pagesize_4_mb = 1
    };
    update_page_directory_entry(page_dir, (void *)0x00000000, (void *)0x00000000, flag);
}

