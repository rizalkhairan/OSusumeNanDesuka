#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/process/process.h"

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
        [0]                            = false,
        [1 ... PAGE_FRAME_MAX_COUNT-1] = false
    },
    // TODO: Initialize page manager state properly
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT
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
    uint32_t required_frames = (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE; // this replicates ceil
    return page_manager_state.free_page_frame_count >= required_frames;
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
    for (int i = 0; i < PAGE_FRAME_MAX_COUNT; i++) {
        if (!page_manager_state.page_frame_map[i]) {
            page_manager_state.page_frame_map[i] = true;
            free_physical_frame_idx = i;
            page_manager_state.free_page_frame_count--;
            break;
        }
    }
    if (free_physical_frame_idx == -1) return false;

    uint32_t physical_addr = free_physical_frame_idx * PAGE_FRAME_SIZE;
    struct PageDirectoryEntryFlag flag = {
        .present_bit       = 1,
        .write_bit         = 1,
        .user_bit          = 1,
        .write_through_bit = 0,
        .cache_disabled_bit = 0,
        .accessed_bit = 0,
        .dirty_bit = 0,
        .use_pagesize_4_mb = 1,
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
    uint32_t virtual_addr_u32 = (uint32_t)virtual_addr;
    uint32_t page_index = virtual_addr_u32 >> 22;

    struct PageDirectoryEntry *entry = &page_dir->table[page_index];

    if (!entry->flag.present_bit || !entry->flag.user_bit) {
        return false; // Not a valid user mapping
    }

    uint32_t frame_index = (entry->lower_address | (entry->higher_address << 10));
    if (frame_index >= PAGE_FRAME_MAX_COUNT) {
        return false;
    }

    page_manager_state.page_frame_map[frame_index] = false;
    page_manager_state.free_page_frame_count++;

    // Clear the PDE
    memset(entry, 0, sizeof(struct PageDirectoryEntry));
    flush_single_tlb(virtual_addr);

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

// ----------------------------------- PROCESS -----------------------------------


__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct {
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager = {
    .page_directory_used = {false},
};

struct PageDirectory* paging_create_new_page_directory(void) {
    /*
     * TODO: Get & initialize empty page directory from page_directory_list
     * - Iterate page_directory_list[] & get unused page directory
     * - Mark selected page directory as used
     * - Create new page directory entry for kernel higher half with flag:
     *     > present bit    true
     *     > write bit      true
     *     > pagesize 4 mb  true
     *     > lower address  0
     * - Set page_directory.table[0x300] with kernel page directory entry
     * - Return the page directory address
     */ 

    for(uint32_t i=0;i<PAGING_DIRECTORY_TABLE_MAX_COUNT;i++){
        if (page_directory_manager.page_directory_used[i]==false){
            page_directory_manager.page_directory_used[i] = true;

            // ensure it's empty
            memset(&page_directory_list[i], 0, sizeof(struct PageDirectory));
            
            struct PageDirectoryEntry new_entry = {
                .flag.present_bit = true,
                .flag.write_bit = true,
                .flag.use_pagesize_4_mb = true,
                .lower_address = 0,
            };
            
            page_directory_list[i].table[0x300] = new_entry;            
            
            return &page_directory_list[i];
        }
    }

    return NULL;
}

bool paging_free_page_directory(struct PageDirectory *page_dir) {
    /**
     * TODO: Iterate & clear page directory values
     * - Iterate page_directory_list[] & check &page_directory_list[] == page_dir
     * - If matches, mark the page directory as unusued and clear all page directory entry
     * - Return true
     */
    for(uint32_t i=0;i<PAGING_DIRECTORY_TABLE_MAX_COUNT;i++){
        if(&page_directory_list[i] == page_dir){
            page_directory_manager.page_directory_used[i] = false;   
            memset(&page_directory_list[i], 0, sizeof(struct PageDirectory));
            return true;
        }
    }
    return false;
}

struct PageDirectory* paging_get_current_page_directory_addr(void) {
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr): /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory*) virtual_addr_page_dir;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr) {
    uint32_t physical_addr_page_dir = (uint32_t) page_dir_virtual_addr;
    // Additional layer of check & mistake safety net
    if ((uint32_t) page_dir_virtual_addr > KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__  volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir): "memory");
