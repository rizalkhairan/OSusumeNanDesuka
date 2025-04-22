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
        char* name = (char*)(dir_entry + sizeof(struct EXT2DirectoryEntry));
        return name;
    }
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset){
    struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)((uint8_t*)ptr + offset);
    return entry;
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
    dotdot->rec_len = BLOCK_SIZE - dot->rec_len; 
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    *((char *)(dotdot + 1)) = '.';
    *((char *)(dotdot + 1) + 1) = '.';

    // Allocate new block for this directory
    allocate_node_blocks(dot, node, inode_to_bgd(inode));
    allocate_node_blocks(dotdot, node, inode_to_bgd(inode));
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
    // struct EXT2Superblock tes = {
    //     .s_inodes_count = 99,
    //     .s_blocks_count = 99,
    //     .s_r_blocks_count = 99,
    //     .s_free_blocks_count = 99,
    //     .s_free_inodes_count = 99,
    //     .s_first_data_block = 99,
    //     .s_first_ino = 99,
    //     .s_blocks_per_group = 99,
    //     .s_frags_per_group = 99,
    //     .s_inodes_per_group = 99,
    //     .s_magic = 99,
    //     .s_prealloc_blocks = 99,
    //     .s_prealloc_dir_blocks = 99,
    // };
    
    struct BlockBuffer b;
    memcpy(b.buf, fs_signature, BLOCK_SIZE);
    write_blocks(&b, 0, 1);  // Signature
    write_blocks(&sb, 1, 1); // Superblock
    for(int i=0;i<GROUPS_COUNT;i++){ // BGDs, TODO: initiate only the first one or all of them?
        struct EXT2BlockGroupDescriptor bgd_template = {
            .bg_block_bitmap = 3 + (i*BLOCKS_PER_GROUP),
            .bg_inode_bitmap = 4 + (i*BLOCKS_PER_GROUP),
            .bg_inode_table = 5 + (i*BLOCKS_PER_GROUP),
            .bg_free_blocks_count = BLOCKS_PER_GROUP-3,
            .bg_free_inodes_count = INODES_PER_GROUP,
            .bg_used_dirs_count = 4,
            .bg_pad = 0,
            .bg_reserved = {0,0,0}
        };
        bgdt.table[i] = bgd_template;
    }
    write_blocks(&bgdt, 2, 1);

    // create root directory
    struct EXT2Inode root_inode = {
        .i_mode = 0x4000, // Directory
        .i_size = 0,
        .i_blocks = 2, // 2 sectors = 1 block
        .i_block[0] = 2,
        .i_block[1] = 2
    };
    sync_node(&root_inode, 2);
    bgdt.table[0].bg_free_inodes_count -= 1;
    bgdt.table[0].bg_used_dirs_count += 1;
    sb.s_free_inodes_count -= 1;
}

void initialize_filesystem_ext2(void){
    if(is_empty_storage()){
        create_ext2();
    } else{
        read_blocks(&sb, 1, 1); // Read Superblock
        struct BlockBuffer bgd_block_buf;
        int bgd_block = 2;
        read_blocks(&bgd_block_buf.buf, bgd_block, 1);
        for(int i=0;i<GROUPS_COUNT;i++){ // Read BGDs, TODO: Read only the first one or all of them?
            memcpy(&bgdt.table[i], bgd_block_buf.buf + i * sizeof(struct EXT2BlockGroupDescriptor), sizeof(struct EXT2BlockGroupDescriptor));
        }
    }
}

bool is_directory_empty(uint32_t inode){
    // Finding current inode from inode number
    struct EXT2Inode *currentInode;
    read_inode(inode, currentInode);

    for (int i = 0; i < 12; i++) {
        if (currentInode->i_block[i] == 0) continue;

        struct BlockBuffer buf;
        read_blocks(&buf, currentInode->i_block[i], 1);

        uint32_t offset = 0;

        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(buf.buf + offset);

            if (entry->inode != 0) {
                char *name = (char *)(entry + 1); 

                // Check if it's not "." or ".."
                if (!(entry->name_len == 1 && name[0] == '.') &&
                    !(entry->name_len == 2 && name[0] == '.' && name[1] == '.')) {
                    return false;  
                }
            }

            if (entry->rec_len == 0) break; 
            offset += entry->rec_len;
        }
    }

    return true;

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
    uint32_t total_read = 0;
    struct BlockBuffer block;
    for (int i = 0; i < 12 && total_read < prequest->buffer_size; i++) {
        if (child_inode.i_block[i] == 0) break;

        read_blocks(&block, child_inode.i_block[i], 1);
        uint32_t to_copy = BLOCK_SIZE;
        if (total_read + to_copy > prequest->buffer_size)
            to_copy = prequest->buffer_size - total_read;

        memcpy((uint8_t *)prequest->buf + total_read, block.buf, to_copy);
        total_read += to_copy;
    }

    return 0;
}

int8_t read(struct EXT2DriverRequest request){

}

int8_t write(struct EXT2DriverRequest *request){
    // unknown/invalid input
    if (request == NULL || request->buf == NULL || request->buffer_size == 0)
    return -1;

    // Validate parent inode
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);
    if ((parent_inode.i_mode & 0xF000) != 0x4000) return 2;

    // Validate if file/folder already exists
    struct EXT2DirectoryEntry *ptr = get_directory_entry(parent_inode.i_block[2], 0);
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

        if (new_ptr == NULL){
            break;
        }
    }

    uint32_t new_inode_number = allocate_node();
    struct EXT2Inode *new_inode;    
    // read_inode(new_inode_number, new_inode);
    // Write file 
    if (!request->is_directory){
        allocate_node_blocks(request->buf, new_inode, inode_to_bgd(request->parent_inode));
        new_inode->i_mode = 0x8000 | (1 << 9);  // Temporary permission
        new_inode->i_size = request->buffer_size;
        ptr;
    }

    // Write directory
    else {
        struct BlockBuffer block;
        new_inode->i_block[0] = new_inode_number;
        new_inode->i_block[1] = request->parent_inode;

        struct EXT2DirectoryEntry new_entry;
        new_entry.inode = new_inode_number;
        new_entry.rec_len = 0;
        new_entry.name_len = request->name_len;
        new_entry.file_type = 2;

        memcpy(block.buf, &new_entry, sizeof(struct EXT2DirectoryEntry));
        memcpy(block.buf + sizeof(struct EXT2DirectoryEntry), request->name, request->name_len);
        
        new_inode->i_mode = 0x4000 | (1 << 9);  // Temporary permission
        new_inode->i_size = request->buffer_size;
        allocate_node_blocks(request->buf, new_inode, inode_to_bgd(request->parent_inode));
    }
}

int8_t delete(struct EXT2DriverRequest request){

}

/* =============================== MEMORY ==========================================*/

// Return inode number
// This function does not have any side effects (memory/disk modification)
uint32_t allocate_node(void){
    uint32_t inode_number = 0;
    for (int i=0; i<GROUPS_COUNT; ++i){
        struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[i];
        if (bgd->bg_free_inodes_count > 0){     // There is a free inode in the group
            inode_number = 1 + (i+1) * INODES_PER_GROUP - bgd->bg_free_inodes_count;
            break;
        }
    }
    // It is entirely possible that there is no free inode in the filesystem
    return inode_number;
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
            uint32_t block = find_free_in_bgd(preferred_bgd);
            if (!block) return;

            node->i_block[i] = block;
            write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), block, 1);
        }
        blocks_allocated++;
    }

    // Singly indirect block
    if (blocks_allocated < blocks_needed) {
        if (node->i_block[12] == 0) {
            node->i_block[12] = find_free_in_bgd(preferred_bgd);
            if (!node->i_block[12]) return;
            uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
            write_blocks(empty, node->i_block[12], 1);
        }

        uint32_t indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(indirect, node->i_block[12], 1);

        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; i++) {
            if (indirect[i] == 0) {
                uint32_t block = find_free_in_bgd(preferred_bgd);
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
            node->i_block[13] = find_free_in_bgd(preferred_bgd);
            if (!node->i_block[13]) return;

            uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
            write_blocks(empty, node->i_block[13], 1);
        }

        uint32_t doubly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(doubly_indirect, node->i_block[13], 1);

        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; i++) {
            if (doubly_indirect[i] == 0) {
                doubly_indirect[i] = find_free_in_bgd(preferred_bgd);
                if (!doubly_indirect[i]) break;

                uint32_t empty[BLOCK_SIZE / sizeof(uint32_t)] = {0};
                write_blocks(empty, doubly_indirect[i], 1);
            }

            uint32_t singly_indirect[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(singly_indirect, doubly_indirect[i], 1);

            for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < blocks_needed; j++) {
                if (singly_indirect[j] == 0) {
                    uint32_t block = find_free_in_bgd(preferred_bgd);
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
    uint32_t bitmap_block_offset = (local_index/8) / BLOCK_SIZE;
    uint32_t bitmap_byte_offset = (local_index/8) % BLOCK_SIZE;
    uint32_t bitmap_bit_offset = local_index % 8;
    read_blocks(&block.buf, bgd->bg_inode_bitmap + bitmap_block_offset, 1);
    block.buf[bitmap_byte_offset] |= (1 << bitmap_bit_offset);
    write_blocks(&block.buf, bgd->bg_inode_bitmap + bitmap_block_offset, 1);

    // Inode table
    uint32_t inode_table_block = bgd->bg_inode_table;
    uint32_t offset_in_block = local_index * INODE_SIZE;
    uint32_t block_offset = offset_in_block / BLOCK_SIZE;
    uint32_t offset_in_buf = offset_in_block % BLOCK_SIZE;
    read_blocks(&block.buf, inode_table_block + block_offset, 1);
    memcpy(block.buf + offset_in_buf, node, INODE_SIZE);
    write_blocks(&block.buf, inode_table_block + block_offset, 1);
}


/* =============================== HELPER ======================================== */

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

void read_inode(uint32_t inode_num, struct EXT2Inode *out) {
    uint32_t bgd_idx = inode_to_bgd(inode_num);
    uint32_t local_idx = inode_to_local(inode_num);

    struct EXT2BlockGroupDescriptor *bgd = &bgdt.table[bgd_idx];
    uint32_t inode_table_block = bgd->bg_inode_table;

    uint32_t inode_size = sizeof(struct EXT2Inode);
    uint32_t offset_in_block = local_idx * inode_size;

    uint32_t block_offset = offset_in_block / BLOCK_SIZE;
    uint32_t offset_in_buf = offset_in_block % BLOCK_SIZE;

    struct BlockBuffer b;
    read_blocks(&b, inode_table_block + block_offset, 1);

    memcpy(out, b.buf + offset_in_buf, inode_size);
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

