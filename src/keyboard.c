#include "header/keyboard/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on = false,

    .is_shift_pressed = false,
    .is_ctrl_pressed = false,
    .is_alt_pressed = false,
    .is_caps_lock_on = false,

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

const char keyboard_scancode_to_ascii_map_shift[256] = {
    0, 0x1B, '!', '@', '#', '$', '%', '^',  '&', '*', '(',  ')',  '_', '+', '\b', '\t',
  'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',  'O', 'P', '{',  '}', '\n',   0,  'A',  'S',
  'D',  'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',   0, '|',  'Z', 'X',  'C',  'V',
  'B',  'N', 'M', '<', '>', '?',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '_',    0,    0,   0,  '+',    0,
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
    // Read scancode from keyboard data port (required for all IRQ1)
    uint8_t scancode = in(KEYBOARD_DATA_PORT);

    // Only process if keyboard input is active
    if (keyboard_state.keyboard_input_on) {
        // Handle extended scancode prefix
        /** 
         * @todo 
         * Clean the conditionals below
         * - Staryo40
         */
        if (scancode == EXTENDED_SCANCODE_BYTE) { 
            keyboard_state.read_extended_mode = true;
            pic_ack(IRQ_KEYBOARD);
            return;
        } else if(scancode == L_SHIFT || scancode == R_SHIFT){
            keyboard_state.is_shift_pressed = true;
        } else if(scancode == KEY_RELEASE(L_SHIFT) || scancode == KEY_RELEASE(R_SHIFT)){
            keyboard_state.is_shift_pressed = false;
        } else if (scancode == CAPS_LOCK){
            keyboard_state.is_caps_lock_on = !keyboard_state.is_caps_lock_on;
        } else if (scancode == L_CTRL){
            keyboard_state.is_ctrl_pressed = true;
        } else if (scancode == KEY_RELEASE(L_CTRL)){
            keyboard_state.is_ctrl_pressed = false;
        } else if (scancode == L_ALT){
            keyboard_state.is_alt_pressed = true;
        } else if (scancode == KEY_RELEASE(L_ALT)){
            keyboard_state.is_alt_pressed = false;
        }

        // Handle extended scancodes (arrow keys)
        if (keyboard_state.read_extended_mode) {
            keyboard_state.read_extended_mode = false;
            
            // Check for arrow keys
            // switch(scancode) {
            //     case EXT_SCANCODE_UP:
            //         keyboard_state.keyboard_buffer = KEY_UP;
            //         break;
            //     case EXT_SCANCODE_DOWN:
            //         keyboard_state.keyboard_buffer = KEY_DOWN;
            //         break;
            //     case EXT_SCANCODE_LEFT:
            //         keyboard_state.keyboard_buffer = KEY_LEFT;
            //         break;
            //     case EXT_SCANCODE_RIGHT:
            //         keyboard_state.keyboard_buffer = KEY_RIGHT;
            //         break;
            // }
            return;
        }
        
        // Convert scancode to ASCII using the mapper
        if (keyboard_state.is_shift_pressed) {
            char ascii = keyboard_scancode_to_ascii_map_shift[scancode];
            keyboard_state.keyboard_buffer = ascii;
        } else if (keyboard_state.is_caps_lock_on){
            char ascii = keyboard_scancode_1_to_ascii_map[scancode];
            if (ascii >= 'a' && ascii <= 'z') {
                ascii = ascii - 32;
            }
            keyboard_state.keyboard_buffer = ascii;
        }else{ // All default behaviors
            char ascii = keyboard_scancode_1_to_ascii_map[scancode];
            keyboard_state.keyboard_buffer = ascii;
        }

        
    }
    pic_ack(IRQ_KEYBOARD);
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