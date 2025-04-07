#include "header/terminal/terminal.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"

static TerminalBuffer terminal_buffer;

void terminal_initialize(){
    framebuffer_clear();

    // Beginning prints
    // 1. facts
    // 2. current file path
    // From 1 and 2, we will determine the beginning cursor position

    // Set cursor correct position
    terminal_buffer.hist_length = 0;
    terminal_buffer.current_line = 0;
    terminal_buffer.viewed_line = 0;
    terminal_buffer.current_line_row = 0; // will change depending on offset
    terminal_buffer.current_line_col = 0; // will change depending on offset
    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
    TerminalLine* line = &terminal_buffer.history[0];
    line->length = 0;
    framebuffer_set_cursor(terminal_buffer.cursor_row, terminal_buffer.cursor_col);
}

void redraw_current_line(){
    TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];
    int r = terminal_buffer.current_line_row;
    int c = terminal_buffer.current_line_col;

    for (int i = 0; i < line->length; ++i) {
        framebuffer_write(r, c, line->buffer[i], 0xF, 0x0);
        if (++c >= FRAMEBUFFER_ROW_LENGTH) {
            c = 0;
            ++r;
        }
    }

    for (int i = line->length; i < MAX_LINE_LENGTH; ++i) {
        framebuffer_write(r, c, ' ', 0xF, 0x0);
        if (++c >= FRAMEBUFFER_ROW_LENGTH) {
            c = 0;
            ++r;
        }
    }

    framebuffer_set_cursor(terminal_buffer.cursor_row, terminal_buffer.cursor_col);
}

void terminal_handle_input(char c){
    TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];
    int cursor_index = (terminal_buffer.cursor_row - terminal_buffer.current_line_row) * FRAMEBUFFER_ROW_LENGTH + terminal_buffer.cursor_col;

    switch ((unsigned char) c) {
        case KEY_UP:
            if (terminal_buffer.viewed_line > 0) {
                terminal_buffer.viewed_line--;
                TerminalLine* viewed = &terminal_buffer.history[terminal_buffer.viewed_line];
                TerminalLine* current = &terminal_buffer.history[terminal_buffer.current_line];

                current->length = viewed->length;
                for (int i = 0; i < viewed->length; i++) {
                    current->buffer[i] = viewed->buffer[i];
                }

                int max_row = terminal_buffer.current_line_row + ((terminal_buffer.current_line_col + current->length) / FRAMEBUFFER_ROW_LENGTH);
                int max_col = (terminal_buffer.current_line_col + current->length) % FRAMEBUFFER_ROW_LENGTH;
                terminal_buffer.cursor_row = max_row;
                terminal_buffer.cursor_col = max_col;
            }
            break;

        case KEY_DOWN:
            if (terminal_buffer.viewed_line < terminal_buffer.current_line) {
                terminal_buffer.viewed_line++;
                
                if (terminal_buffer.viewed_line < terminal_buffer.current_line) {
                    TerminalLine* viewed = &terminal_buffer.history[terminal_buffer.viewed_line];
                    TerminalLine* current = &terminal_buffer.history[terminal_buffer.current_line];
        
                    current->length = viewed->length;
                    for (int i = 0; i < viewed->length; i++) {
                        current->buffer[i] = viewed->buffer[i];
                    }
        
                    int max_row = terminal_buffer.current_line_row + ((terminal_buffer.current_line_col + current->length) / FRAMEBUFFER_ROW_LENGTH);
                    int max_col = (terminal_buffer.current_line_col + current->length) % FRAMEBUFFER_ROW_LENGTH;
                    terminal_buffer.cursor_row = max_row;
                    terminal_buffer.cursor_col = max_col;
                } else {
                    TerminalLine* current = &terminal_buffer.history[terminal_buffer.current_line];
                    current->length = 0;
        
                    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
                    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
                }
            }
            break;
        case KEY_LEFT:
            if (terminal_buffer.cursor_row == terminal_buffer.current_line_row){
                if (terminal_buffer.cursor_col > terminal_buffer.current_line_col){
                    terminal_buffer.cursor_col--;
                }
            } else {
                if (terminal_buffer.cursor_col > 0) {
                    terminal_buffer.cursor_col--;
                } else {
                    terminal_buffer.cursor_col = FRAMEBUFFER_ROW_LENGTH;
                    terminal_buffer.cursor_row--;
                }
            }
            break;
        case KEY_RIGHT:
            int max_row = terminal_buffer.current_line_row + ((terminal_buffer.current_line_col + line->length) / FRAMEBUFFER_ROW_LENGTH);
            int max_col = (terminal_buffer.current_line_col + line->length) % FRAMEBUFFER_ROW_LENGTH;
            if (terminal_buffer.cursor_row == max_row){
                if (terminal_buffer.cursor_col < max_col) {
                    terminal_buffer.cursor_col++;
                }
            } else {
                if (++terminal_buffer.cursor_col >= FRAMEBUFFER_ROW_LENGTH){
                    terminal_buffer.cursor_col = 0;
                    terminal_buffer.cursor_row++;
                }
            }
            break;
        case '\n':
            if (terminal_buffer.hist_length < MAX_HISTORY) {
                TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];
                add_line_to_history(line);
        
                // print file path
                // place the cursor after file path
                terminal_buffer.cursor_row++;
                terminal_buffer.cursor_col = 0;

                terminal_buffer.current_line_row = terminal_buffer.cursor_row;
                terminal_buffer.current_line_col = terminal_buffer.cursor_col;
            }
            break;
        case '\b':
            if (line->length > 0){
                terminal_line_delete_char(line, cursor_index-1);
                if (terminal_buffer.cursor_row == terminal_buffer.current_line_row){
                    if (terminal_buffer.cursor_col > terminal_buffer.current_line_col){
                        terminal_buffer.cursor_col--;
                    }
                } else {
                    if (terminal_buffer.cursor_col > 0) {
                        terminal_buffer.cursor_col--;
                    } else {
                        terminal_buffer.cursor_col = FRAMEBUFFER_ROW_LENGTH-1;
                        terminal_buffer.cursor_row--;
                    }
                }
            }
            break;
        default:
            if (line->length < MAX_LINE_LENGTH){
                terminal_line_insert_char(line, c, cursor_index);
                if (++terminal_buffer.cursor_col >= FRAMEBUFFER_ROW_LENGTH){
                    terminal_buffer.cursor_col = 0;
                    terminal_buffer.cursor_row++;
                } 
            }
            break;
    }

    redraw_current_line();
}

void add_line_to_history(){
    TerminalLine* current = &terminal_buffer.history[terminal_buffer.current_line];
    if (current->length == 0){
        return;
    }

    int duplicate_index = -1;
    for (int i = 0; i < terminal_buffer.current_line; i++) {
        if (is_same_line(terminal_buffer.history[i], *current)) {
            duplicate_index = i;
            break;
        }
    }

    if (duplicate_index != -1) {
        TerminalLine temp = terminal_buffer.history[duplicate_index];

        for (int i = duplicate_index; i < terminal_buffer.current_line - 1; i++) {
            terminal_buffer.history[i] = terminal_buffer.history[i + 1];
        }

        terminal_buffer.history[terminal_buffer.current_line - 1] = temp;
        terminal_buffer.history[terminal_buffer.current_line].length = 0;
    } else {
        terminal_buffer.current_line++;
        terminal_buffer.hist_length++;
    }

    terminal_buffer.viewed_line = terminal_buffer.current_line;
    terminal_buffer.history[terminal_buffer.current_line].length = 0;
}

bool is_same_line(TerminalLine l1, TerminalLine l2){
    if (l1.length != l2.length){
        return false;
    } 

    for (int i = 0; i < l1.length; i++){
        if (l1.buffer[i] != l2.buffer[i]){
            return false;
        }
    }

    return true;
}


void terminal_line_insert_char(TerminalLine* line, char c, int index) {
    if (line->length >= MAX_LINE_LENGTH - 1) return; 
    if (index < 0 || index > line->length) return;  

    for (int i = line->length; i > index; i--) {
        line->buffer[i] = line->buffer[i - 1];
    }

    line->buffer[index] = c;
    line->length++;
}

void terminal_line_delete_char(TerminalLine* line, int index) {
    if (index < 0 || index >= line->length) return;

    for (int i = index; i < line->length - 1; i++) {
        line->buffer[i] = line->buffer[i + 1];
    }

    line->length--;
}