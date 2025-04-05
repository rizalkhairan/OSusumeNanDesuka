#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos = r * FRAMEBUFFER_ROW_LENGTH + c;
    
    // Send low byte of cursor position to port 0x3D4
    out(0x3D4, 0x0F);
    out(0x3D5, (uint8_t)(pos & 0xFF));
    
    // Send high byte of cursor position to port 0x3D4
    out(0x3D4, 0x0E);
    out(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint32_t offset = (row * FRAMEBUFFER_ROW_LENGTH + col) * 2;
    uint8_t color = (bg << 4) | (fg & 0x0F);
    
    FRAMEBUFFER_MEMORY_OFFSET[offset] = c; // Character
    FRAMEBUFFER_MEMORY_OFFSET[offset + 1] = color; // Color
}

void framebuffer_clear(void) {
    uint8_t *framebuffer_ptr = FRAMEBUFFER_MEMORY_OFFSET;
    for (uint16_t i = 0; i < FRAMEBUFFER_ROW_LENGTH * FRAMEBUFFER_COL_LENGTH; i++) {
        *framebuffer_ptr++ = 0x00; // Empty character
        *framebuffer_ptr++ = 0x07; // Gray character & black background
    }
}