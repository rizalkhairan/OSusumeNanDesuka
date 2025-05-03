#include "header/cpu/gdt.h"
#include "header/interrupt/interrupt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        { // Null Descriptor
            .segment_low = 0,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0,
            .descriptor_type = 0,
            .descriptor_privilege_level = 0,
            .segment_present = 0,
            .segment_limit_high = 0,
            .avl = 0,
            .long_mode = 0,
            .default_operation_size = 0,
            .granularity = 0,
            .base_high = 0
        },
        { // Kernel Code Segement
            .segment_low = 0xFFFF,       // Limit (low)
            .base_low = 0x0000,          // Base (low)
            .base_mid = 0x00,            // Base (middle)
            .type_bit = 0b1010,          // Type: Executable, Readable
            .descriptor_type = 1,        // Code/Data segment
            .descriptor_privilege_level = 0, // Ring 0 (kernel)
            .segment_present = 1,        // Present in memory
            .segment_limit_high = 0xF,   // Limit (high)
            .avl = 0,                    // Available for OS use
            .long_mode = 0,              // Not 64-bit
            .default_operation_size = 1, // 32-bit segment
            .granularity = 1,            // 4KB granularity
            .base_high = 0x00            // Base (high)
        },
        { // Kernel Data Segment
            .segment_low = 0xFFFF,       // Limit (low)
            .base_low = 0x0000,          // Base (low)
            .base_mid = 0x00,            // Base (middle)
            .type_bit = 0b0010,          // Type: Writable
            .descriptor_type = 1,        // Code/Data segment
            .descriptor_privilege_level = 0, // Ring 0 (kernel)
            .segment_present = 1,        // Present in memory
            .segment_limit_high = 0xF,   // Limit (high)
            .avl = 0,                    // Available for OS use
            .long_mode = 0,              // Not 64-bit
            .default_operation_size = 1, // 32-bit segment
            .granularity = 1,            // 4KB granularity
            .base_high = 0x00            // Base (high)
        },

        /* 3. USER CODE SEGMENT (Ring 3) */
        {
            .segment_low = 0xFFFF,
            .base_low = 0x0000,
            .base_mid = 0x00,
            .type_bit = 0b1010,          // Code: Executable, Readable
            .descriptor_type = 1,
            .descriptor_privilege_level = 3, // Ring 3
            .segment_present = 1,
            .segment_limit_high = 0xF,
            .avl = 0,
            .long_mode = 0,
            .default_operation_size = 1,
            .granularity = 1,
            .base_high = 0x00
        },

        /* 4. USER DATA SEGMENT (Ring 3) */
        {
            .segment_low = 0xFFFF,
            .base_low = 0x0000,
            .base_mid = 0x00,
            .type_bit = 0b0010,          // Data: Readable, Writable
            .descriptor_type = 1,
            .descriptor_privilege_level = 3,
            .segment_present = 1,
            .segment_limit_high = 0xF,
            .avl = 0,
            .long_mode = 0,
            .default_operation_size = 1,
            .granularity = 1,
            .base_high = 0x00
        },

        /* 5. TSS DESCRIPTOR (Task State Segment) */
        {
            .segment_low = sizeof(struct TSSEntry) & 0xFFFF, // TSS size (low)
            .base_low = 0,               // Base set by gdt_install_tss()
            .base_mid = 0,
            .type_bit = 0x9,             // 32-bit Available TSS
            .descriptor_type = 0,        // System segment
            .descriptor_privilege_level = 0, // Only kernel can access
            .segment_present = 1,
            .segment_limit_high = (sizeof(struct TSSEntry) >> 16) & 0xF,
            .avl = 0,
            .long_mode = 0,
            .default_operation_size = 1,
            .granularity = 0,            // Byte granularity
            .base_high = 0
        }
    }
};

/**
 * _gdt_gdtr, predefined system GDTR. 
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    // TODO : Implement, this GDTR will point to global_descriptor_table. 
    //        Use sizeof operator
    .size = sizeof(global_descriptor_table) - 1, // Size of GDT minus 1
    .address = &global_descriptor_table          // Pointer to the GDT
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}
