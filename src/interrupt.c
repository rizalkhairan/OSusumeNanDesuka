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
    // __asm__ volatile("cli");
    // // Setup how often PIT fire
    // uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    // out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    // out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) (pit_timer_counter_to_fire & 0xFF));
    // out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) ((pit_timer_counter_to_fire >> 8) & 0xFF));

    // // Activate the interrupt
    // out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
    // __asm__ volatile("sti");
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
        case 20:
            // play sound
            play_sound(frame.cpu.general.ebx);
            break;
        case 21:
            // stop sound
            nosound();
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

/**
 * SYSCALL 13, 14, 15 HANDLERS
 * 
 * Copying and moving files/directories in EXT2 filesystem
 */
void get_entry_pure_name(void *entry, char *purename) {
    struct EXT2DirectoryEntry *dir_entry = (struct EXT2DirectoryEntry *)entry;
    if (dir_entry->inode == 0 || dir_entry->name_len == 0 || dir_entry->name_len > 255) {
        purename[0] = '\0';
        return;
    }

    char* name = (char*)(entry + sizeof(struct EXT2DirectoryEntry));
    memcpy(purename, name, dir_entry->name_len);
    purename[dir_entry->name_len] = '\0';
}

/*
0: Exists
1: Exists file
2: Not found
3: Parent invalid
-1: Unknown error
 */
int8_t exist_ext2(uint32_t parent_inode, const char *path, uint32_t *res_inode) {
    char temp_path[512];
    size_t len = 0;
    while (path[len] != '\0') len++;

    if (len >= sizeof(temp_path)) return -1; // too long
    memcpy(temp_path, path, len); // no null terminator needed

    size_t i = 0;

    while (i < len) {
        char token[256];
        for (int i=0; i < 256; i++){
            token[i] = 0;
        }
        size_t j = 0;

        while (i < len && temp_path[i] == '/') i++; // skip slashes

        while (i < len && temp_path[i] != '/' && j < sizeof(token)) {
            token[j++] = temp_path[i++];
        }

        if (j == 0) continue; 

        size_t k = i;
        while (k < len && temp_path[k] == '/') k++;

        bool is_last = (k >= len);

        struct EXT2DriverRequest req = {
            .parent_inode = parent_inode,
            .name = token,
            .name_len = j,
            .buf = NULL,
            .buffer_size = 0,
            .is_directory = true
        };

        struct EXT2Inode parent_inode_st;
        read_inode(parent_inode, &parent_inode_st);

        struct EXT2DirectoryEntry entry;
        bool found = find_directory_entry(&req, &parent_inode_st, &entry);

        if (!found && is_last){
            req.is_directory = false;
            found = find_directory_entry(&req, &parent_inode_st, &entry);
            if (!found) return 2; // file not found
        } else if (!found) {
            return 2; // folder not found
        }

        struct EXT2Inode child_inode;
        read_inode(entry.inode, &child_inode);
        uint16_t mode = child_inode.i_mode & 0xF000;

        // If this is the final component
        if (is_last) {
            if (res_inode) *res_inode = entry.inode;
            if (mode == 0x4000) return 0; // directory
            else if (mode == 0x8000) return 1; // file
            else return -1;
        }

        // Otherwise must be a directory to continue
        if (mode != 0x4000) return 3;
        parent_inode = entry.inode;
    }

    return -1; 
}

/*
0: Success
1: Source not found
2: Destination not found/invalid
3: Invalid parent (either dest or source)
4: Failed to read source
5: Failed to write to destination
6: Directory child read/write failed
-1: Unknown error
 */
int8_t copy_cp(struct EXT2CopyRequest* copy_request){
    char dest_parent[512], dest_leaf[256];
    split_path(copy_request->destination, dest_parent, dest_leaf);
    char src_parent[512], src_leaf[256];
    split_path(copy_request->source, src_parent, src_leaf);

    // Check if source exists 
    uint32_t src_inode;
    int8_t src_res = exist_ext2(copy_request->root_inode, copy_request->source, &src_inode);
    if (src_res != 0 && src_res != 1) return (src_res == 2) ? 1 : src_res;

    // Check if destination exists 
    char *final_name;
    uint32_t dest_inode; // inode of target directory

    int8_t dest_res = exist_ext2(copy_request->root_inode, copy_request->destination, &dest_inode);
    if (dest_res == 0){ // it is a directory and it exists
        final_name = src_leaf;
    } else { // does not exists or is a file
        dest_res = exist_ext2(copy_request->root_inode, dest_parent, &dest_inode);
        if (dest_res != 0){
            return (dest_res == 1) ? 2 : dest_res;
        } 
        final_name = dest_leaf;
    }

    size_t src_leaf_len = 0;
    while (src_leaf[src_leaf_len] != '\0') src_leaf_len++;
    size_t final_name_len = 0;
    while (final_name[final_name_len] != '\0') final_name_len++;

    // Writing Base
    uint8_t buffer[BLOCK_SIZE * 16];  // Allocate buffer for file data 

    uint32_t src_parent_inode = copy_request->root_inode;
    int8_t src_parent_res = exist_ext2(copy_request->root_inode, src_parent, &src_parent_inode);
    if (src_parent_res != 0 && src_parent_res != 1) return (src_parent_res == 2) ? 1 : src_parent_res;

    struct EXT2DriverRequest read_req = {
        .buf = buffer,
        .name = (char *)src_leaf,
        .name_len = src_leaf_len,
        .parent_inode = src_parent_inode,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = src_res == 0 
    };

    int8_t read_stat;
    if (src_res == 0){
        read_stat = read_directory(&read_req);
        if (read_stat != 0) return 4; // read failed
    } else {
        read_stat = read(read_req);
        if (read_stat != 0) return 4; // read failed
    }

    struct EXT2Inode src_node;
    read_inode(src_inode, &src_node);
    // 2. Write to destination
    struct EXT2DriverRequest write_req = {
        .buf = buffer,
        .name = (char *)final_name,
        .name_len = final_name_len,
        .parent_inode = dest_inode,
        .buffer_size = src_node.i_size,
        .is_directory = src_res == 0
    };

    int8_t write_stat = write(&write_req);
    if (write_stat != 0) return 5; // write failed

    // Writing children of directory
    if (src_res == 0){
        uint32_t created_inode;
        int8_t created_res = exist_ext2(dest_inode, final_name, &created_inode); // should succeed everytime if didnt return 5 before
        if (created_res != 0) return 6;

        int8_t copy_child = recursive_move_dir_files(src_parent_inode, src_leaf, src_inode, created_inode);
        if (copy_child != 0) return 6; 
    }

    return 0; // success
}

/*
0: Success
1: Source not found
2: Destination not found/invalid
3: Invalid parent (either dest or source)
4: Failed to read source
5: Failed to write to destination
6: Directory child read/write failed
-1: Unknown error
 */
int8_t move_mv(struct EXT2CopyRequest* copy_request){
    uint32_t src_inode;
    int8_t src_res = exist_ext2(copy_request->root_inode, copy_request->source, &src_inode);
    if (src_res != 0 && src_res != 1) return (src_res == 2) ? 1 : src_res;

    int8_t copy_res = copy_cp(copy_request);
    if (copy_res != 0) return copy_res;
    
    char src_parent[512], src_leaf[256];
    split_path(copy_request->source, src_parent, src_leaf);
    size_t src_leaf_len = 0;
    while (src_leaf[src_leaf_len] != '\0') src_leaf_len++;

    uint32_t parent_inode;
    int8_t parent_res = exist_ext2(copy_request->root_inode, src_parent, &parent_inode);
    if (parent_res != 0 && parent_res != 1) return (parent_res == 2) ? 1 : parent_res;

    if (src_res == 0){ // directory delete
        int8_t delete_res = delete_recur_dir(parent_inode, src_leaf);
        return (delete_res == 0) ? 0 : -1;
    } else { // file delete
        struct EXT2DriverRequest delete_req = {
            .buf = NULL,
            .name = src_leaf,
            .name_len = src_leaf_len,
            .parent_inode = copy_request->root_inode,
            .buffer_size = 0,
            .is_directory = false
        };
        int8_t delete_res = delete(delete_req);
        return (delete_res == 0) ? 0 : -1;
    }
}


/*
0: Success
1: Failed
-1: Unknown error
 */
int8_t recursive_move_dir_files(uint32_t src_parent, char* src_name, uint32_t src_dir_inode, uint32_t dest_dir_inode) {
    size_t src_name_len = 0;
    while (src_name[src_name_len] != '\0') src_name_len++;
    struct EXT2Inode src_inode;
    read_inode(src_dir_inode, &src_inode);
    
    uint8_t buffer[BLOCK_SIZE * 16]; // big enough for directory contents

    struct EXT2DriverRequest req = {
        .buf = buffer,
        .name = (char *) src_name,
        .name_len = src_name_len,
        .parent_inode = src_parent,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = true
    };
    read_directory(&req);

    struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*) buffer;
    get_directory_entry(entry, 0);
    uint32_t directory_inodes[BLOCK_SIZE]; // Assume that a directory cannot contain more than 512 other directories
    char directory_names[BLOCK_SIZE][256]; 
    // uint32_t directory_inodes_created[BLOCK_SIZE];
    size_t current_dir_entry = 0;
    while (true){
        uint32_t cur_inode = entry->inode;
        char cur_name[256];
        get_entry_pure_name(entry, cur_name);
        if (!memcmp(cur_name, ".", 1) || !memcmp(cur_name, "..", 2)) {
            struct EXT2DirectoryEntry* t_entry;
            t_entry = get_next_directory_entry(entry);
            if (t_entry == entry){
                break;
            } else {
                entry = t_entry;
                continue;
            }
        }

        if (entry->file_type == EXT2_FT_DIR){
            directory_inodes[current_dir_entry] = cur_inode;
            memcpy(directory_names[current_dir_entry], cur_name, entry->name_len);
            directory_names[current_dir_entry][entry->name_len] = '\0'; 
            current_dir_entry++;

        } else if (entry->file_type == EXT2_FT_REG_FILE){
            // READ
            int entry_length = entry->name_len;
            uint8_t entryBuffer[BLOCK_SIZE * 16];
            struct EXT2DriverRequest entry_req = {
                .buf = entryBuffer,
                .name = cur_name,
                .name_len = entry_length,
                .parent_inode = src_dir_inode,
                .buffer_size = BLOCK_SIZE * 16,
                .is_directory = false
            };
            int8_t read_res = read(entry_req);

            struct EXT2Inode cur_node;
            read_inode(cur_inode, &cur_node);
            // WRITE
            struct EXT2DriverRequest write_req = {
                .buf = entryBuffer,
                .name = cur_name,
                .name_len = entry_length,
                .parent_inode = dest_dir_inode,
                .buffer_size = cur_node.i_size,
                .is_directory = false
            };
            int8_t write_res = write(&write_req);
        }

        struct EXT2DirectoryEntry* temp_entry;
        temp_entry = get_next_directory_entry(entry);
        if (temp_entry == entry){
            break;
        } else {
            entry = temp_entry;
        }
    }
    
    for (int i = 0; i < current_dir_entry; i++){
        recursive_make_dir(src_dir_inode, &directory_names[i][0], directory_inodes[i], dest_dir_inode);
    }

    return 0; // success
}

/*
0: Success
1: Failed
-1: Unknown error
 */
int8_t recursive_make_dir(uint32_t src_parent, char* src_name, uint32_t src_dir_inode, uint32_t dest_dir_inode){
    uint8_t buffer[BLOCK_SIZE * 16];
    size_t src_name_len = 0;
    while (src_name[src_name_len] != '\0') src_name_len++;
    
    // READ
    uint8_t entryBuffer[BLOCK_SIZE * 16];
    struct EXT2DriverRequest entry_req = {
        .buf = buffer,
        .name = src_name,
        .name_len = src_name_len,
        .parent_inode = src_parent,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = true
    };
    int8_t read_res = read_directory(&entry_req);
    if (read_res != 0) return (read_res == -1) ? -1 : 1;

    struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*) buffer;
    get_directory_entry(entry, 0);

    // WRITE
    struct EXT2DriverRequest write_req = {
        .buf = buffer,
        .name = src_name,
        .name_len = src_name_len,
        .parent_inode = dest_dir_inode,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = true
    };
    int8_t write_res = write(&write_req);
    if (write_res != 0) return (write_res == -1) ? -1 : 1;

    read_res = read_directory(&write_req);
    struct EXT2DirectoryEntry* write_entry = (struct EXT2DirectoryEntry*) buffer;
    get_directory_entry(write_entry, 0);

    recursive_move_dir_files(src_dir_inode, src_name, entry->inode, write_entry->inode);
}

/*
0: Success
1: Failed
-1: Unknown error
 */
int8_t delete_recur_dir(uint32_t parent_inode, char* cur_dir_name){
    size_t cur_name_len = 0;
    while (cur_dir_name[cur_name_len] != '\0') cur_name_len++;

    uint8_t buffer[BLOCK_SIZE * 16];
    struct EXT2DriverRequest cur_dir = {
        .buf = buffer,
        .name = cur_dir_name,
        .name_len = cur_name_len,
        .parent_inode = parent_inode,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = true
    };
    int8_t read_res = read_directory(&cur_dir);
    if (read_res != 0) return (read_res == -1) ? -1 : 1;

    struct EXT2DirectoryEntry* dir_entry = (struct EXT2DirectoryEntry*) buffer;
    get_directory_entry(dir_entry, 0);
    uint32_t cur_dir_inode = dir_entry->inode;
    bool all_children_killed = true;

    while (true){
        uint32_t cur_inode = dir_entry->inode;
        char cur_name[256];
        get_entry_pure_name(dir_entry, cur_name);
        if (!memcmp(cur_name, ".", 1) || !memcmp(cur_name, "..", 2)) {
            struct EXT2DirectoryEntry* t_entry;
            t_entry = get_next_directory_entry(dir_entry);
            if (t_entry == dir_entry){
                break;
            } else {
                dir_entry = t_entry;
                continue;
            }
        }

        if (dir_entry->file_type == EXT2_FT_DIR){
            int8_t delete_res = delete_recur_dir(cur_dir_inode, cur_name);
            if (delete_res != 0) all_children_killed = false;
        } else if (dir_entry->file_type == EXT2_FT_REG_FILE){
            struct EXT2DriverRequest delete_req = {
                .buf = NULL,
                .name = cur_name,
                .name_len = dir_entry->name_len,
                .parent_inode = cur_dir_inode,
                .buffer_size = 0,
                .is_directory = false
            };
            int8_t delete_res = delete(delete_req);
            if (delete_res != 0) all_children_killed = false;
        }

        struct EXT2DirectoryEntry* temp_entry;
        temp_entry = get_next_directory_entry(dir_entry);
        if (temp_entry == dir_entry){
            break;
        } else {
            dir_entry = temp_entry;
        }
    }

    if (all_children_killed){ // finally delete parent dir
        struct EXT2DriverRequest delete_req = {
            .buf = NULL,
            .name = cur_dir_name,
            .name_len = cur_name_len,
            .parent_inode = parent_inode,
            .buffer_size = 0,
            .is_directory = true
        };
        int8_t delete_res = delete(delete_req);
        return delete_res;
    } else {
        return 1;
    }
}

void split_path(const char *path, char *parent_out, char *leaf_out) {
    size_t len = 0;
    while (path[len] != '\0') len++;

    size_t slash_before_last_file = len;
    while (path[slash_before_last_file - 1] == '/') slash_before_last_file--; // account for trailing slashes
    while (slash_before_last_file > 0 && path[slash_before_last_file - 1] != '/') {
        slash_before_last_file--;
    }

    // Copy parent
    for (size_t i = 0; i < slash_before_last_file && i < 511; i++) {
        parent_out[i] = path[i];
    }
    parent_out[slash_before_last_file] = '\0';

    // Copy leaf
    size_t j = 0;
    for (size_t i = slash_before_last_file; i < len && j < 255; i++) {
        leaf_out[j++] = path[i];
    }
    leaf_out[j] = '\0';

    if (slash_before_last_file == 0){
        parent_out[0] = '.';
        parent_out[1] = '/';
        parent_out[2] = '\0';
    }
}


/**
 * SYSCALL 20 and 21 HELPERS
 *
 * Play sound using the Programmable Interval Timer (PIT) channel 2.
 * The frequency is set by writing to the PIT, and the speaker is enabled
 * by manipulating the control port 0x61.
 */
void play_sound(uint32_t freq) {
    if (freq == 0) return;

    uint32_t divisor = 1193180 / freq;

    // Command byte to set PIT channel 2, access mode lobyte/hibyte, mode 3 (square wave)
    out(0x43, 0xB6);

    // Set frequency divisor for channel 2
    out(0x42, (uint8_t)(divisor & 0xFF));         // Low byte
    out(0x42, (uint8_t)((divisor >> 8) & 0xFF));  // High byte

    // Enable the speaker by setting bits 0 and 1 at port 0x61
    uint8_t tmp = in(0x61);
    if ((tmp & 0x03) != 0x03) {
        out(0x61, tmp | 0x03);
    }
}

void nosound() {
    // Clear bits 0 and 1 at port 0x61 to turn off the speaker
    uint8_t tmp = in(0x61) & 0xFC;
    out(0x61, tmp);
}