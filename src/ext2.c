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
            memcpy(&bgdt.table[0], &bgd_template, sizeof(struct EXT2BlockGroupDescriptor));
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
            memcpy(&bgdt.table[i], &bgd_template, sizeof(struct EXT2BlockGroupDescriptor));
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
    set_bitmap_bit(&b, 0, true); // Mark inode 0 as unused (invalid inode)
    set_bitmap_bit(&b, 1, true); // Mark inode 1 as used (unclear inode). Inode 2 is synced later
    write_blocks(b.buf, bgdt.table[0].bg_inode_bitmap, 1);
    set_bitmap_bit(&b, 0, false); // After group 0, all local inodes are free
    set_bitmap_bit(&b, 1, false);
    for (uint8_t group=1;group<GROUPS_COUNT;group++) {
        write_blocks(b.buf, bgdt.table[group].bg_inode_bitmap, 1);
    }
    // Block bitmap
    for (uint8_t i=0;i<initial_group_blocks;i++){
        set_bitmap_bit(&b, i, true);
    }
    write_blocks(b.buf, bgdt.table[0].bg_block_bitmap, 1);
    set_bitmap_bit(&b, initial_group_blocks-1, false); // Without superblock and signature
    set_bitmap_bit(&b, initial_group_blocks-2, false);
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
    bgdt.table[0].bg_used_dirs_count += 1;
    update_bgdt();
    
    sb.s_free_inodes_count -= 1;

    init_directory_table(&root_inode, 2, 2);
    sync_node(&root_inode, 2);
}

void initialize_filesystem_ext2(void){
    struct BlockBuffer tmp;
    if(is_empty_storage()){
        create_ext2();
    } else{
        read_blocks(tmp.buf, 1, 1); // Read Superblock
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
    if (prequest == NULL || prequest->buffer_size == 0)
    return -1;

    // Validate parent inode
    struct EXT2Inode parent_inode;
    read_inode(prequest->parent_inode, &parent_inode);
    if ((parent_inode.i_mode & 0xF000) != 0x4000) return 3; 

    // Validate if parent inode has child
    struct EXT2DirectoryEntry entry;
    bool found = find_directory_entry(prequest, &parent_inode, &entry); 
    if (!found) return 2;

    // Validate child is a directory
    struct EXT2Inode child_inode;
    read_inode(entry.inode, &child_inode);
    if ((child_inode.i_mode & 0xF000) != 0x4000) return 1;
    
    prequest->buffer_size = BLOCK_SIZE * 2;
    // Copy blocks of directory to buf
    load_inode_data(&child_inode, prequest->buf, prequest->buffer_size);

    return 0;
}

int8_t read(struct EXT2DriverRequest request){
    struct EXT2Inode inode;
    struct EXT2DirectoryEntry entry;

    // Unknown / invalid input
    if (request.buffer_size == 0 || request.name == NULL || request.name_len == 0) {
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
    if (!find_directory_entry(&request, &inode, &entry)) {
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

    // Validate whether there's enough blocks to hold the data
    uint32_t blocks_needed = (request->buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(!exists_n_free_blocks(blocks_needed)) return -1;

    // Validate whether there's an available inode
    uint32_t new_inode_number = allocate_node();
    if(new_inode_number == 0) return -1;
    
    struct EXT2DirectoryEntry new_entry = {0};
    struct EXT2Inode new_inode = {0};
    // Write file 
    if (!request->is_directory){
        // Set permission: 0x8000 untuk file reguler | 0x0200 = user write permission
        new_inode.i_mode = 0x8000 | 0x0200;
        // Ukuran file
        new_inode.i_size = request->buffer_size;
        // Jumlah blok (dalam 512-byte blocks, EXT2 i_blocks menyimpan ukuran dalam 512-byte block)
        new_inode.i_blocks = blocks_needed;
        
        // Buat entri direktori untuk file baru
        new_entry.inode = new_inode_number;
        new_entry.rec_len = 0;
        new_entry.name_len = request->name_len;
        new_entry.file_type = 1; // 1 = file reguler
        
    }
    // Write directory
    else {
        // Allocate corresponding Inode for the new directory
        new_inode.i_mode = 0x4000;
        new_inode.i_size = BLOCK_SIZE;
        new_inode.i_blocks = 1;
        
        new_entry.inode = new_inode_number;
        new_entry.name_len = request->name_len;
        new_entry.file_type = 2;
    }
    // Entry insertion
    int8_t entry_addition = add_directory_entry(request, &new_entry, &parent_inode);
    if (entry_addition == 2) {
        return 1;
    } else if (entry_addition != 0) {
        return -1; // Unknown error
    }
    
    // Write data
    if (!request->is_directory) {
        allocate_node_blocks(request->buf, &new_inode, inode_to_bgd(request->parent_inode));
    } else {
        init_directory_table(&new_inode, new_inode_number, request->parent_inode);
    }
    
    // Update metadata
    sync_node(&new_inode, new_inode_number);
    update_bgdt();

    return 0;
}

int8_t delete(struct EXT2DriverRequest request) {
    if (request.name == NULL || request.name_len == 0) {
        return -1; // Invalid request
    }

    struct EXT2Inode parent_inode;
    read_inode(request.parent_inode, &parent_inode);
    if ((parent_inode.i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR) {
        return 3; // Parent is not a directory
    }

    // Attempt to find directory entry in parent
    // Outcome: not found, found but an unempty directory, or found
    // When found, delete the entry by modifying the entry of the previous entry
    uint32_t deleted_inode_number = 0;
    int8_t entry_deletion = delete_directory_entry(&request, &parent_inode, &deleted_inode_number);
    if (entry_deletion == 1) {
        return 1; // Directory not found
    } else if (entry_deletion == 2) {
        return 2; // Unable to delete non-empty directory
    } else if (entry_deletion != 0) {
        return -1; // Unknown error
    }

    // Delete inode, BGD, and superblock
    // Deallocate the inode and its blocks
    deallocate_node(deleted_inode_number);
    update_bgdt();
    // Superblock To do...
}

/* =============================== MEMORY ==========================================*/

// Return inode number
// This function does not have any side effects (memory/disk modification)
uint32_t allocate_node(void){
    // Check each group's inode bitmap
    struct BlockBuffer bitmap;
    uint32_t group = GROUPS_COUNT;  // Initial invalid group
    // read_blocks(bitmap.buf, bgdt.table[group].bg_inode_bitmap, 1);
    for (uint32_t inode_number=1; inode_number<=(GROUPS_COUNT * INODES_PER_GROUP); inode_number++){
        // Check if the correct group bitmap is loaded
        if (group != inode_to_bgd(inode_number)) {
            group = inode_to_bgd(inode_number);
            read_blocks(bitmap.buf, bgdt.table[group].bg_inode_bitmap, 1);
        }
        
        if (!is_bitmap_set(&bitmap, inode_to_local(inode_number))) {
            return inode_number;
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
    if (inode < 1 || inode > INODES_PER_GROUP * GROUPS_COUNT) { return; }

    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t local_index = inode_to_local(inode);
    struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[bgd_index];
    struct BlockBuffer block;

    // Inode bitmap
    read_blocks(block.buf, bgd->bg_inode_bitmap, 1);
    set_bitmap_bit(&block, local_index, true);
    write_blocks(block.buf, bgd->bg_inode_bitmap, 1);

    // Inode table
    uint32_t inode_table_block = bgd->bg_inode_table + (local_index / INODES_PER_TABLE);
    uint32_t offset_in_block = (local_index % INODES_PER_TABLE) * INODE_SIZE;
    read_blocks(block.buf, inode_table_block, 1);
    memcpy(block.buf + offset_in_block, node, INODE_SIZE);
    write_blocks(block.buf, inode_table_block, 1);
    bgd->bg_free_inodes_count--;
    sb.s_free_inodes_count--;
}


/* =============================== HELPER ======================================== */

void load_inode_data(struct EXT2Inode* inode, void* buf, uint32_t buffer_size) {
    uint32_t total_read = 0;
    if (inode->i_size > buffer_size) return; // Not enough buffer
    if (inode->i_size == 0) return; // Empty file
    // if (buf == NULL) return; // Invalid buffer

    // Direct blocks
    for (uint8_t i = 0; i < 12 && total_read < buffer_size; i++) {
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
        for (uint8_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
            if (block_ptr[i] == 0) continue;
            total_read += load_block_data(block_ptr[i], depth - 1, buf + total_read, buffer_size - total_read);
        }
    }

    return total_read;
}

uint32_t load_inode_next_block(struct EXT2Inode* inode, void* buf, uint32_t block_offset, struct BlockBuffer* indirect_pointers) {
    // Implemented for sequential forward reading
    uint32_t block_to_read = block_offset + 1;
    if (block_to_read < DIRECT_BLOCK_COUNT) {
        if (inode->i_block[block_to_read] == 0) return 0;
        read_blocks(buf, inode->i_block[block_to_read], 1);
        return inode->i_block[block_to_read];
    } else {
        uint32_t reduced_block_offset = block_to_read;
        if (block_to_read < DIRECT_BLOCK_COUNT + SINGLY_INDIRECT_BLOCK_COUNT) {
            reduced_block_offset -= DIRECT_BLOCK_COUNT;
            read_blocks(indirect_pointers, inode->i_block[12], 1);
            return load_indirect_block(inode, buf, reduced_block_offset, indirect_pointers, 1);
        } else if (block_to_read < DIRECT_BLOCK_COUNT + SINGLY_INDIRECT_BLOCK_COUNT + DOUBLY_INDIRECT_BLOCK_COUNT) {
            reduced_block_offset -= DIRECT_BLOCK_COUNT + SINGLY_INDIRECT_BLOCK_COUNT;
            read_blocks(indirect_pointers+1, inode->i_block[13], 1);
            return load_indirect_block(inode, buf, reduced_block_offset, indirect_pointers, 2);
        } else if (block_to_read < DIRECT_BLOCK_COUNT + SINGLY_INDIRECT_BLOCK_COUNT + DOUBLY_INDIRECT_BLOCK_COUNT + TRIPLY_INDIRECT_BLOCK_COUNT) {
            reduced_block_offset -= DIRECT_BLOCK_COUNT + SINGLY_INDIRECT_BLOCK_COUNT + DOUBLY_INDIRECT_BLOCK_COUNT;
            read_blocks(indirect_pointers+2, inode->i_block[14], 1);
            return load_indirect_block(inode, buf, reduced_block_offset, indirect_pointers, 3);
        }
    }
    return 0;
}

uint32_t load_indirect_block(struct EXT2Inode *inode, void *buf, uint32_t reduced_block_offset, struct BlockBuffer* indirect_pointers, uint8_t depth) {
    if (depth == 1) {
        uint32_t *pointers = (uint32_t *) indirect_pointers->buf;
        uint32_t block_to_read = pointers[reduced_block_offset];
        if (block_to_read == 0) return 0;
        read_blocks(buf, block_to_read, 1);
        return block_to_read;
    } else if (depth == 2 || depth == 3) {
        uint32_t *pointers_to_pointers = (uint32_t *) ((indirect_pointers+(depth-1))->buf);
        uint32_t filter = 1;
        for (uint8_t i=0;i<depth-1;i++) { filter *= SINGLY_INDIRECT_BLOCK_COUNT; }
        uint32_t pointers_to_read = pointers_to_pointers[reduced_block_offset / filter];
        if (reduced_block_offset % filter == 0) {
            // To reduce unnecessary read from disk into indirect_pointers when already loaded
            read_blocks(indirect_pointers+(depth-2), pointers_to_read, 1);
        }
        return load_indirect_block(inode, buf, reduced_block_offset % filter, indirect_pointers, depth-1);
    }
    return 0;
}

// Helper to modify block bitmap
void set_bitmap_bit(struct BlockBuffer *bitmap, uint32_t bit, bool value) {
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

uint32_t allocate_additional_blocks(struct EXT2Inode *node, uint32_t preferred_bgd, uint32_t blocks_needed) {
    return 0; // Not implemented yet
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode){
    struct BlockBuffer buf = {0};

    // Create self .
    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)buf.buf;
    dot->inode = inode;
    dot->name_len = 1;
    dot->file_type = 2; // 2 = directory
    *((char *)(dot + 1)) = '.';
    dot->rec_len = get_entry_len(dot);

    // Create .. (parent)
    struct EXT2DirectoryEntry *dotdot = (struct EXT2DirectoryEntry *)((uint8_t *)dot + dot->rec_len);
    dotdot->inode = parent_inode;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    *((char *)(dotdot + 1)) = '.';
    *((char *)(dotdot + 1) + 1) = '.';
    dotdot->rec_len = 0; 

    // Allocate new block for this directory
    allocate_node_blocks(buf.buf, node, inode_to_bgd(inode));
}

uint16_t get_entry_len(struct EXT2DirectoryEntry *entry) {
    uint16_t len = sizeof(struct EXT2DirectoryEntry) + entry->name_len;
    len = (len + 3) & ~3; // Align to 4 bytes
    return len;
}

bool find_directory_entry(struct EXT2DriverRequest *request, struct EXT2Inode *parent_inode, struct EXT2DirectoryEntry *result) {
    struct BlockBuffer directory_entries[1];
    struct BlockBuffer indirect_pointers[3];
    uint32_t current_loaded_block = parent_inode->i_block[0];
    uint32_t block_count = 0;
    memset(directory_entries[0].buf, 0x0, BLOCK_SIZE);
    memset(indirect_pointers[0].buf, 0x0, 3 * BLOCK_SIZE);
    read_blocks(directory_entries[0].buf, current_loaded_block, 1);
    struct EXT2DirectoryEntry *entry = get_directory_entry(&directory_entries[0], 0);
    uint16_t offset = 0;
    uint16_t current_entry_len;

    for (;;) {  // Iterate linked list of entries
        entry = (struct EXT2DirectoryEntry *)(directory_entries[0].buf + offset);
        
        if (correct_request_entry(entry, request)) {
            memcpy(result, entry, sizeof(struct EXT2DirectoryEntry));
            return true;
        }
        
        if (entry->rec_len == 0) {
            return false;
        }
        
        offset += entry->rec_len;
        while (offset > BLOCK_SIZE) {
            // Load new block(s)
            offset -=  BLOCK_SIZE;
            current_loaded_block = load_inode_next_block(parent_inode, directory_entries[0].buf, block_count, indirect_pointers);
            block_count++;
            if (current_loaded_block==0) return false; // Points into an entry but run out of blocks
        }
    }

    return false;
}

int8_t add_directory_entry(struct EXT2DriverRequest *request, struct EXT2DirectoryEntry *dir, struct EXT2Inode *parent_inode) {
    struct BlockBuffer directory_entries[1];
    struct BlockBuffer indirect_pointers[3];
    uint32_t current_loaded_block = parent_inode->i_block[0];
    uint32_t block_count = 0;
    memset(directory_entries[0].buf, 0x0, BLOCK_SIZE);
    memset(indirect_pointers[0].buf, 0x0, 3 * BLOCK_SIZE);
    read_blocks(directory_entries[0].buf, current_loaded_block, 1);
    struct EXT2DirectoryEntry *entry = get_directory_entry(&directory_entries[0], 0);
    uint16_t offset = 0;
    uint16_t new_entry_len = get_entry_len(dir);
    uint16_t current_entry_len;

    dir->rec_len = 0;
    for (;;) {  // Iterate linked list of entries
        offset += entry->rec_len;
        while (offset > BLOCK_SIZE) {
            // Load new block
            offset -=  BLOCK_SIZE;
            current_loaded_block = load_inode_next_block(parent_inode, directory_entries[0].buf, block_count, indirect_pointers);
            block_count++;
            if (current_loaded_block==0) return -1; // Points into an entry but run out of blocks
        }
        entry = (struct EXT2DirectoryEntry *)(directory_entries[0].buf + offset);

        if (correct_request_entry(entry, request)) {
            return 2; // Entry already exists
        }

        current_entry_len = get_entry_len(entry);
        if (entry->rec_len == 0 && offset + current_entry_len + new_entry_len > BLOCK_SIZE) {
            // End of entries but need to allocate new block
            uint32_t new_block = allocate_additional_blocks(parent_inode, parent_inode->i_block[0] / BLOCKS_PER_GROUP, 1);
            if (new_block == 0) { return 2; } // No free block available
            entry->rec_len = BLOCK_SIZE - offset;
            memcpy(directory_entries[0].buf + offset, entry, sizeof(struct EXT2DirectoryEntry));
            write_blocks(directory_entries[0].buf, current_loaded_block, 1);

            // New block. Put entry in zero offset
            memset(directory_entries[0].buf, 0x0, BLOCK_SIZE);
            memcpy(directory_entries[0].buf, dir, sizeof(struct EXT2DirectoryEntry));
            memcpy(directory_entries[0].buf + sizeof(struct EXT2DirectoryEntry), request->name, dir->name_len);
            write_blocks(directory_entries[0].buf, new_block, 1);
            return 1;
        }
        if (entry->rec_len == 0) {
            // End of entries with enough space or feasible gaps between entries
            if (entry->rec_len != 0) {  // Adjust for inserting an entry in a gap
                dir->rec_len = entry->rec_len - current_entry_len;
            }
            entry->rec_len = current_entry_len;
            memcpy(directory_entries[0].buf + offset, entry, sizeof(struct EXT2DirectoryEntry));
            
            memcpy(directory_entries[0].buf + offset + current_entry_len, dir, sizeof(struct EXT2DirectoryEntry));
            memcpy(directory_entries[0].buf + offset + current_entry_len + sizeof(struct EXT2DirectoryEntry), request->name, dir->name_len);
            write_blocks(directory_entries[0].buf, current_loaded_block, 1);
            return 0;
        }
    }
    return -1;
}

int8_t delete_directory_entry(struct EXT2DriverRequest* delete_request, struct EXT2Inode* parent_inode, uint32_t* deleted_inode_number) {
    struct BlockBuffer directory_entries[1];
    struct BlockBuffer indirect_pointers[3];
    uint32_t prev_loaded_block = parent_inode->i_block[0];      // Guaranteed to be not empty (first two entries)
    uint32_t current_loaded_block = parent_inode->i_block[0];
    uint32_t block_count = 0;
    memset(directory_entries[0].buf, 0x0, BLOCK_SIZE);
    memset(indirect_pointers[0].buf, 0x0, 3 * BLOCK_SIZE);
    read_blocks(&directory_entries[0], current_loaded_block, 1);
    struct EXT2DirectoryEntry *prev_entry = get_directory_entry(&directory_entries[0], 0);
    struct EXT2DirectoryEntry *entry = get_next_directory_entry(prev_entry);
    uint8_t offset = prev_entry->rec_len;   // Current entry offset in block

    for (;;) {  // Iterate linked list of entries
        for (;;) {  // Check entries of the current blocks
            if (offset + entry->rec_len > BLOCK_SIZE) {
                offset = offset + entry->rec_len - BLOCK_SIZE;
                break;
            }

            if (correct_request_entry(entry, delete_request)) {
                if (entry->file_type == EXT2_FT_DIR && !is_directory_empty(entry->inode)) {
                    return 2;   // Unable to delete non-empty directory
                }
                *deleted_inode_number = entry->inode;
                entry->inode = 0;   // For safe measure
                write_blocks(&directory_entries[0], current_loaded_block, 1);
                read_blocks(&directory_entries[0], prev_loaded_block, 1);
                prev_entry->rec_len += entry->rec_len;  // Point to the same offset in the stack as was previously loaded
                write_blocks(&directory_entries[0], prev_loaded_block, 1);
                return 0;
            }
            if (entry->rec_len==0) {
                return 1;
            }
            prev_loaded_block = current_loaded_block;
            prev_entry = entry;
            entry = get_next_directory_entry(entry);
            offset += prev_entry->rec_len;
        }

        prev_loaded_block = current_loaded_block;
        current_loaded_block = load_inode_next_block(parent_inode, directory_entries[0].buf, block_count, &indirect_pointers[0]);
        block_count++;
        if (current_loaded_block==0) {
            return 1;
        }
        prev_entry = entry;
        entry = (struct EXT2DirectoryEntry *)(directory_entries[0].buf + offset);
    }
    return -1;
}

bool correct_request_entry(struct EXT2DirectoryEntry *entry, struct EXT2DriverRequest *request) {
    if (entry->inode == 0) {
        return false;
    }
    if (entry->name_len != request->name_len) {
        return false;
    }
    if (memcmp(get_entry_name(entry), request->name, request->name_len) != 0) {
        return false;
    }
    if (request->is_directory && (entry->file_type != EXT2_FT_DIR)) {
        return false;
    }
    if (!request->is_directory && (entry->file_type == EXT2_FT_DIR)) {
        return false;
    }
    return true;
}

void update_bgdt(void){
    struct BlockBuffer b;
    memset(b.buf, 0, BLOCK_SIZE);
    memcpy(b.buf, &bgdt, sizeof(struct EXT2BlockGroupDescriptorTable));
    for (uint8_t group = 0; group < GROUPS_COUNT; group++) {
        if (group == 0) {
            write_blocks(&b, 2, 1);
        } else {
            write_blocks(&b, group * BLOCKS_PER_GROUP, 1);
        }
    }
}

/*
0: Exists
1: Exists file
2: Not found
3: Parent invalid
-1: Unknown error
 */
int8_t exist_ext2(uint32_t parent_inode, const char *path, uint32_t *res_inode) {
    char temp_path[256];
    size_t len = 0;
    while (path[len] != '\0') len++;

    if (len >= sizeof(temp_path)) return -1; // too long
    memcpy(temp_path, path, len); // no null terminator needed

    size_t i = 0;

    while (i < len) {
        char token[64];
        for (int i=0; i < 64; i++){
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
    char dest_parent[256], dest_leaf[64];
    split_path(copy_request->destination, dest_parent, dest_leaf);
    char src_parent[256], src_leaf[64];
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

    // 2. Write to destination
    struct EXT2DriverRequest write_req = {
        .buf = buffer,
        .name = (char *)final_name,
        .name_len = final_name_len,
        .parent_inode = dest_inode,
        .buffer_size = read_req.buffer_size,
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
    
    char src_parent[256], src_leaf[64];
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
        char* cur_name = get_entry_name(entry);
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
            uint8_t entryBuffer[BLOCK_SIZE * 16];
            struct EXT2DriverRequest entry_req = {
                .buf = buffer,
                .name = cur_name,
                .name_len = entry->name_len,
                .parent_inode = src_dir_inode,
                .buffer_size = BLOCK_SIZE * 16,
                .is_directory = false
            };
            int8_t read_res = read(entry_req);

            // WRITE
            struct EXT2DriverRequest write_req = {
                .buf = buffer,
                .name = cur_name,
                .name_len = entry->name_len,
                .parent_inode = dest_dir_inode,
                .buffer_size = BLOCK_SIZE * 16,
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
        char* cur_name = get_entry_name(dir_entry);
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
    for (size_t i = 0; i < slash_before_last_file && i < 255; i++) {
        parent_out[i] = path[i];
    }
    parent_out[slash_before_last_file] = '\0';

    // Copy leaf
    size_t j = 0;
    for (size_t i = slash_before_last_file; i < len && j < 63; i++) {
        leaf_out[j++] = path[i];
    }
    leaf_out[j] = '\0';

    if (slash_before_last_file == 0){
        parent_out[0] = '.';
        parent_out[1] = '/';
        parent_out[2] = '\0';
    }
}