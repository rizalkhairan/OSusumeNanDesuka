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
    terminal_buffer.hist_length = 1;
    terminal_buffer.current_line = 0;
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
    int row = terminal_buffer.current_line_row;
    int col = terminal_buffer.current_line_col;

    int r = row;
    int c = col;

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
    int cursor_index = (terminal_buffer.cursor_row - terminal_buffer.current_line_row) + terminal_buffer.cursor_col;

    switch ((unsigned char) c) {
        case KEY_UP:
            if (terminal_buffer.current_line > 0){
                terminal_buffer.current_line--;
                line = &terminal_buffer.history[terminal_buffer.current_line];
                int max_row = terminal_buffer.current_line_row + ((terminal_buffer.current_line_col + line->length) / FRAMEBUFFER_ROW_LENGTH);
                if (terminal_buffer.current_line == terminal_buffer.hist_length){
                    terminal_buffer.cursor_col = 0;
                    terminal_buffer.cursor_row = max_row;
                } else {
                    int max_col = (terminal_buffer.current_line_col + line->length) % FRAMEBUFFER_ROW_LENGTH;
                    terminal_buffer.cursor_col = max_col;
                    terminal_buffer.cursor_row = max_row;
                }
            }
            break;
        case KEY_DOWN:
            if (terminal_buffer.current_line < terminal_buffer.hist_length){
                terminal_buffer.current_line++;
                line = &terminal_buffer.history[terminal_buffer.current_line];
                int max_row = terminal_buffer.current_line_row + ((terminal_buffer.current_line_col + line->length) / FRAMEBUFFER_ROW_LENGTH);
                if (terminal_buffer.current_line == terminal_buffer.hist_length){
                    terminal_buffer.cursor_col = 0;
                    terminal_buffer.cursor_row = max_row;
                } else {
                    int max_col = (terminal_buffer.current_line_col + line->length) % FRAMEBUFFER_ROW_LENGTH;
                    terminal_buffer.cursor_col = max_col;
                    terminal_buffer.cursor_row = max_row;
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
                terminal_buffer.current_line++;
                terminal_buffer.hist_length++;
        
                TerminalLine* new_line = &terminal_buffer.history[terminal_buffer.current_line];
                new_line->length = 0;
        
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
                        terminal_buffer.cursor_col = FRAMEBUFFER_ROW_LENGTH;
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