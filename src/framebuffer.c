#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // TODO : Implement
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint32_t offset = (row * FRAMEBUFFER_ROW_LENGTH + col) * 2;
    uint8_t color = (bg << 4) | (fg & 0x0F);
    
    FRAMEBUFFER_MEMORY_OFFSET[offset] = c; // Character
    FRAMEBUFFER_MEMORY_OFFSET[offset + 1] = color; // Color
}

void framebuffer_clear(void) {
    // TODO : Implement
}