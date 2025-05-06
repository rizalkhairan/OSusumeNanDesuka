#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"
#include "header/interrupt/idt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"
#include "header/terminal/terminal.h"
#include "header/filesystem/disk.h"
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    terminal_initialize();
    keyboard_state_activate();

    initialize_filesystem_ext2();
    
    int x = 555;
    for(int i=0;i<0;i++){
        struct EXT2DriverRequest req = {
            .name = "Dirs    ",
            .name_len = 8,
            .parent_inode = 2,
            .buffer_size = 8,
            .is_directory = true,
        };
        memset(req.name+4, (char)(i/1000+48), 1);
        memset(req.name+5, (char)((i/100)%10+48), 1);
        memset(req.name+6, (char)((i/10)%10+48), 1);
        memset(req.name+7, (char)(i%10+48), 1);
        if(i==24){
            int kusanagi = 39;
        }
        x = write(&req);
    }


    // Write File
    int8_t result = 99;
    char file[] = "FileXXXX has been written";

    struct BlockBuffer file_buf[1];
    
    for (uint32_t i=0;i<sizeof(file_buf)/BLOCK_SIZE;i++) {
        memset(file_buf[i].buf, (char) (i%10)+'0', BLOCK_SIZE);
    }
    for (int i=0;i<10;i++) {
        memcpy(file_buf[0].buf, file, sizeof(file));
        memset(file_buf[0].buf+4, (char)(i/1000+48), 1);
        memset(file_buf[0].buf+5, (char)((i/100)%10+48), 1);
        memset(file_buf[0].buf+6, (char)((i/10)%10+48), 1);
        memset(file_buf[0].buf+7, (char)(i%10+48), 1);
        struct EXT2DriverRequest req = {
            .buf = file_buf[0].buf,
            .name = "File    ",
            .name_len = 8,
            .parent_inode = 2,
            .buffer_size = sizeof(file_buf),
            .is_directory = false,
        };
        memset(req.name+4, (char)(i/1000+48), 1);
        memset(req.name+5, (char)((i/100)%10+48), 1);
        memset(req.name+6, (char)((i/10)%10+48), 1);
        memset(req.name+7, (char)(i%10+48), 1);
        result = write(&req);
    }
    
    
    // Delete file
    struct EXT2DriverRequest req10 = {
        .name = "File0002",
        .name_len = 8,
        .parent_inode = 2,
        .buffer_size = 0,
        .is_directory = false,
    };
    result = delete(req10);    
    struct EXT2DriverRequest req = {
        .name = "File0001",
        .name_len = 8,
        .parent_inode = 2,
        .buffer_size = 0,
        .is_directory = false,
    };
    result = delete(req);

    char testres[] = "Test result: ";
    for (int i = 0; i < sizeof(testres); i++) {
        framebuffer_write(20, i, testres[i], 0x07, 0x00);
    }
    framebuffer_write(20, sizeof(testres)+3, (char) (result%10+'0'), 0x07, 0x00);
    framebuffer_write(20, sizeof(testres)+2, (char) (result/10%10+'0'), 0x07, 0x00);
    framebuffer_write(20, sizeof(testres)+1, (char) (result/100%10+'0'), 0x07, 0x00);
    framebuffer_write(20, sizeof(testres), (char) (result/1000%10+'0'), 0x07, 0x00);
    
    // struct EXT2Inode root;
    // read_inode(2, &root);
    
    struct EXT2Inode root;
    read_inode(2, &root);

    struct EXT2DriverRequest req2 = {
            .name = ".",
            .name_len = 1,
            .parent_inode = 2,
            .buffer_size = 2*BLOCK_SIZE,
            .is_directory = true,
        };
        
    x = read_directory(&req2);

    int ootori = 39;

    // struct BlockBuffer b;
    // set_bitmap_bit(b.buf, 112, true);
    // framebuffer_write(2, 0, is_bitmap_set(b.buf, 112)+48, 0x07, 0x00);
    // framebuffer_write(2, 3, 'a', 0x07, 0x00);
    // framebuffer_write(2, 0, is_bitmap_set(b.buf, 112)+48, 0x07, 0x00);
    // for (int i = 0; i < 512; i++) b.buf[i] = i % 16;
    // write_blocks(&b, 17, 1);
    // while (true);



    /* -------------------------------------- */
    /* Inodes */
    // uint32_t inode_number = allocate_node();
    // uint32_t iter = 0;
    // struct EXT2Inode inode_temp = {
    //     .i_mode = EXT2_S_IFDIR,
    //     .i_size = BLOCK_SIZE,
    //     .i_blocks = 0
    // };
    // memset(inode_temp.i_block, 0x0, 15 * sizeof(uint32_t));
    // while (inode_number != 0 && iter < 10000) {
    //     sync_node(&inode_temp, inode_number);
    //     inode_number = allocate_node();
    //     iter++;
    // }
    // sync_node(&inode_temp, inode_number);


    // framebuffer_write(5, 5, (char) iter%10+'0', 0x07, 0x00);
    // framebuffer_write(5, 4, (char) (iter/10%10)+'0', 0x07, 0x00);
    // framebuffer_write(5, 3, (char) (iter/100%10)+'0', 0x07, 0x00);

    // framebuffer_write(6, 5, (char) bgdt.table[0].bg_free_inodes_count%10+'0', 0x07, 0x00);
    // framebuffer_write(6, 4, (char) bgdt.table[0].bg_free_inodes_count/10%10+'0', 0x07, 0x00);
    // framebuffer_write(6, 3, (char) bgdt.table[0].bg_free_inodes_count/100%10+'0', 0x07, 0x00);

    // framebuffer_write(7, 5,(char) allocate_node()+'0', 0x07, 0x00);

    // framebuffer_write(8, 5, (char) bgdt.table[0].bg_inode_bitmap%10+'0', 0x07, 0x00);
    // framebuffer_write(8, 4, (char) bgdt.table[0].bg_inode_bitmap/10%10+'0', 0x07, 0x00);


    char block[] = {"Blocks per group: "};
    char inode[] = {"Inodes per group: "};
    char inode_per_table[] = {"Inodes per table: "};
    char group[] = {"Groups count: "};
    
    for (int i = 0; i < sizeof(block); i++) {
        framebuffer_write(0, i, block[i], 0x07, 0x00);
    }
    framebuffer_write(0, sizeof(block), (char) (((BLOCKS_PER_GROUP/1000)%10)+'0'), 0x07, 0x00);
    framebuffer_write(0, sizeof(block)+1, (char) (((BLOCKS_PER_GROUP/100)%10)+'0'), 0x07, 0x00);
    framebuffer_write(0, sizeof(block)+2, (char) (((BLOCKS_PER_GROUP/10)%10)+'0'), 0x07, 0x00);
    framebuffer_write(0, sizeof(block)+3, (char) ((BLOCKS_PER_GROUP%10)+'0'), 0x07, 0x00);
    
    for (int i = 0; i < sizeof(inode); i++) {
        framebuffer_write(1, i, inode[i], 0x07, 0x00);
    }
    framebuffer_write(1, sizeof(inode), (char) (((INODES_PER_GROUP/1000)%10)+'0'), 0x07, 0x00);
    framebuffer_write(1, sizeof(inode)+1, (char) (((INODES_PER_GROUP/100)%10)+'0'), 0x07, 0x00);
    framebuffer_write(1, sizeof(inode)+2, (char) (((INODES_PER_GROUP/10)%10)+'0'), 0x07, 0x00);
    framebuffer_write(1, sizeof(inode)+3, (char) ((INODES_PER_GROUP%10)+'0'), 0x07, 0x00);
    
    for (int i = 0; i < sizeof(inode_per_table); i++) {
        framebuffer_write(2, i, inode_per_table[i], 0x07, 0x00);
    }
    framebuffer_write(2, sizeof(inode_per_table), (char) (((INODES_PER_TABLE/1000)%10)+'0'), 0x07, 0x00);
    framebuffer_write(2, sizeof(inode_per_table)+1, (char) (((INODES_PER_TABLE/100)%10)+'0'), 0x07, 0x00);
    framebuffer_write(2, sizeof(inode_per_table)+2, (char) (((INODES_PER_TABLE/10)%10)+'0'), 0x07, 0x00);
    framebuffer_write(2, sizeof(inode_per_table)+3, (char) ((INODES_PER_TABLE%10)+'0'), 0x07, 0x00);
    
    for (int i = 0; i < sizeof(group); i++) {
        framebuffer_write(3, i, group[i], 0x07, 0x00);
    }
    framebuffer_write(3, sizeof(group), (char) (((GROUPS_COUNT/1000)%10)+'0'), 0x07, 0x00);
    framebuffer_write(3, sizeof(group)+1, (char) (((GROUPS_COUNT/100)%10)+'0'), 0x07, 0x00);
    framebuffer_write(3, sizeof(group)+2, (char) (((GROUPS_COUNT/10)%10)+'0'), 0x07, 0x00);
    framebuffer_write(3, sizeof(group)+3, (char) ((GROUPS_COUNT%10)+'0'), 0x07, 0x00);
    
    
    char superblock_size[] = {"Superblock size: "};
    char inode_size[] = {"Inode size: "};
    char bgd[] = {"BGD size: "};
    char directory_entry[] = {"Directory entry size: "};
    
    for (int i = 0; i < sizeof(superblock_size); i++) {
        framebuffer_write(9, i, superblock_size[i], 0x07, 0x00);
    }
    framebuffer_write(9, sizeof(superblock_size), (char) (sizeof(struct EXT2Superblock)/10)%10+'0', 0x07, 0x00);
    framebuffer_write(9, sizeof(superblock_size)+1, (char) sizeof(struct EXT2Superblock)%10+'0', 0x07, 0x00);
    
    for (int i = 0; i < sizeof(inode_size); i++) {
        framebuffer_write(10, i, inode_size[i], 0x07, 0x00);
    }
    framebuffer_write(10, sizeof(inode_size), (char) (sizeof(struct EXT2Inode)/10)%10+'0', 0x07, 0x00);
    framebuffer_write(10, sizeof(inode_size)+1, (char) sizeof(struct EXT2Inode)%10+'0', 0x07, 0x00);
    
    for (int i = 0; i < sizeof(bgd); i++) {
        framebuffer_write(11, i, bgd[i], 0x07, 0x00);
    }
    framebuffer_write(11, sizeof(bgd), (char) (sizeof(struct EXT2BlockGroupDescriptor)/10)%10+'0', 0x07, 0x00);
    framebuffer_write(11, sizeof(bgd)+1, (char) sizeof(struct EXT2BlockGroupDescriptor)%10+'0', 0x07, 0x00);
    
    for (int i = 0; i < sizeof(directory_entry); i++) {
        framebuffer_write(12, i, directory_entry[i], 0x07, 0x00);
    }
    framebuffer_write(12, sizeof(directory_entry), (char) (sizeof(struct EXT2DirectoryEntry)/10)%10+'0', 0x07, 0x00);
    framebuffer_write(12, sizeof(directory_entry)+1, (char) sizeof(struct EXT2DirectoryEntry)%10+'0', 0x07, 0x00);
    
    
    char test[] = {"Test: "};
    for (int i = 0; i < sizeof(test); i++) {
        framebuffer_write(15, i, test[i], 0x07, 0x00);
    }
    
    uint32_t group_count = BLOCK_SIZE / sizeof(struct EXT2BlockGroupDescriptor);
    framebuffer_write(16, 0, (char) (group_count/100)%10+'0', 0x07, 0x00);
    framebuffer_write(16, 1, (char) (group_count/10)%10+'0', 0x07, 0x00);
    framebuffer_write(16, 2, (char) (group_count%10)+'0', 0x07, 0x00);
    
    uint32_t blocks_per_group = DISK_SPACE / BLOCK_SIZE / 8;
    framebuffer_write(17, 0, (char) (blocks_per_group/1000)%10+'0', 0x07, 0x00);
    framebuffer_write(17, 1, (char) (blocks_per_group/100)%10+'0', 0x07, 0x00);
    framebuffer_write(17, 2, (char) (blocks_per_group/10)%10+'0', 0x07, 0x00);
    framebuffer_write(17, 3, (char) (blocks_per_group%10)+'0', 0x07, 0x00);
    
    uint32_t blocks_per_group_macro = DISK_SPACE / BLOCK_SIZE / (BLOCK_SIZE / sizeof(struct EXT2BlockGroupDescriptor)) / 2;
    framebuffer_write(18, 0, (char) (blocks_per_group_macro/1000)%10+'0', 0x07, 0x00);
    framebuffer_write(18, 1, (char) (blocks_per_group_macro/100)%10+'0', 0x07, 0x00);
    framebuffer_write(18, 2, (char) (blocks_per_group_macro/10)%10+'0', 0x07, 0x00);
    framebuffer_write(18, 3, (char) (blocks_per_group_macro%10)+'0', 0x07, 0x00);
    
    bool equal_group_count = (blocks_per_group == blocks_per_group_macro);
    framebuffer_write(19, 0, (char) (equal_group_count ? 'T' : 'F'), 0x07, 0x00);
    
    // struct EXT2DriverRequest reqaa = {
    //     .name = "",
    //     .name_len = 255,
    //     .parent_inode = 2,
    //     .buffer_size = 1,
    //     .is_directory = true,
    // };
    // x = write(&reqaa);

    while (true) {

    }
}