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
    uint16_t total = (total + 3) & ~0x03; // padding
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
    return inode/INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode){
    return inode % INODES_PER_GROUP;
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
    uint32_t new_block = allocate_block(); 

    node->i_size = BLOCK_SIZE;
    node->i_blocks = 1;
    node->i_block[0] = new_block;
    for (int i = 1; i < 15; i++) node->i_block[i] = 0;
}

/* =============================== INITIALIZER ==========================================*/

struct EXT2Superblock sb = {            //TODO: recheck values...
    .s_inodes_count = INODES_PER_GROUP*GROUPS_COUNT,
    .s_blocks_count = BLOCKS_PER_GROUP*GROUPS_COUNT,
    .s_r_blocks_count = 0,
    .s_free_blocks_count = BLOCKS_PER_GROUP*GROUPS_COUNT - 10,
    .s_free_inodes_count = INODES_PER_GROUP*GROUPS_COUNT - 1,
    .s_first_data_block = 1,
    .s_first_ino = 1,
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
            .bg_used_dirs_count = 0,
            .bg_pad = 0,
            .bg_reserved = {0,0,0}
        };
        bgdt.table[i] = bgd_template;
    }
    write_blocks(&bgdt, 2, 1);

    // create root directory
    
}

void initialize_filesystem_ext2(void){
    if(is_empty_storage){
        create_ext2();
    } else{
        read_blocks(&sb, 1, 1); // Read Superblock
        for(int i=0;i<GROUPS_COUNT;i++){ // Read BGDs, TODO: Read only the first one or all of them?
            struct EXT2BlockGroupDescriptor bgd_template = {};
            int bgd_block = 2 + (i*BLOCKS_PER_GROUP/(BLOCK_SIZE/sizeof(struct EXT2BlockGroupDescriptor)));
            read_blocks(&bgd_template, bgd_block, 1);
            bgdt.table[i] = bgd_template;
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

}

int8_t read(struct EXT2DriverRequest request){

}

int8_t write(struct EXT2DriverRequest *request){

}

int8_t delete(struct EXT2DriverRequest request){

}

/* =============================== MEMORY ==========================================*/

uint32_t allocate_node(void){

}

void deallocate_node(uint32_t inode){

}

void deallocate_blocks(void *loc, uint32_t blocks){

}

uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, uint32_t depth, uint32_t *last_bgd, bool bgd_loaded){

}

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd){

}

void sync_node(struct EXT2Inode *node, uint32_t inode){

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
