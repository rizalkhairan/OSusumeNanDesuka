#include "header/cpu/gdt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {
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
        {
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
        {
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
