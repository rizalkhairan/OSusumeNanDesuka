#include "header/keyboard/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

struct KeyboardDriverState keyboard_state = {
    .keyboard_input_on = false,
    .keyboard_buffer = 0
};

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

void keyboard_isr(void) {
    // 1. Read scancode from keyboard data port (required for all IRQ1)
    uint8_t scancode = in(KEYBOARD_DATA_PORT);
    
    // 2. Always acknowledge the interrupt (required)
    pic_ack(IRQ_KEYBOARD);
    
    // 3. Only process if keyboard input is active
    if (keyboard_state.keyboard_input_on) {
        // Ignore key release events (high bit set)
        if (scancode & 0x80) {
            return;
        }
        
        // Convert scancode to ASCII using the mapper
        char ascii = keyboard_scancode_1_to_ascii_map[scancode];
        
        // Store only valid ASCII characters (non-zero in mapper)
        if (ascii != 0) {
            keyboard_state.keyboard_buffer = ascii;
        }
    }
}

void keyboard_state_activate(void){
    keyboard_state.keyboard_input_on = true;
    keyboard_state.keyboard_buffer = 0;
}

void keyboard_state_deactivate(void){
    keyboard_state.keyboard_input_on = false;
}

void get_keyboard_buffer(char *buf){
    *buf = keyboard_state.keyboard_buffer;
    keyboard_state.keyboard_buffer = 0;
}