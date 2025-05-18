#include <stdint.h>
#include "header/filesystem/ext2.h"
#include "header/terminal/terminal.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"

#define BLOCK_COUNT 16

uint32_t cwd_inode = 2;
char cwd_name[255];
uint16_t cwd_name_len;

uint32_t filepath_len = 23;
static TerminalBuffer terminal_buffer;

// need this? or rely the parsing on each function?
Command command_table[] = {
    { "clear",  5, 0, 0},
    { "cd",     2, 0, 1},
    { "mv",     2, 2, 2},
    { "cp",     2, 2, 2},
    { "cat",    3, 1, 1},
    { "ls",     2, 1, 1},
    { "mkdir",  5, 1, 1},
    { "rm",     2, 1, 1},
    { "find",   4, 1, 1},
};
#define NUM_COMMANDS (sizeof(command_table)/sizeof(Command))

void terminal_initialize(){
    syscall(8, 0, 0, 0);

    // Beginning prints
    // 1. facts
    // 2. current file path
    // From 1 and 2, we will determine the beginning cursor position

    char* filepath = "OSusumeWaNanDesuka?:.$ ";
    syscall(6, filepath, filepath_len, 0xA);

    // Set cursor correct position
    terminal_buffer.hist_length = 0;
    terminal_buffer.current_line = 0;
    terminal_buffer.viewed_line = 0;
    terminal_buffer.current_line_row = 0; // will change depending on offset
    terminal_buffer.current_line_col = filepath_len; // will change depending on offset
    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
    TerminalLine* line = &terminal_buffer.history[0];
    line->length = 0;
    
    syscall(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
}

void write_path(){
    char* filepath = "OSusumeWaNanDesuka?:.$ ";
    TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];

    terminal_buffer.current_line_col = 0;
    terminal_buffer.current_line_row += (line->length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    syscall(6, filepath, filepath_len, 0xA);
    terminal_buffer.current_line_col = filepath_len;
   
    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
    syscall(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);

    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
}

void redraw_current_line(){
    TerminalLine line = terminal_buffer.history[terminal_buffer.current_line];

    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    syscall(6, &line.buffer, line.length, 0xF);

    syscall(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
}

void terminal_handle_input(char c){
    TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];
    int cursor_index;
    if (terminal_buffer.cursor_row == terminal_buffer.current_line_row) {
        cursor_index = terminal_buffer.cursor_col - terminal_buffer.current_line_col;
    } else {
        cursor_index = (terminal_buffer.cursor_row - terminal_buffer.current_line_row) * FRAMEBUFFER_ROW_LENGTH +
                       terminal_buffer.cursor_col - filepath_len;
    }

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
                execute(line->buffer, line->length);
                add_line_to_history(line);
                write_path();
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
    // syscall(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);
    // syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    redraw_current_line();
    // syscall(6, line, cursor_index, 0x3);
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

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

int main(void) {
    cwd_name[0] = '.';
    cwd_name_len = 1;

    terminal_initialize();
    syscall(7, 0, 0, 0);
    while(true){
        char c;
        syscall(4, &c, 0, 0);
        if(c){
            terminal_handle_input(c);
        }
    }

    return 0;
}

// --------------- utilities ---------------
ParsedInput parse_input_all(const char* input, uint32_t length){
    bool isKutip = false;
    ParsedInput res = {0};

    uint32_t i = 0;
    while(i<length && res.argc < MAX_ARGC){
        // skip leading spaces
        while(i<length && input[i]==' '){
            i++;
        }
        // all input has been read
        if(i>=length) break;
         
        // this is the start of a word
        uint32_t start = i;
        if(input[i]=='\''){
            isKutip = true;
            start = ++i;
            while(i<length && input[i]!= '\''){
                i++;
            }
            isKutip = false;

        } else{
            while(i<length && input[i]!= ' '){
                i++;
            }
        }

        // copy the word to argv
        uint32_t word_len = i-start;
        if(word_len >= MAX_ARG_LEN){
            word_len = MAX_ARG_LEN -1;
        }

        Arg* arg = &res.argv[res.argc];
        memcpy(arg->buffer, &input[start], word_len);
        arg->length = word_len;
        res.argc++;
        
        if (isKutip == false && i < length && input[i] == '\'') {
            i++;
        }
    }
    return res;
}

ParsedInput parse_input_n(const char *input, uint32_t length, int n){
    ParsedInput res = {0};

    uint32_t i = 0;
    for(uint32_t arg_index=0; arg_index<n && i<length; arg_index++){
        // skip leading spaces
        while(i<length && input[i]==' '){
            i++;
        }
        // all input has been read
        if(i>=length) break;
         
        // this is the start of a word
        uint32_t start = i;
        uint32_t word_len = 0;

        if(arg_index == n-1){
            start = i;
            word_len = length - start;
        } else{
            if(input[i]=='\''){
                start = ++i;
                while(i<length && input[i]!= '\''){
                    i++;
                }
                word_len = i-start;
                if(i<length && input[i]=='\''){i++;}
            } else{
                while(i<length && input[i]!= ' '){
                    i++;
                }
                word_len = i - start;
            }
        }

        // copy the word to argv
        if(word_len >= MAX_ARG_LEN){
            word_len = MAX_ARG_LEN -1;
        }

        Arg* arg = &res.argv[res.argc];
        memcpy(arg->buffer, &input[start], word_len);
        arg->length = word_len;
        res.argc++;
    }
    return res;
}

void execute(const char* input, uint32_t length){
    ParsedInput args = parse_input_n(input, length, 2);
    if(args.argc==0){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x2);
        return;
    }
    
    uint8_t i=0;
    for(i=0;i<NUM_COMMANDS;i++){
        if(memcmp(command_table->name, args.argv[0].buffer, args.argv[0].length)==0){
            break;
        }
    }
    if(i<NUM_COMMANDS){
        if(i==0){clear(args.argv[1].buffer, args.argv[1].length);}
        
    } else{
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, args.argv[1].buffer, args.argv[1].length, 0xE);
    }

    // if(memcmp("ikanaide", args.argv[0].buffer, args.argv[0].length)==0){
    //     terminal_buffer.current_line_col = 0;
    //     terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
    //     syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    //     syscall(6, args.argv[1].buffer, args.argv[1].length, 0xE);
    // }
}

// --------------- commands ---------------
void clear(const char* input, uint32_t length){
    if(length!=0){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x3);
        return;
    }
    terminal_buffer.current_line_col = 0;
    terminal_buffer.current_line_row = -1; // TODO: VALIDASI TEMBUS LAYAR
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    syscall(8,0,0,0);
}

void cd(const char* input, uint32_t length){
    if(length==0){
        cwd_name[0] = '.';
        cwd_name_len = 1;
        cwd_inode = 2;
        return;
    }
    return;
}