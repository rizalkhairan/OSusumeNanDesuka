#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'D', 'e', 'v', 'e', 'l', 'o', 'p', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ',  ' ',
    'O', 'S', 'u', 's', 'u', 'm', 'e', 'N', 'a', 'n', 'D', 'e', 's', 'u', 'k',  'a',
    'I', 'F', '2', '1', '3', '0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'I', 'T', 'B', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/* =============================== GENERAL ==========================================*/

char *get_entry_name(void *entry){
    struct EXT2DirectoryEntry *dir_entry = (struct EXT2DirectoryEntry *)entry;
    if(dir_entry->inode==0 || dir_entry->name_len == 0 || dir_entry->name_len > 255){
        return NULL;
    } else{
        char* name = (char*)(entry + sizeof(struct EXT2DirectoryEntry));
        return name;
    }
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset){
    if (offset < 0) {
        return NULL;
    }
    if (offset > 0) {
        struct EXT2DirectoryEntry *current_entry = (struct EXT2DirectoryEntry*) ptr; 
        struct EXT2DirectoryEntry *next_entry = ptr + current_entry->rec_len;
        return get_directory_entry(next_entry, offset-1);
    }
    return (struct EXT2DirectoryEntry*) ptr;
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry){
    struct EXT2DirectoryEntry* next = (struct EXT2DirectoryEntry*)((uint8_t*)entry + entry->rec_len);
    return next;
}

uint16_t get_entry_record_len(uint8_t name_len){
    uint16_t total = (name_len + sizeof(struct EXT2DirectoryEntry)); // naive size
    total = (total + 3) & ~0x03; // padding
    return total;
}

uint32_t get_dir_first_child_offset(void *ptr){
    // First entry (.)
    struct EXT2DirectoryEntry *secondEntry = get_directory_entry(ptr, 0);
    // Second entry (..)
    secondEntry = get_next_directory_entry(secondEntry);
    
    return (uint32_t)((uint8_t *)secondEntry - (uint8_t *)ptr);
}

uint32_t inode_to_bgd(uint32_t inode){
    return (inode-1)/INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode){
    return (inode-1) % INODES_PER_GROUP;
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode){
    struct BlockBuffer buf = {0};

    // Create self .
    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)buf.buf;
    dot->inode = inode;
    dot->rec_len = 12; // 8 (struct) + 1 (name) + padding = 12
    dot->name_len = 1;
    dot->file_type = 2; // 2 = directory
    *((char *)(dot + 1)) = '.';

    // Create .. (parent)
    struct EXT2DirectoryEntry *dotdot = (struct EXT2DirectoryEntry *)((uint8_t *)dot + dot->rec_len);
    dotdot->inode = parent_inode;
    dotdot->rec_len = 12; 
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    *((char *)(dotdot + 1)) = '.';
    *((char *)(dotdot + 1) + 1) = '.';

    // Allocate new block for this directory
    allocate_node_blocks(buf.buf, node, inode_to_bgd(inode));
}

/* =============================== INITIALIZER ==========================================*/

struct EXT2Superblock sb = {            //TODO: recheck values...
    .s_inodes_count = INODES_PER_GROUP*GROUPS_COUNT,
    .s_blocks_count = BLOCKS_PER_GROUP*GROUPS_COUNT,
    .s_r_blocks_count = 0,
    .s_free_blocks_count = BLOCKS_PER_GROUP*GROUPS_COUNT - 10,
    .s_free_inodes_count = INODES_PER_GROUP*GROUPS_COUNT - 1,
    .s_first_data_block = 1,
    .s_first_ino = 2,
    .s_blocks_per_group = BLOCKS_PER_GROUP,
    .s_frags_per_group = BLOCKS_PER_GROUP,
    .s_inodes_per_group = INODES_PER_GROUP,
    .s_magic = EXT2_SUPER_MAGIC,
    .s_prealloc_blocks = 0,
    .s_prealloc_dir_blocks = 0,
};

struct EXT2BlockGroupDescriptorTable bgdt = {};

bool is_empty_storage(void){
    struct BlockBuffer b;
    read_blocks(&b, 0, 1);
    int res = memcmp(fs_signature, b.buf, BLOCK_SIZE);
    if(res==0){
        return false;
    }
    return true;
}


void create_ext2(void){
    /* Block scheme:

                Block# modulo BLOCKS_PER_GROUP
                |  0  |  1  |  2  |  3  |  4  |  5  | ... | 18  | 19  | 20  | 21  | ... | BLOCKS_PER_GROUP-1
    Group#  0   |sign |super|bgdt*|bbitm|ibitm|inodes---------------------->|free...
            1   |bgdt |bbitm|ibitm|inodes---------------->|free...
            2   |bgdt |bbitm|ibitm|inodes---------------->|free...
            3   |bgdt |bbitm|ibitm|inodes---------------->|free...
            .
            .
    GROUPS_COUNT|bgdt |bbitm|ibitm|inodes---------------->|free...
            -1  |     |     |     |                       |
            Legend: sign = fs_signature, super = superblock, bgdt* = block group descriptor table (prime copy)
            bgdt = copy/backup of bgdt*, bbitm = block bitmap, ibitm = inode bitmap, inodes = inode table
    
            Note:
                - always refer to the actual block number stored in the BGDs
                - no inode will span multiple blocks, there is a gap at the end of an inode block (use read_inode function)
    */

    struct BlockBuffer b;
    memcpy(b.buf, fs_signature, BLOCK_SIZE);
    write_blocks(&b, 0, 1);  // Signature
    memset(b.buf, 0x0, BLOCK_SIZE);
    memcpy(b.buf, &sb, sizeof(struct EXT2Superblock));
    write_blocks(b.buf, 1, 1); // Superblock
   
    // Signature, superblock, BGDT, block bitmap, inode bitmap, and INODES_TABLE_BLOCK_COUNT blocks of inodes
    uint8_t initial_group_blocks = 5 + INODES_TABLE_BLOCK_COUNT;
    for(uint8_t i=0;i<GROUPS_COUNT;i++){ // BGDs
        if (i==0) {
            struct EXT2BlockGroupDescriptor bgd_template = {
                .bg_block_bitmap = 3 + (i*BLOCKS_PER_GROUP),
                .bg_inode_bitmap = 4 + (i*BLOCKS_PER_GROUP),
                .bg_inode_table = 5 + (i*BLOCKS_PER_GROUP),
                .bg_free_blocks_count = BLOCKS_PER_GROUP-initial_group_blocks,
                .bg_free_inodes_count = INODES_PER_GROUP,
                .bg_used_dirs_count = 0,
                .bg_pad = 0,
                .bg_reserved = {0,0,0}
            };
            bgdt.table[i] = bgd_template;
        }
        else {
            struct EXT2BlockGroupDescriptor bgd_template = {
                .bg_block_bitmap = 1 + (i*BLOCKS_PER_GROUP),
                .bg_inode_bitmap = 2 + (i*BLOCKS_PER_GROUP),
                .bg_inode_table = 3 + (i*BLOCKS_PER_GROUP),
                .bg_free_blocks_count = BLOCKS_PER_GROUP-(initial_group_blocks-2),  // Without signature and superblock
                .bg_free_inodes_count = INODES_PER_GROUP,
                .bg_used_dirs_count = 0,
                .bg_pad = 0,
                .bg_reserved = {0,0,0}
            };
            bgdt.table[i] = bgd_template;
        }
    }

    // Write BGDT
    memset(b.buf, 0, BLOCK_SIZE);
    memcpy(b.buf, &bgdt, sizeof(struct EXT2BlockGroupDescriptorTable));
    for (uint8_t group=0;group<GROUPS_COUNT;group++) {
        if (group == 0) {
            write_blocks(b.buf, 2, 1);
        }
        else {
            write_blocks(b.buf, group * BLOCKS_PER_GROUP, 1);
        }
    }
    
    // Write inode bitmap
    memset(b.buf, 0, BLOCK_SIZE);
    set_bitmap_bit(b.buf, 0, true); // Mark inode 0 as unused (invalid inode)
    set_bitmap_bit(b.buf, 1, true); // Mark inode 1 as used (unclear inode). Inode 2 is synced later
    write_blocks(b.buf, bgdt.table[0].bg_inode_bitmap, 1);
    set_bitmap_bit(b.buf, 0, false); // After group 0, all local inodes are free
    set_bitmap_bit(b.buf, 1, false);
    for (uint8_t group=1;group<GROUPS_COUNT;group++) {
        write_blocks(b.buf, bgdt.table[group].bg_inode_bitmap, 1);
    }
    // Block bitmap
    for (uint8_t i=0;i<initial_group_blocks;i++){
        set_bitmap_bit(b.buf, i, true);
    }
    write_blocks(b.buf, bgdt.table[0].bg_block_bitmap, 1);
    set_bitmap_bit(b.buf, initial_group_blocks-1, false); // Without superblock and signature
    set_bitmap_bit(b.buf, initial_group_blocks-2, false);
    for (uint8_t group=1;group<GROUPS_COUNT;group++) {
        write_blocks(b.buf, bgdt.table[group].bg_block_bitmap, 1);
    }

    // create root directory. TO DO
    struct EXT2Inode root_inode = {
        .i_mode = 0x4000, // Directory
        .i_size = BLOCK_SIZE, // TODO: RECHECK
        .i_blocks = 1
    };
    memset(root_inode.i_block, 0x0, 15 * sizeof(uint32_t));
    sync_node(&root_inode, 2);
    updateBGDTInode(2, true); // TODO: when to update global metadata?
    bgdt.table[0].bg_used_dirs_count += 1;
    
    sb.s_free_inodes_count -= 1;

    init_directory_table(&root_inode, 2, 2);
    sync_node(&root_inode, 2);
}

void initialize_filesystem_ext2(void){
    if(is_empty_storage()){
        create_ext2();
    } else{
        read_blocks(&sb, 1, 1); // Read Superblock
        struct BlockBuffer bgd_block_buf;
        int bgd_block = 2;
        read_blocks(&bgd_block_buf.buf, bgd_block, 1);
        for(int i=0;i<GROUPS_COUNT;i++){
            memcpy(&bgdt.table[i], bgd_block_buf.buf + i * sizeof(struct EXT2BlockGroupDescriptor), sizeof(struct EXT2BlockGroupDescriptor));
        }
    }
}

bool is_directory_empty(uint32_t inode){
    // Finding current inode from inode number
    struct EXT2Inode *currentInode;
    read_inode(inode, currentInode);

    struct BlockBuffer buf;
    read_blocks(&buf, currentInode->i_block[0], 1);

    uint32_t offset = 0;
    struct EXT2DirectoryEntry *self = (struct EXT2DirectoryEntry *)(buf.buf + offset);
    offset += self->rec_len;
    struct EXT2DirectoryEntry *parent = (struct EXT2DirectoryEntry *)(buf.buf + offset);
    if (parent->rec_len == 0){
        return true;
    } else {
        return false;
    }
}

/* =============================== CRUD ==========================================*/

int8_t read_directory(struct EXT2DriverRequest *prequest){
     // Unknown / invalid input
    if (prequest == NULL || prequest->buf == NULL || prequest->buffer_size == 0)
    return -1;

    // Validate parent inode
    struct EXT2Inode parent_inode;
    read_inode(prequest->parent_inode, &parent_inode);
    if ((parent_inode.i_mode & 0xF000) != 0x4000) return 3; 

    // Validate if parent inode has child
    struct EXT2DirectoryEntry entry;
    bool found = find_directory_entry(&parent_inode, prequest->name, prequest->name_len, &entry); 
    if (!found) return 2;

    // Validate child is a directory
    struct EXT2Inode child_inode;
    read_inode(entry.inode, &child_inode);
    if ((child_inode.i_mode & 0xF000) != 0x4000) return 1;

    // Copy blocks of directory to buf
    load_inode_data(&child_inode, prequest->buf, prequest->buffer_size);

    return 0;
}

int8_t read(struct EXT2DriverRequest request){
    struct EXT2Inode inode;
    struct EXT2DirectoryEntry entry;

    // Unknown / invalid input
    if (request.buf == NULL || request.buffer_size == 0 || request.name == NULL || request.name_len == 0) {
        return -1;
    }
    if (request.is_directory) {
        return 1;
    }

    read_inode(request.parent_inode, &inode);   // Assume inode validity
    if ((inode.i_mode & 0xF000)!=EXT2_S_IFDIR) {
        return 4;   // Invalid inode or inode is not a directory
    }

    // Search file
    if (!find_directory_entry(&inode, request.name, request.name_len, &entry)) {
        return 3;   // File not found
    }
    if (entry.file_type != EXT2_FT_REG_FILE) {
        return 1;   // Not a file
    }

    // Load data
    read_inode(entry.inode, &inode);
    if (inode.i_size > request.buffer_size) {
        return 2;   // Not enough buffer
    }

    load_inode_data(&inode, request.buf, request.buffer_size);
    return 0;   // Success
}

int8_t write(struct EXT2DriverRequest *request){
    // unknown/invalid input
    if (request == NULL || ((request->buf == NULL || request->buffer_size == 0) && request->is_directory == false))
    return -1;

    // Validate parent inode
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);
    if ((parent_inode.i_mode & 0xF000) != 0x4000) return 2;

    // Validate if file/folder already exists
    struct BlockBuffer dir_entry;
    read_blocks(&dir_entry, parent_inode.i_block[0], 1);
    struct EXT2DirectoryEntry *ptr = get_directory_entry(&dir_entry, 0);
    struct EXT2DirectoryEntry *new_ptr = ptr;
    while (true){
        ptr = new_ptr;
        if (memcmp(get_entry_name(ptr), request->name, request->name_len) == 0){
            // file
            if(ptr->file_type == 1 && request->is_directory == 0){
                return 1;
            } 
            
            // directory
            if(ptr->file_type == 2 && request->is_directory == 1){
                return 1;
            }
        }
        new_ptr = get_next_directory_entry(ptr);

        if (new_ptr->rec_len == 0){
            break;
        }
    }

    // Validate whether there's enough blocks to hold the data
    uint32_t blocks_needed = (request->buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(!exists_n_free_blocks(blocks_needed)) return -1;

    // Validate whether there's an available inode
    uint32_t new_inode_number = allocate_node();
    if(new_inode_number == 0) return -1;
    
    // Write file 
    if (!request->is_directory){
        struct EXT2Inode *new_inode;    
        // Alokasikan memori untuk inode baru
        struct EXT2Inode temp_inode = {0};
        new_inode = &temp_inode;

        // Set permission: 0x8000 untuk file reguler | 0x0200 = user write permission
        new_inode->i_mode = 0x8000 | 0x0200;

        // Ukuran file
        new_inode->i_size = request->buffer_size;

        // Jumlah blok (dalam 512-byte blocks, EXT2 i_blocks menyimpan ukuran dalam 512-byte block)
        new_inode->i_blocks = blocks_needed * (BLOCK_SIZE / 512);

        // Alokasikan blok dan isi dengan data
        allocate_node_blocks(request->buf, new_inode, inode_to_bgd(request->parent_inode));

        // Buat entri direktori untuk file baru
        struct EXT2DirectoryEntry new_entry;
        new_entry.inode = new_inode_number;
        new_entry.rec_len = get_entry_record_len(request->name_len);
        new_entry.name_len = request->name_len;
        new_entry.file_type = 1; // 1 = file reguler

        add_directory_entry(new_entry, request->name, request->parent_inode);
        // Sync the inode table and bitmap to disk
        sync_node(new_inode, new_inode_number);
    }


    // Write directory
    else {
        // Directory entry for the request
        struct EXT2DirectoryEntry new_entry ={
            .inode = new_inode_number,
            .rec_len = get_entry_record_len(request->name_len),
            .name_len = request->name_len,
            .file_type = 2,
        };
        
        // Allocate corresponding Inode for the new directory
        struct EXT2Inode new_inode = {
            new_inode.i_mode = 0x4000,
            new_inode.i_size = request->buffer_size,
            new_inode.i_blocks = 1,
        };
        init_directory_table(&new_inode, new_inode_number, request->parent_inode);
        sync_node(&new_inode, new_inode_number);

        // Add the new directory to its parent's directory entry
        add_directory_entry(new_entry, request->name, request->parent_inode);
        // Sync the inode table and bitmap to disk
        sync_node(&new_inode, new_inode_number);
        updateBGDTInode(new_inode_number, true);
    }

    // Should parent be synced too?
    struct EXT2Inode *parent;
    read_inode(request->parent_inode, parent);
    sync_node(parent, request->parent_inode);
    return 0;
}

int8_t delete(struct EXT2DriverRequest request) {
    // Validate input parameters
    if (request.name == NULL || request.name_len == 0) {
        return -1; // Invalid request
    }

    // Read parent inode
    struct EXT2Inode parent_inode;
    read_inode(request.parent_inode, &parent_inode);

    // Check if parent is a directory
    if ((parent_inode.i_mode & 0xF000) != 0x4000) {
        return 3; // Parent is not a directory
    }

    // Find the directory entry in parent
    struct EXT2DirectoryEntry entry;
    bool found = find_directory_entry(&parent_inode, request.name, request.name_len, &entry);
    if (!found) {
        return 2; // Entry not found
    }

    // Read child inode to check type
    struct EXT2Inode child_inode;
    read_inode(entry.inode, &child_inode);

    // If it's a directory, ensure it's empty
    if ((child_inode.i_mode & 0xF000) == 0x4000) { // Directory
        if (!is_directory_empty(entry.inode)) {
            return 1; // Directory not empty
        }
    }

    // Deallocate the inode and its blocks
    deallocate_node(entry.inode);
    updateBGDTInode(entry.inode, false);

    // Function to search and mark entry in a block

    // Check direct blocks
    for (int i = 0; i < 12; i++) {
        if (parent_inode.i_block[i] != 0 && mark_entry_in_block(parent_inode.i_block[i], &entry, &request)) {
            return 0; // Success
        }
    }

    // Check singly indirect block
    if (parent_inode.i_block[12] != 0) {
        uint32_t indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect, parent_inode.i_block[12], 1);

        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (indirect[i] != 0 && mark_entry_in_block(indirect[i], &entry, &request)) {
                return 0;
            }
        }
    }

    // Check doubly indirect block
    if (parent_inode.i_block[13] != 0) {
        uint32_t doubly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(doubly_indirect, parent_inode.i_block[13], 1);

        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (doubly_indirect[i] == 0) continue;

            uint32_t singly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(singly_indirect, doubly_indirect[i], 1);

            for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
                if (singly_indirect[j] != 0 && mark_entry_in_block(singly_indirect[j], &entry, &request)) {
                    return 0;
                }
            }
        }
    }

    return -1; // Entry not found
}

/* =============================== MEMORY ==========================================*/

// Return inode number
// This function does not have any side effects (memory/disk modification)
uint32_t allocate_node(void){
    uint32_t inode_number = 0;
    struct BlockBuffer bitmap;
    for (int i=0; i<GROUPS_COUNT; ++i){
        struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[i];
        read_blocks(bitmap.buf, bgd->bg_inode_bitmap, 1);
        for (uint32_t local_inode=0;local_inode<INODES_PER_GROUP;local_inode++) {
            if (!is_bitmap_set(bitmap.buf, local_inode)) {
                return local_inode + (i * INODES_PER_GROUP);
            }
        }
    }

    return 0;
}

void deallocate_node(uint32_t inode_num) {
    if (inode_num == 0) return;
    
    struct EXT2Inode node;
    read_inode(inode_num, &node);
    
    // Free the data blocks associated with this inode
    struct BlockBuffer bitmap;
    uint32_t bgd_index = inode_to_bgd(inode_num);
    uint32_t last_bgd_idx = bgd_index;
    
    // Read the block bitmap for the BGD containing this inode
    read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
    
    // Free direct blocks
    for (int i = 0; i < 12; i++) {
        if (node.i_block[i] == 0) continue;
        
        uint32_t block_bgd = node.i_block[i] / BLOCKS_PER_GROUP;
        if (block_bgd != last_bgd_idx) {
            // Write current bitmap and load the new one
            write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
            read_blocks(&bitmap, bgdt.table[block_bgd].bg_block_bitmap, 1);
            last_bgd_idx = block_bgd;
        }
        
        // Mark block as free in bitmap
        uint32_t block_in_group = node.i_block[i] % BLOCKS_PER_GROUP;
        set_bitmap_bit(&bitmap, block_in_group, false);
        
        // Update block count
        bgdt.table[block_bgd].bg_free_blocks_count++;
        node.i_block[i] = 0;
    }
    
    // Free singly indirect blocks
    if (node.i_block[12] != 0) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect_blocks, node.i_block[12], 1);
        
        // Free blocks pointed to by the indirect block
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (indirect_blocks[i] == 0) continue;
            
            uint32_t block_bgd = indirect_blocks[i] / BLOCKS_PER_GROUP;
            if (block_bgd != last_bgd_idx) {
                write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
                read_blocks(&bitmap, bgdt.table[block_bgd].bg_block_bitmap, 1);
                last_bgd_idx = block_bgd;
            }
            
            uint32_t block_in_group = indirect_blocks[i] % BLOCKS_PER_GROUP;
            set_bitmap_bit(&bitmap, block_in_group, false);
            bgdt.table[block_bgd].bg_free_blocks_count++;
        }
        
        // Free the indirect block itself
        uint32_t ind_bgd = node.i_block[12] / BLOCKS_PER_GROUP;
        if (ind_bgd != last_bgd_idx) {
            write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
            read_blocks(&bitmap, bgdt.table[ind_bgd].bg_block_bitmap, 1);
            last_bgd_idx = ind_bgd;
        }
        
        uint32_t ind_block_in_group = node.i_block[12] % BLOCKS_PER_GROUP;
        set_bitmap_bit(&bitmap, ind_block_in_group, false);
        bgdt.table[ind_bgd].bg_free_blocks_count++;
        node.i_block[12] = 0;
    }
    
    // Free doubly indirect blocks
    if (node.i_block[13] != 0) {
        uint32_t dbl_indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(dbl_indirect_blocks, node.i_block[13], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (dbl_indirect_blocks[i] == 0) continue;
            
            uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(indirect_blocks, dbl_indirect_blocks[i], 1);
            
            // Free blocks pointed to by this indirect block
            for (uint32_t j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
                if (indirect_blocks[j] == 0) continue;
                
                uint32_t block_bgd = indirect_blocks[j] / BLOCKS_PER_GROUP;
                if (block_bgd != last_bgd_idx) {
                    write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
                    read_blocks(&bitmap, bgdt.table[block_bgd].bg_block_bitmap, 1);
                    last_bgd_idx = block_bgd;
                }
                
                uint32_t block_in_group = indirect_blocks[j] % BLOCKS_PER_GROUP;
                set_bitmap_bit(&bitmap, block_in_group, false);
                bgdt.table[block_bgd].bg_free_blocks_count++;
            }
            
            // Free the indirect block
            uint32_t ind_bgd = dbl_indirect_blocks[i] / BLOCKS_PER_GROUP;
            if (ind_bgd != last_bgd_idx) {
                write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
                read_blocks(&bitmap, bgdt.table[ind_bgd].bg_block_bitmap, 1);
                last_bgd_idx = ind_bgd;
            }
            
            uint32_t ind_block_in_group = dbl_indirect_blocks[i] % BLOCKS_PER_GROUP;
            set_bitmap_bit(&bitmap, ind_block_in_group, false);
            bgdt.table[ind_bgd].bg_free_blocks_count++;
        }
        
        // Free the doubly indirect block itself
        uint32_t dbl_bgd = node.i_block[13] / BLOCKS_PER_GROUP;
        if (dbl_bgd != last_bgd_idx) {
            write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
            read_blocks(&bitmap, bgdt.table[dbl_bgd].bg_block_bitmap, 1);
            last_bgd_idx = dbl_bgd;
        }
        
        uint32_t dbl_block_in_group = node.i_block[13] % BLOCKS_PER_GROUP;
        set_bitmap_bit(&bitmap, dbl_block_in_group, false);
        bgdt.table[dbl_bgd].bg_free_blocks_count++;
        node.i_block[13] = 0;
    }
    
    // Write the last bitmap we modified
    write_blocks(&bitmap, bgdt.table[last_bgd_idx].bg_block_bitmap, 1);
    
    // Mark inode as free in inode bitmap
    uint32_t inode_local = inode_to_local(inode_num);
    read_blocks(&bitmap, bgdt.table[bgd_index].bg_inode_bitmap, 1);
    set_bitmap_bit(&bitmap, inode_local, false);
    write_blocks(&bitmap, bgdt.table[bgd_index].bg_inode_bitmap, 1);
    
    // Update inode count in BGD
    bgdt.table[bgd_index].bg_free_inodes_count++;
    
    // If it was a directory, update directory count
    if ((node.i_mode & 0xF000) == EXT2_S_IFDIR) {
        bgdt.table[bgd_index].bg_used_dirs_count--;
    }
    
    // Update the superblock's counts
    sb.s_free_blocks_count++;
    sb.s_free_inodes_count++;
    
    // Write BGD and superblock
    write_blocks(&bgdt, 2, 1);
    write_blocks(&sb, 1, 1);
}


// Deallocate consecutive blocks
void deallocate_blocks(void *loc, uint32_t blocks) {
    if (!loc || blocks == 0) return;
    uint32_t start_block = *(uint32_t *)loc;

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t block = start_block + i;
        uint32_t bgd_index = block / BLOCKS_PER_GROUP;

        struct BlockBuffer bitmap;
        uint32_t bitmap_block = bgdt.table[bgd_index].bg_block_bitmap;

        read_blocks(&bitmap, bitmap_block, 1);

        uint32_t block_in_group = block % BLOCKS_PER_GROUP;
        set_bitmap_bit(&bitmap, block_in_group, false);

        write_blocks(&bitmap, bitmap_block, 1);
    }
}

// Recursive block deallocator
uint32_t deallocate_block(uint32_t *locations, uint32_t blocks,
                          struct BlockBuffer *bitmap, uint32_t depth,
                          uint32_t *last_bgd, bool bgd_loaded) {

    if (!locations || !bitmap || !last_bgd) return *last_bgd;
    for (uint32_t i = 0; i < blocks; i++) {
        if (locations[i] == 0) continue;

        if (depth > 0) {
            uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(indirect_blocks, locations[i], 1);

            deallocate_block(indirect_blocks, BLOCK_SIZE / sizeof(uint32_t),
                             bitmap, depth - 1, last_bgd, false);
        }

        uint32_t bgd_index = locations[i] / BLOCKS_PER_GROUP;
        uint32_t block_in_group = locations[i] % BLOCKS_PER_GROUP;

        if (!bgd_loaded || bgd_index != *last_bgd) {
            write_blocks(bitmap, bgdt.table[*last_bgd].bg_block_bitmap, 1);
            read_blocks(bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
            *last_bgd = bgd_index;
        }

        set_bitmap_bit(bitmap, block_in_group, false);
        locations[i] = 0;
    }

    if (!bgd_loaded) {
        write_blocks(bitmap, bgdt.table[*last_bgd].bg_block_bitmap, 1);
    }

    return *last_bgd;
}

// Block allocator for inode
void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t preferred_bgd) {
    uint32_t blocks_needed = (node->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t blocks_allocated = 0;
    char *data_ptr = (char *)ptr;

    // Direct blocks
    for (int i = 0; i < 12 && blocks_allocated < blocks_needed; i++) {
        if (node->i_block[i] == 0) {
            uint32_t block = find_free_anywhere(preferred_bgd);
            if (!block) return;

            node->i_block[i] = block;
            write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), block, 1);
            blocks_allocated++;
        }
    }

    // Singly indirect block
    if (blocks_allocated < blocks_needed) {
        if (node->i_block[12] == 0) {
            node->i_block[12] = find_free_anywhere(preferred_bgd);
            if (!node->i_block[12]) return;
            uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
            write_blocks(empty, node->i_block[12], 1);
        }

        uint32_t indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect, node->i_block[12], 1);

        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; i++) {
            if (indirect[i] == 0) {
                uint32_t block = find_free_anywhere(preferred_bgd);
                if (!block) break;

                indirect[i] = block;
                write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), block, 1);
                blocks_allocated++;
            }
        }

        write_blocks(indirect, node->i_block[12], 1);
    }

    // Doubly indirect block
    if (blocks_allocated < blocks_needed) {
        if (node->i_block[13] == 0) {
            node->i_block[13] = find_free_anywhere(preferred_bgd);
            if (!node->i_block[13]) return;

            uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
            write_blocks(empty, node->i_block[13], 1);
        }

        uint32_t doubly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(doubly_indirect, node->i_block[13], 1);

        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; i++) {
            if (doubly_indirect[i] == 0) {
                doubly_indirect[i] = find_free_anywhere(preferred_bgd);
                if (!doubly_indirect[i]) break;

                uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
                write_blocks(empty, doubly_indirect[i], 1);
            }

            uint32_t singly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(singly_indirect, doubly_indirect[i], 1);

            for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; j++) {
                if (singly_indirect[j] == 0) {
                    uint32_t block = find_free_anywhere(preferred_bgd);
                    if (!block) break;

                    singly_indirect[j] = block;
                    write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), block, 1);
                    blocks_allocated++;
                }
            }

            write_blocks(singly_indirect, doubly_indirect[i], 1);
        }

        write_blocks(doubly_indirect, node->i_block[13], 1);
    }

    node->i_blocks = blocks_allocated;
}

// Write inode and its bitmap to disk
void sync_node(struct EXT2Inode *node, uint32_t inode){
    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t local_index = inode_to_local(inode);
    struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[bgd_index];
    struct BlockBuffer block;

    // Inode bitmap
    read_blocks(block.buf, bgd->bg_inode_bitmap, 1);
    set_bitmap_bit(block.buf, inode, true);
    write_blocks(block.buf, bgd->bg_inode_bitmap, 1);

    // Inode table
    uint32_t inode_table_block = bgd->bg_inode_table + (local_index / INODES_PER_TABLE);
    uint32_t offset_in_block = (local_index % INODES_PER_TABLE) * INODE_SIZE;
    read_blocks(block.buf, inode_table_block, 1);
    memcpy(block.buf + offset_in_block, node, INODE_SIZE);
    write_blocks(block.buf, inode_table_block, 1);
}


/* =============================== HELPER ======================================== */

void load_inode_data(struct EXT2Inode* inode, void* buf, uint32_t buffer_size) {
    uint32_t total_read = 0;
    if (inode->i_size > buffer_size) return; // Not enough buffer
    if (inode->i_size == 0) return; // Empty file
    if (buf == NULL) return; // Invalid buffer

    // Direct blocks
    for (int i = 0; i < 12 && total_read < buffer_size; i++) {
        if (inode->i_block[i] == 0) return;
        total_read += load_block_data(inode->i_block[i], 0, buf + total_read, buffer_size - total_read);
    }
    // Indirect blocks
    if (inode->i_block[12] != 0 && total_read < buffer_size) {
        total_read += load_block_data(inode->i_block[12], 1, buf + total_read, buffer_size - total_read);
    } else { return; }
    // Doubly indirect blocks
    if (inode->i_block[13] != 0 && total_read < buffer_size) {
        total_read += load_block_data(inode->i_block[13], 2, buf + total_read, buffer_size - total_read);
    } else { return; }
    // Triply indirect blocks
    if (inode->i_block[14] != 0 && total_read < buffer_size) {
        total_read += load_block_data(inode->i_block[14], 3, buf + total_read, buffer_size - total_read);
    } else { return; }
}

uint32_t load_block_data(uint32_t block_number, uint8_t depth, void* buf, uint32_t buffer_size) {
    struct BlockBuffer block;
    uint32_t total_read = 0;
    uint32_t *block_ptr = (uint32_t *)block.buf;
    
    if (buffer_size <= 0) return 0;
    
    read_blocks(&block, block_number, 1);
    if (depth == 0) {
        uint32_t to_read = BLOCK_SIZE;
        if (buffer_size < BLOCK_SIZE) {
            to_read = buffer_size;
        }
        memcpy(buf, block.buf, to_read);
        total_read = to_read;
    } else {
        total_read = 0;
        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (block_ptr[i] == 0) continue;
            total_read += load_block_data(block_ptr[i], depth - 1, buf + total_read, buffer_size - total_read);
        }
    }

    return total_read;
}

// Helper to modify block bitmap
static void set_bitmap_bit(struct BlockBuffer *bitmap, uint32_t bit, bool value) {
    if (!bitmap || bit >= BLOCKS_PER_GROUP) return;
    uint32_t byte = bit / 8;
    uint8_t mask = 1 << (bit % 8);

    if (value) {
        bitmap->buf[byte] |= mask;
    } else {
        bitmap->buf[byte] &= ~mask;
    }
}

// Return true if bit is set
bool is_bitmap_set(struct BlockBuffer *bitmap, uint32_t bit) {
    if (bit > BLOCK_SIZE) return false; 
    uint32_t byte = bit / 8;
    uint8_t mask = 1 << (bit % 8);

    return (bitmap->buf[byte] & mask) != 0;
}

// Helper to find first free block in group
static uint32_t find_free_in_bgd(uint32_t bgd_index) {
    if (bgd_index >= GROUPS_COUNT) return 0;
    struct BlockBuffer bitmap;
    uint32_t bitmap_block = bgdt.table[bgd_index].bg_block_bitmap;
    
    read_blocks(&bitmap, bitmap_block, 1);

    for (uint32_t i = 0; i < BLOCKS_PER_GROUP; i++) {
        uint32_t byte = i / 8;
        uint8_t bit = i % 8;

        if (!(bitmap.buf[byte] & (1 << bit))) {
            set_bitmap_bit(&bitmap, i, true);
            write_blocks(&bitmap, bitmap_block, 1);
            return bgd_index * BLOCKS_PER_GROUP + i;
        }
    }

    return 0; // No space in this group
}

// Helper to find first free block in group, or anywhere else if one exists
uint32_t find_free_anywhere(uint32_t bgd_index) {
    // Try to find in bgd_index-th block group
    uint32_t block = find_free_in_bgd(bgd_index);
    if(block!=0) return block;

    // Try to find in other block groups
    for(uint32_t i=0;i<GROUPS_COUNT;i++){
        if(i==bgd_index) continue;
        block = find_free_in_bgd(i);
        if(block!=0) return block;
    }

    // Disk is full
    return 0;
}

// Check whether there's n blocks available to store data inside the disk
bool exists_n_free_blocks(int n){
    struct BlockBuffer bitmap;
    uint32_t total = 0;
    for(uint32_t i=0;i<GROUPS_COUNT;i++){
        uint32_t bitmap_block = bgdt.table[i].bg_block_bitmap;
        read_blocks(&bitmap, bitmap_block, 1);
        for (uint32_t i = 0; i < BLOCKS_PER_GROUP; i++) {
            uint32_t byte = i / 8;
            uint8_t bit = i % 8;
    
            if (!(bitmap.buf[byte] & (1 << bit))) {
                total++;
            }
            if(total==n){
                return true;
            }
        }
    }
    return false;
}

void read_inode(uint32_t inode_num, struct EXT2Inode *out) {
    uint32_t bgd_idx = inode_to_bgd(inode_num);
    uint32_t local_idx = inode_to_local(inode_num);
    struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[bgd_idx];

    // Locate the block that contain the specific inode and its offset within the block
    uint32_t inode_table_block = bgd->bg_inode_table + (local_idx / INODES_PER_TABLE);
    uint32_t offset = (local_idx % INODES_PER_TABLE) * INODE_SIZE;

    struct BlockBuffer b;
    read_blocks(&b, inode_table_block, 1);

    memcpy(out, b.buf + offset, INODE_SIZE);
}

bool find_directory_entry(struct EXT2Inode *dir_inode, char *name, uint8_t name_len, struct EXT2DirectoryEntry *result) {
    struct BlockBuffer block;

    for (int i = 0; i < 12; i++) {
        if (dir_inode->i_block[i] == 0) continue;
        read_blocks(&block, dir_inode->i_block[i], 1);

        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(block.buf + offset);
            if (entry->inode != 0 && entry->name_len == name_len &&
                memcmp((char *)(entry + 1), name, name_len) == 0) {
                memcpy(result, entry, sizeof(struct EXT2DirectoryEntry));
                return true;
            }
            offset += entry->rec_len;
        }
    }
    return false;
}

// add dir and its name to directory entry in inode_number
// TODO: Should there be any validation here (thus, refactoring this to an int for returning error code),
// or should this just assume that everything will happen perfectly (enough block, etc)
void add_directory_entry(struct EXT2DirectoryEntry dir, char *name, uint32_t inode_number){
    struct EXT2Inode source_inode;
    read_inode(inode_number, &source_inode);    
    
    struct EXT2DirectoryEntry *current_dir;
    struct BlockBuffer *current_dir_block;
    
    // iterate every direct blocks
    for(uint32_t i=0;i<12;i++){
        if(source_inode.i_block[i]==0){
            source_inode.i_block[i] = find_free_anywhere(inode_to_bgd(inode_number));
            sync_node(&source_inode, inode_number);
        }
        read_blocks(current_dir_block, source_inode.i_block[i], 1);
        current_dir = get_directory_entry(current_dir_block, 0);
        
        // check if there's a space for the new directory entry
        uint32_t offset = 0;
        while(current_dir->inode != 0 && offset <= BLOCK_SIZE){
            offset += current_dir->rec_len;
            current_dir = get_next_directory_entry(current_dir);
        }
        // found!
        if(BLOCK_SIZE - offset >= dir.rec_len){
            // Calculate where in the buffer to write the new entry
            uint8_t *entry_location = ((uint8_t *)current_dir_block) + offset;
            // Copy the EXT2DirectoryEntry structure
            memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
            // Copy the name right after the struct
            memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

            write_blocks(current_dir_block, source_inode.i_block[i], 1);
            return;
        }
    }

    // Iterate every singly indirect block
    // If singly indirect block is not used, create the indirect block, then write the directory entry
    if (source_inode.i_block[12] == 0) {
        source_inode.i_block[12] = find_free_anywhere(inode_to_bgd(inode_number)); // indirect block
        sync_node(&source_inode, inode_number);
        if (!source_inode.i_block[12]) return;

        uint32_t pointer_per_block = BLOCK_SIZE / sizeof(uint32_t);
        uint32_t indirect[pointer_per_block]; // table of pointer to block of directory entry
        indirect[0] = find_free_anywhere(inode_to_bgd(inode_number));

        // Calculate where in the buffer to write the new entry
        uint8_t *entry_location = ((uint8_t *)current_dir_block);
        // Copy the EXT2DirectoryEntry structure
        memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
        // Copy the name right after the struct
        memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

        write_blocks(indirect, source_inode.i_block[12], 1);
        write_blocks(current_dir_block, indirect[0], 1);
        return;
    } else{
        // else, read the singly indirect block
        uint32_t pointer_per_block = BLOCK_SIZE / sizeof(uint32_t);
        uint32_t indirect[pointer_per_block]; // table of pointer to block of directory entry
        read_blocks(indirect, source_inode.i_block[12], 1);

        for(uint32_t i=0; i<pointer_per_block; i++){
            // if there's an unallocated space, create the block for directory entry, then write it
            if(indirect[i] == 0){
                indirect[i] = find_free_anywhere(inode_to_bgd(inode_number));
                if (!indirect[i]) return;
                // Calculate where in the buffer to write the new entry
                uint8_t *entry_location = ((uint8_t *)current_dir_block);
                // Copy the EXT2DirectoryEntry structure
                memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
                // Copy the name right after the struct
                memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

                // update the singly indirect block, and write the directory entry block
                write_blocks(indirect, source_inode.i_block[12], 1);
                write_blocks(current_dir_block, indirect[i], 1);
                return;
            }

            // read the indirect block
            read_blocks(&current_dir_block, indirect[i], 1);
            current_dir = get_directory_entry(current_dir_block, 0);
        
            // check if there's a space for the new directory entry
            uint32_t offset = 0;
            while(current_dir->inode != 0 && offset <= BLOCK_SIZE){
                offset += current_dir->rec_len;
                current_dir = get_next_directory_entry(current_dir);
            }
            // found!
            if(BLOCK_SIZE - offset <= dir.rec_len){
                // Calculate where in the buffer to write the new entry
                uint8_t *entry_location = ((uint8_t *)current_dir_block) + offset;
                // Copy the EXT2DirectoryEntry structure
                memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
                // Copy the name right after the struct
                memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

                write_blocks(current_dir_block, indirect[i], 1);
                return;
            }  
        }
    }

    // Iterate every doubly indirect block
    // If doubly indirect block is not used, create the doubly indirect block, then write the directory entry
    if (source_inode.i_block[13] == 0) {
        if(!exists_n_free_blocks(3)) return;
        
        source_inode.i_block[13] = find_free_anywhere(inode_to_bgd(inode_number));
        sync_node(&source_inode, inode_number);
        if (!source_inode.i_block[13]) return;
        
        uint32_t pointer_per_block = BLOCK_SIZE / sizeof(uint32_t);
        uint32_t doubly_indirect[pointer_per_block];
        doubly_indirect[0] = find_free_anywhere(inode_to_bgd(inode_number));
        if (!doubly_indirect[0]) return;
        write_blocks(doubly_indirect, source_inode.i_block[13], 1);

        uint32_t singly_indirect[pointer_per_block]; // table of pointer to block of directory entry
        singly_indirect[0] = find_free_anywhere(inode_to_bgd(inode_number));
        if (!singly_indirect[0]) return;
        write_blocks(singly_indirect, doubly_indirect[0], 1);


        // Calculate where in the buffer to write the new entry
        uint8_t *entry_location = ((uint8_t *)current_dir_block);
        // Copy the EXT2DirectoryEntry structure
        memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
        // Copy the name right after the struct
        memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

        write_blocks(current_dir_block, singly_indirect[0], 1);
        return;
    } else{
        // else, read the doubly indirect block
        uint32_t pointer_per_block = BLOCK_SIZE / sizeof(uint32_t);
        uint32_t doubly_indirect[pointer_per_block];
        read_blocks(doubly_indirect, source_inode.i_block[13], 1);

        // iterate every entry on the doubly indirect block
        for(uint32_t i=0; i<pointer_per_block; i++){
            // if an entry isn't used, allocate!
            if(doubly_indirect[i] == 0){
                doubly_indirect[i] = find_free_anywhere(inode_to_bgd(inode_number));
                if (!doubly_indirect[i]) return;
                
                uint32_t singly_indirect[pointer_per_block];
                singly_indirect[0] = find_free_anywhere(inode_to_bgd(inode_number));
                
                // Calculate where in the buffer to write the new entry
                uint8_t *entry_location = ((uint8_t *)current_dir_block);
                // Copy the EXT2DirectoryEntry structure
                memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
                // Copy the name right after the struct
                memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

                // update the doubly indirect block, singly indirect block, and write the directory entry block
                write_blocks(doubly_indirect, source_inode.i_block[13], 1);
                write_blocks(singly_indirect, doubly_indirect[i], 1);
                write_blocks(current_dir_block, singly_indirect[0], 1);
                return;
            }
            
            // else, read the singly indirect block
            uint32_t pointer_per_block = BLOCK_SIZE / sizeof(uint32_t);
            uint32_t singly_indirect[pointer_per_block]; // table of pointer to block of directory entry
            read_blocks(singly_indirect, doubly_indirect[i], 1);

            for(uint32_t i=0; i<pointer_per_block; i++){
                // if there's an unallocated space, create the block for directory entry, then write it
                if(singly_indirect[i] == 0){
                    singly_indirect[i] = find_free_anywhere(inode_to_bgd(inode_number));
                    if (!singly_indirect[i]) return;
                    // Calculate where in the buffer to write the new entry
                    uint8_t *entry_location = ((uint8_t *)current_dir_block);
                    // Copy the EXT2DirectoryEntry structure
                    memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
                    // Copy the name right after the struct
                    memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

                    // update the singly indirect block, and write the directory entry block
                    write_blocks(singly_indirect, doubly_indirect[i], 1);
                    write_blocks(current_dir_block, singly_indirect[i], 1);
                    return;
                }

                read_blocks(&current_dir_block, singly_indirect[i], 1);
                current_dir = get_directory_entry(current_dir_block, 0);
        
                // check if there's a space for the new directory entry
                uint32_t offset = 0;
                while(current_dir->inode != 0  && offset <= BLOCK_SIZE){
                    offset += current_dir->rec_len;
                    current_dir = get_next_directory_entry(current_dir);
                }
                // found!
                if(BLOCK_SIZE - offset <= dir.rec_len){
                    // Calculate where in the buffer to write the new entry
                    uint8_t *entry_location = ((uint8_t *)current_dir_block);
                    // Copy the EXT2DirectoryEntry structure
                    memcpy(entry_location, &dir, sizeof(struct EXT2DirectoryEntry));
                    // Copy the name right after the struct
                    memcpy(entry_location + sizeof(struct EXT2DirectoryEntry), name, dir.name_len);

                    write_blocks(&current_dir_block, singly_indirect[i], 1);
                    return;
                }  
            }
        }
    }
}

bool mark_entry_in_block(uint32_t block_number, struct EXT2DirectoryEntry *entry, struct EXT2DriverRequest *request) {
    struct BlockBuffer block;
    read_blocks(&block, block_number, 1);
    uint32_t offset = 0;
    bool found = false;

    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *current = (struct EXT2DirectoryEntry *)(block.buf + offset);
        
        // Check if this entry matches the one we're trying to delete
        if (current->inode == entry->inode && 
            current->name_len == request->name_len &&
            memcmp((char *)(current + 1), request->name, request->name_len) == 0) {
            
            if (offset > 0) {
                // Merge with previous entry (if not the first entry in the block)
                struct EXT2DirectoryEntry *prev = (struct EXT2DirectoryEntry *)(block.buf + offset - current->rec_len);
                prev->rec_len += current->rec_len;
            } else {
                // If this is the first entry, just set inode to 0 to mark as free
                current->inode = 0;
            }

            // Write back the modified block with the updated entry
            write_blocks(&block, block_number, 1);
            found = true;
            break;
        }

        offset += current->rec_len;
    }

    return found;
}

<<<<<<< HEAD
=======
void updateBGDTInode(uint32_t inode_number, bool is_update){
    uint32_t inode_location = inode_to_bgd(inode_number);
    
    if(is_update) bgdt.table[inode_location].bg_free_inodes_count--; // update
    else bgdt.table[inode_location].bg_free_inodes_count++; // delete
    
    struct BlockBuffer b;
    memset(b.buf, 0, BLOCK_SIZE);
    memcpy(b.buf, &bgdt, sizeof(struct EXT2BlockGroupDescriptorTable));
    for (uint8_t group=0;group<GROUPS_COUNT;group++) {
        if (group == 0) {
            write_blocks(b.buf, 2, 1);
        }
        else {
            write_blocks(b.buf, group * BLOCKS_PER_GROUP, 1);
        }
    }
}
>>>>>>> 98c7dc0 (feat: helper to update bgdt free inode counter)
