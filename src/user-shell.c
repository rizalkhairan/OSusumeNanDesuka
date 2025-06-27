#include <stdint.h>
#include "header/filesystem/ext2.h"
#include "header/terminal/terminal.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"
#include "header/process/process.h"

#define BLOCK_COUNT 16

Path absolute_path[8] = {0};
uint8_t depth = 0;
char fullpath[2040];
uint32_t fullpath_length = 1;

uint32_t cwd_inode = 2;
char cwd_name[255];
uint16_t cwd_name_len;
uint32_t filepath_len = 22;

static char *terminal_hostname = "OSusumeWaNanDesuka?:";
static uint8_t terminal_hostname_len = 20;

static struct TerminalBuffer terminal;


// Command command_table[] = {
//     { "clear",  5},
//     { "cd",     2},
//     { "mv",     2},
//     { "cp",     2},
//     { "cat",    3},
//     { "ls",     2},
//     { "mkdir",  5},
//     { "rm",     2},
//     { "find",   4},
//     { "kill",   4},
//     { "ps",     2},
//     { "exec",   4},
// };
#define NUM_COMMANDS (sizeof(command_table)/sizeof(Command))

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
    memcpy(absolute_path[0].name, ".", 1);
    absolute_path[0].length = 1;
    absolute_path[0].inode_num = 2;
    cwd_name[0] = '.';
    cwd_name_len = 1;
    
    // terminal_initialize();

    terminal_init();
    history_init();
    print_path();
    
    syscall(7, 0, 0, 0);
    // execute("kill 0", 6);
    // execute("ps", 2);
    // execute("cp yoisaki asahina", 18);
    // execute("exec timer", 10);
    // execute("exec timer", 10);
    // execute("kill 1", 6);
    while(true){
        char c;
        syscall(4, &c, 0, 0);
        if(c){
            terminal_handle_input(c);
        }
    }

    return 0;
}

// --------------- CLI (Rizal's version) --------------- 
void terminal_init() {
    initialize_line_feeds();

    // Pointers to the buffers. Set to 1 to enable first_char and last_char to actually point to valid characters
    terminal.first_char = 1;
    terminal.last_char = 0;
    terminal.first_displayed_char = 1;
    terminal.input_char = 1;
    
    // Portion of the framebuffer the user-shell can use
    terminal.allocated_rows = 23;
    terminal.allocated_columns = FRAMEBUFFER_ROW_LENGTH;
    terminal.screen_size = terminal.allocated_rows * terminal.allocated_columns;
}
void terminal_clear() {
    memset(terminal.terminal_buffer_data, 0x0, sizeof(terminal.terminal_buffer_data));
    memset(terminal.terminal_colors, 0x0, sizeof(terminal.terminal_colors));

    terminal_init();
}
void terminal_flush() {
    // Flush current page by calling SYSCALL 30
    char screen[terminal.screen_size];
    int32_t area[5] = {0, terminal.allocated_rows, terminal.screen_size, 0, 0};
    uint8_t colors[terminal.screen_size * 2];
    
    // Obtain screen
    memset(screen, 0x0, sizeof(screen));
    memset(colors, 0x0, sizeof(colors));
    uint16_t to_read = terminal.first_displayed_char;
    for (uint16_t i = 0; i < terminal.screen_size; i++) {
        screen[i] = terminal.terminal_buffer_data[to_read];
        colors[2 * i] = terminal.terminal_colors[to_read] >> 8;
        colors[2 * i + 1] = terminal.terminal_colors[to_read] & 0xFF;
        
        if (to_read == terminal.last_char) {
            break;
        }

        to_read++;
        if (to_read >= TERMINAL_BUFFER_SIZE) {
            to_read = 0;
        }
    }
    
    syscall(30, (uint32_t)screen, (uint32_t)area, (uint32_t)colors);

    terminal.last_flushed_col = area[3];
    terminal.last_flushed_row = area[4];
}

/* Cursor */
void set_cursor(uint16_t dist) {
    syscall(9, terminal.cursor.row, terminal.cursor.col, 0);
}
void cursor_left() {
    if (terminal.cursor.dist == 1) return;
    set_cursor(terminal.cursor.dist-1);
}
void cursor_right() {
    set_cursor(terminal.cursor.dist+1);
}

/* Functions that also flush. Used to control the screen. Call these functions */
void move_to_last_screen() { // Assumes end_screen is already calculated
    // Set current_screen to the character after the last line feed or the first character
    if (terminal.line_feeds.head == terminal.line_feeds.tail) {
        terminal.end_screen = terminal.first_char;
    } else {
        terminal.end_screen = terminal.line_feeds.positions[terminal.line_feeds.tail];
        terminal.end_screen = (terminal.end_screen + 1) % TERMINAL_BUFFER_SIZE;
    }

    // Scroll up such to fill the screen
    for (uint16_t i = 0; i < terminal.line_feeds.tail - terminal.line_feeds.head; i++) {
        scroll_down();
        if (terminal.first_displayed_char-1 == terminal.line_feeds.positions[terminal.line_feeds.tail]){
            break;
        }
    }
    for (uint8_t i = 0; i < (terminal.allocated_rows - 5); i++) {
        scroll_up();
    }
    terminal_flush();
}
void redraw_input(char* command, uint32_t command_length) { // To redraw command history
    terminal.last_char = terminal.input_char;
    puts_terminal(command, command_length, DEFAULT_COLOR, TERMINAL_BACKGROUND);

    // End the input with null terminator so the flushed output ends
    // terminal.terminal_buffer_data[(terminal.last_char+1)%TERMINAL_BUFFER_SIZE] = '\0';
    terminal_flush();

    set_cursor(command_length + 1);
}
void print_path() {
    puts_terminal(terminal_hostname, terminal_hostname_len, HOSTNAME_COLOR, TERMINAL_BACKGROUND);

    get_absolute_path();
    puts_terminal(fullpath, get_absolute_path_length(), HOSTNAME_COLOR, TERMINAL_BACKGROUND);
    puts_terminal("$ ", 2, HOSTNAME_COLOR, TERMINAL_BACKGROUND);
    terminal.input_char = terminal.last_char;

    terminal_flush();
}

/* Scrolling. >>> Make sure to flush the terminal <<< */
void scroll_up(){
    if (terminal.first_displayed_char == terminal.first_char) return;

    // Find previous line feed
    bool prev_line_feed_found = false;
    uint16_t line_feed_idx = terminal.line_feeds.tail;
    uint16_t current_displayed_char_rel = (TERMINAL_BUFFER_SIZE + terminal.first_displayed_char - 1 - terminal.first_char)%TERMINAL_BUFFER_SIZE;
    for (;;) {
        if (line_feed_idx == terminal.line_feeds.head) {
            break;
        }

        uint16_t line_feed_char_rel = (TERMINAL_BUFFER_SIZE + terminal.line_feeds.positions[line_feed_idx] - terminal.first_char) % TERMINAL_BUFFER_SIZE;
        if (line_feed_char_rel < current_displayed_char_rel) {
            // Found a line feed before the first displayed character
            prev_line_feed_found = true;
            break;
        }

        if (line_feed_idx == 0) {
            line_feed_idx = TERMINAL_BUFFER_SIZE - 1;
        } else {
            line_feed_idx--;
        }
    }

    if (prev_line_feed_found) {
        // Go the last wrapped line if this line spans more than the allocated columns
        uint16_t prev_line_feed_rel = (TERMINAL_BUFFER_SIZE + terminal.line_feeds.positions[line_feed_idx] - terminal.first_char) % TERMINAL_BUFFER_SIZE;
        while (prev_line_feed_rel + terminal.allocated_columns < current_displayed_char_rel) {
            prev_line_feed_rel += terminal.allocated_columns;
        }
        terminal.first_displayed_char = (terminal.first_char + prev_line_feed_rel + 1) % TERMINAL_BUFFER_SIZE;
    } else {
        // If no previous line feed found, set it to the first character
        terminal.first_displayed_char = terminal.first_char;
    }

}
void scroll_down(){
    // Try to point to the next character after a newline or after allocated columns
    uint16_t to_read = terminal.first_displayed_char;
    uint16_t read_chars = 0;
    for (;;) {
        if (terminal.terminal_buffer_data[to_read] == NEWLINE) {
            terminal.first_displayed_char = (to_read + 1) % TERMINAL_BUFFER_SIZE;
            break;
        }
        if (read_chars == terminal.allocated_columns + 1) {
            terminal.first_displayed_char = (to_read + 1) % TERMINAL_BUFFER_SIZE;
            break;
        }
        read_chars++;
        to_read = (to_read + 1) % TERMINAL_BUFFER_SIZE;
        if (to_read == terminal.last_char) {
            break; // Unable to scroll down further
        }
    }
}


/* Line separator calculation */
void initialize_line_feeds() {
    terminal.line_feeds.head = 0;
    terminal.line_feeds.tail = 0;
    memset(terminal.line_feeds.positions, 0x0, sizeof(terminal.line_feeds.positions));
}
void record_line_feed(uint16_t pos){
    uint16_t new_tail = terminal.line_feeds.tail;
    if (new_tail == (TERMINAL_BUFFER_SIZE - 1)) {
        new_tail = 0;
    } else {
        new_tail = terminal.line_feeds.tail + 1;
    }

    if (new_tail == terminal.line_feeds.head) {
        delete_first_line();
    }

    terminal.line_feeds.positions[new_tail] = pos;
    terminal.line_feeds.tail = new_tail;
}
void delete_first_line() {  // Zero out the first line in the terminal buffer until line feed or to the maximum columns
    uint16_t first_index = (terminal.line_feeds.head + 1) % TERMINAL_BUFFER_SIZE;
    uint16_t first_line_feed = terminal.line_feeds.positions[first_index];
    uint16_t i = terminal.first_char;
    uint16_t deleted_count = 0;
    for (;;) {
        char deleted = terminal.terminal_buffer_data[i];
        terminal.terminal_buffer_data[i] = 0x0;
        deleted_count++;

        if (deleted == NEWLINE) {
            terminal.line_feeds.head = first_index;
            break;
        } else if (deleted_count == 1+terminal.allocated_columns) { // Anticipate newline chatacter on index allocated_columns
            terminal.terminal_buffer_data[i] = deleted;
            deleted--;
            break;
        }

        if (i == terminal.last_char) {
            terminal_clear();   // To make sure it returns to a clean state
            return;
        }
        if (i == TERMINAL_BUFFER_SIZE - 1) {
            i = 0;
        } else {
            i++;
        }
    }
    terminal.first_char = (terminal.first_char + deleted_count) % TERMINAL_BUFFER_SIZE;
}

/* Put a string into the buffer. Call this function */
void puts_terminal(char* s, uint32_t strlen, uint8_t fg_color, uint8_t bg_color) {
    uint16_t color = (fg_color << 8) | bg_color;

    uint16_t to_write = terminal.last_char;
    for (uint32_t i = 0; i < strlen; i++) {
        to_write++;
        // If the terminal buffer is full, overwrite the first line
        if (to_write >= TERMINAL_BUFFER_SIZE) {
            to_write = 0;
        }

        terminal.terminal_buffer_data[to_write] = s[i];
        terminal.terminal_colors[to_write] = color;
        if (s[i] == '\n') {
            record_line_feed(to_write);
        }

        // Check whether the terminal buffer is full
        uint16_t used_buffer_len = (TERMINAL_BUFFER_SIZE + to_write - terminal.first_char) % TERMINAL_BUFFER_SIZE;
        if (used_buffer_len >= TERMINAL_BUFFER_SIZE - 1) {
            delete_first_line();
        }
    }
    terminal.last_char = to_write;
}


// --------------- history ---------------
static struct History inputs;

void history_init() {
    inputs.hist_length = 0;
    inputs.current_line = 0;
    inputs.viewed_line = 0;

    for (int i = 0; i < MAX_HISTORY; i++) {
        inputs.history[i].length = 0;
    }
}

void add_line_to_history(){
    InputLine* current = &inputs.history[inputs.current_line];
    if (current->length == 0){
        return;
    }

    int8_t duplicate_index = -1;
    for (int8_t i = 0; i < inputs.current_line; i++) {
        if (is_same_line(inputs.history[i], *current)) {
            duplicate_index = i;
            break;
        }
    }

    if (duplicate_index != -1) {
        InputLine temp = inputs.history[duplicate_index];

        for (int i = duplicate_index; i < inputs.current_line - 1; i++) {
            inputs.history[i] = inputs.history[i + 1];
        }

        inputs.history[inputs.current_line - 1] = temp;
        inputs.history[inputs.current_line].length = 0;
    } else {
        inputs.current_line++;
        inputs.hist_length++;
    }
    
    inputs.viewed_line = inputs.current_line;
    inputs.history[inputs.current_line].length = 0;
}

void terminal_line_insert_char(InputLine* line, char c, int index) {
    if (line->length >= MAX_LINE_LENGTH - 1) return; 
    if (index < 0 || index > line->length) return;  

    for (int i = line->length; i > index; i--) {
        line->buffer[i] = line->buffer[i - 1];
    }

    line->buffer[index] = c;
    line->length++;
}

void terminal_line_delete_char(InputLine* line, int index) {
    if (index < 0 || index >= line->length) return;

    for (int i = index; i < line->length - 1; i++) {
        line->buffer[i] = line->buffer[i + 1];
    }

    line->length--;
}

void prev_input() {
    if (inputs.current_line == 0) return;

    if (inputs.viewed_line == inputs.current_line) {
        inputs.viewed_line = inputs.current_line - 1;
    } else if (inputs.viewed_line > 0) {
        inputs.viewed_line--;
    }

}
void next_input() {
    if (inputs.viewed_line == inputs.current_line) {
        return; // Already at the latest input
    }

    if (inputs.viewed_line < inputs.hist_length - 1) {
        inputs.viewed_line++;
    } else {
        inputs.viewed_line = inputs.hist_length - 1;
    }
}

bool is_same_line(InputLine l1, InputLine l2){
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
// --------------- CLI ---------------

void terminal_handle_input(char c){
    uint16_t cursor_index;

    switch ((unsigned char) c) {
        case KEY_UP:
            move_to_last_screen();
            prev_input();
            InputLine* prev_line = &inputs.history[inputs.viewed_line];
            memcpy(inputs.history[inputs.current_line].buffer, prev_line->buffer, prev_line->length);
            inputs.history[inputs.current_line].length = prev_line->length;
            redraw_input(prev_line->buffer, prev_line->length);
            break;
        case KEY_DOWN:
            move_to_last_screen();
            next_input();
            InputLine* next_line = &inputs.history[inputs.viewed_line];
            memcpy(inputs.history[inputs.current_line].buffer, next_line->buffer, next_line->length);
            inputs.history[inputs.current_line].length = next_line->length;
            redraw_input(next_line->buffer, next_line->length);
            break;
        case KEY_LEFT:
            move_to_last_screen();
            cursor_left();
            break;
        case KEY_RIGHT:
            move_to_last_screen();
            cursor_right();
            break;
        case CTRL_KEY_UP:
            scroll_up();
            terminal_flush();
            break;
        case CTRL_KEY_DOWN:
            scroll_down();
            terminal_flush();
            break;
        case '\n':
            puts_terminal("\n", 1, DEFAULT_COLOR, TERMINAL_BACKGROUND);
            InputLine* command = &inputs.history[inputs.current_line];
            execute(command->buffer, command->length);
            add_line_to_history(command);

            print_path();
            move_to_last_screen();
            break;
        case '\b':
            InputLine* current_line = &inputs.history[inputs.current_line];
            cursor_index = terminal.cursor.dist;
            terminal_line_delete_char(current_line, current_line->length - 1);
            redraw_input(current_line->buffer, current_line->length);
            break;
        default:
            InputLine* line = &inputs.history[inputs.current_line];
            cursor_index = terminal.cursor.dist;
            terminal_line_insert_char(line, c, line->length);
            redraw_input(line->buffer, line->length);
            break;
    }    
}


// --------------- utilities ---------------
ParsedInput parse_input_all(const char* input, uint32_t length, char delimiter){
    bool isKutip = false;
    ParsedInput res = {0};

    uint32_t i = 0;
    while(i<length && res.argc < MAX_ARGC){
        // skip leading spaces
        while(i<length && input[i]== delimiter){
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
            while(i<length && input[i]!= delimiter){
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

        memcpy(res.argv[res.argc].buffer, &input[start], word_len);
        res.argv[res.argc].length = word_len;
        res.argc++;
    }
    return res;
}

struct EXT2DirectoryEntry *get_next_directory_entry_shell(struct EXT2DirectoryEntry *entry){
    struct EXT2DirectoryEntry* next = (struct EXT2DirectoryEntry*)((uint8_t*)entry + entry->rec_len);
    return next;
}

char *get_entry_name_shell(void *entry){
    struct EXT2DirectoryEntry *dir_entry = (struct EXT2DirectoryEntry *)entry;
    if(dir_entry->inode==0 || dir_entry->name_len == 0 || dir_entry->name_len > 255){
        return NULL;
    } else{
        char* name = (char*)((uint8_t *)entry + sizeof(struct EXT2DirectoryEntry));
        return name;
    }
}

void get_absolute_path(){
    uint32_t offset = 0;
    for(uint8_t i=0;i<=depth;i++){
        if(i>0){
            memcpy((char*)((uint8_t*)fullpath + offset), "/", 1);
            offset++;
        }
        memcpy((char*)((uint8_t*)fullpath + offset), absolute_path[i].name, absolute_path[i].length);
        offset += absolute_path[i].length;
    }
}

uint32_t get_absolute_path_length(){
    uint32_t res = 0;
    for(uint8_t i=0;i<=depth;i++){
        if(i>0){
            res++; // hitung '/'
        }
        res+= absolute_path[i].length;
    }
    return res;
}

void updateAbsolutePath(){
    for(uint8_t i=1;i<=depth;i++){
        char buffer2[BLOCK_COUNT*BLOCK_SIZE];
        struct EXT2DriverRequest req = {
            .buf                   = buffer2,
            .name                  = absolute_path[i].name,
            .parent_inode          = absolute_path[i-1].inode_num,
            .buffer_size           = 0x100000,
            .name_len              = absolute_path[i].length,
            .is_directory          = 1
        };
        struct EXT2DirectoryEntry new;
        uint32_t retval = 0;
        syscall(12, &req, &new, &retval);
        absolute_path[i].inode_num = new.inode;
    }
}

bool execute(const char* input, uint32_t length){
    Command command_table[] = {
        { "clear",  5},
        { "cd",     2},
        { "mv",     2},
        { "cp",     2},
        { "cat",    3},
        { "ls",     2},
        { "mkdir",  5},
        { "rm",     2},
        { "find",   4},
        { "kill",   4},
        { "ps",     2},
        { "exec",   4},
        { "badapple", 8}
    };

    ParsedInput args = parse_input_n(input, length, 2);
    if(args.argc==0){
        return;
    }

    uint8_t i=0;
    for(i=0;i<NUM_COMMANDS;i++){
        if(memcmp(command_table[i].name, args.argv[0].buffer, args.argv[0].length)==0){
            break;
        }
    }
    if(i<NUM_COMMANDS){
        if(i==0){clear(args.argv[1].buffer, args.argv[1].length);}
        else if(i==1){cd(args.argv[1].buffer, args.argv[1].length);}
        else if(i==2){mv(args.argv[1].buffer, args.argv[1].length);}
        else if(i==3){cp(args.argv[1].buffer, args.argv[1].length);}
        else if(i==4){cat(args.argv[1].buffer, args.argv[1].length);}
        else if(i==5){ls(args.argv[1].buffer, args.argv[1].length);}
        else if(i==6){mkdir(args.argv[1].buffer, args.argv[1].length);}
        else if(i==7){rm(args.argv[1].buffer, args.argv[1].length);}
        else if(i==8){find(args.argv[1].buffer, args.argv[1].length);}
        else if(i==9){kill(args.argv[1].buffer, args.argv[1].length);}
        else if(i==10){ps(args.argv[1].buffer, args.argv[1].length);}
        else if(i==11){exec(args.argv[1].buffer, args.argv[1].length);}
         else if(i==12){badapple(args.argv[1].buffer, args.argv[1].length);}

        return true;
    } else{
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return false;
    }
}


// --------------- commands ---------------
void clear(const char* input, uint32_t length){
    if(length!=0){
        puts_terminal("Invalid arg\n", 12, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    terminal_clear();
}

void cd(const char* input, uint32_t length){
    if(length==0){
        cwd_name[0] = '.';
        cwd_name_len = 1;
        cwd_inode = 2;
        depth = 0;
        return;
    }
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        puts_terminal("Invalid arg\n", 12, 0x4, TERMINAL_BACKGROUND);
        return; // error message?
    }

    uint32_t res;
    existEXT2Arg input_param = {
        .base_inode = cwd_inode,
        .path = args.argv[0].buffer,
        .res_inode = &res,
    };
    uint8_t retval = 2;
    syscall(13, &input_param, &retval, 0);

    if(retval!=0){
        puts_terminal("Directory not found\n", 20, 0x4, TERMINAL_BACKGROUND);
        return;
    } else{
        // parsing sukses
        ParsedInput path = parse_input_all(args.argv[0].buffer, args.argv[0].length, '/');

        for(uint32_t i=0;i<path.argc;i++){
            if(depth>=8){
                break;
            }
            char dotdot[2];
            dotdot[0] ='.';
            dotdot[1] ='.';
            if(path.argv[i].length==1 && memcmp(path.argv[i].buffer, ".", path.argv[i].length)==0){} // nothing happened
            else if(path.argv[i].length==2 && (memcmp(path.argv[i].buffer, dotdot, path.argv[i].length)==0)){
                if(depth!=0){
                    depth--;
                }
            } else{
                depth++;
                memcpy(absolute_path[depth].name, path.argv[i].buffer, path.argv[i].length);
                absolute_path[depth].length = path.argv[i].length;
            }
        }

        cwd_inode = res;
        // uint32_t a = cwd_inode;
        cwd_name_len = absolute_path[depth].length;
        memcpy(cwd_name, absolute_path[depth].name, absolute_path[depth].length);
        updateAbsolutePath();
    }
}

void cat(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    struct EXT2DriverRequest request;
    char buff[5 * BLOCK_SIZE] = {0};
    request.buf = buff;
    request.name = args.argv[0].buffer;
    request.name_len = args.argv[0].length;
    request.is_directory = false;
    request.parent_inode = cwd_inode;
    request.buffer_size = BLOCK_SIZE * BLOCK_COUNT;
    uint8_t is_file_exists = 0;
    syscall(0, &request, &is_file_exists, 0);
    if(is_file_exists != 0) {
        puts_terminal("File not found\n", 15, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    puts_terminal(request.buf, request.buffer_size, DEFAULT_COLOR, TERMINAL_BACKGROUND);
    // syscall(6, args.argv[0].buffer, args.argv[0].length, 0xF);
    uint32_t retFile = 0;

    // bikin request untuk isi dari 
    struct EXT2DriverRequest req;
    req.parent_inode = cwd_inode;
    req.buf = request.buf;
    req.name = ".";
    req.name_len = 1;
    req.is_directory = true;
    req.buffer_size = BLOCK_SIZE;
    syscall(1, (uint32_t)&req, 0, 0);

    // req.buf 
    struct EXT2DirectoryEntry entry = {0};
    char tes2[BLOCK_SIZE];
    struct EXT2DriverRequest request2 = {
        .buf                   = tes2,
        .name                  = args.argv[0].buffer,
        .parent_inode          = cwd_inode,
        .buffer_size           = 0x100000,
        .name_len              = args.argv[0].length,
        .is_directory          = 0
    };
    struct EXT2DirectoryEntry new = {0};
    syscall(12, &request2, &new, 0);
    if (new.inode == 0) {
        puts_terminal("Unknown error\n", 14, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    uint32_t retval = 0;
    syscall(11, new.inode, &retval, 0);

    puts_terminal(request.buf, retval, DEFAULT_COLOR, TERMINAL_BACKGROUND);

}
// --------------- commands/find ---------------

#define DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE 1024

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH 255u
#endif

#ifndef MAX_PATH_DEPTH 
#define MAX_PATH_DEPTH 8u
#endif

struct DirectoryEntrySearchQueueItem {
    struct EXT2DirectoryEntry *entry;
};
struct DirectoryEntrySearchQueue {
    uint32_t head;
    uint32_t tail;
    struct DirectoryEntrySearchQueueItem items[DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE];
};
void desq_create_queue(struct DirectoryEntrySearchQueue* queue) {
    queue->head = 0;
    queue->tail = 0;
}
bool desq_enqueue(struct DirectoryEntrySearchQueue* queue, struct DirectoryEntrySearchQueueItem item) {
    if ((queue->tail + 1) % DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE == queue->head) {
        return false;
    }
    queue->items[queue->tail] = item;
    queue->tail = (queue->tail + 1) % DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE;
    return true;
}
bool desq_dequeue(struct DirectoryEntrySearchQueue* queue, struct DirectoryEntrySearchQueueItem *item) {
    if (queue->head == queue->tail) {
        return false;
    }
    *item = queue->items[queue->head];
    queue->head = (queue->head + 1) % DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE;
    return true;
}
uint32_t desq_queue_size(struct DirectoryEntrySearchQueue* queue) {
    return (DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE + queue->tail - queue->head) % DIRECTORY_ENTRY_SEARCH_QUEUE_SIZE;
}

void find_recurse(char *path, uint32_t* path_len, uint32_t inode, char *target, uint32_t* target_len) {
    /* Directory reading */
    // Prepare this buffer to store directory entries
    // TO MY FUTURE SELF: how do you handle inode with more directory entries
    // REPLY: Query using syscall(11, ...)
    uint16_t ENTRIES_BUFFER_SIZE = BLOCK_SIZE * MAX_PATH_DEPTH;
    // syscall(11, inode, &ENTRIES_BUFFER_SIZE, 0); // sort this out when metadata is ready
    uint8_t entries[ENTRIES_BUFFER_SIZE];

    char dot = '.';
    struct EXT2DriverRequest request = {
        .name = &dot,
        .name_len = 1,
        .parent_inode = inode,
        .buf = entries,
        .buffer_size = ENTRIES_BUFFER_SIZE,
        .is_directory = true,
    };
    int8_t retval;
    syscall(1, (uint32_t)&request, &retval, 0);
    if (retval != 0) return;

    struct DirectoryEntrySearchQueue queue;
    desq_create_queue(&queue);

    struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)entries;
    entry = get_next_directory_entry_shell(entry);
    for (;;) {
        if (entry->rec_len == 0) break;
        
        entry = get_next_directory_entry_shell(entry);
        char *name = (char *)((uint8_t *)entry + sizeof(struct EXT2DirectoryEntry));
        if (memcmp(name, target, entry->name_len) == 0) {   // Found target
           char result[MAX_NAME_LENGTH * MAX_PATH_DEPTH];
           memcpy(result, path, *path_len);
           result[*path_len] = '/';
           memcpy(result + *path_len + 1, name, entry->name_len);
           uint32_t length = *path_len + 1 + entry->name_len;
           
           puts_terminal(result, length, DEFAULT_COLOR, TERMINAL_BACKGROUND);
           puts_terminal("\n", 1, DEFAULT_COLOR, TERMINAL_BACKGROUND);
        }
        
        if (entry->file_type == EXT2_FT_DIR) {
            struct DirectoryEntrySearchQueueItem item = { .entry = entry };
            desq_enqueue(&queue, item);
        }
    }

    struct DirectoryEntrySearchQueueItem item;
    struct EXT2DirectoryEntry *child_entry;
    uint16_t current_path_len = *path_len;
    while (desq_queue_size(&queue) > 0) {
        desq_dequeue(&queue, &item);
        child_entry = item.entry;
        char *child_name = (char *)((uint8_t *)child_entry + sizeof(struct EXT2DirectoryEntry));

        path[current_path_len] = '/';
        memcpy(path + current_path_len + 1, child_name, child_entry->name_len);
        *path_len = current_path_len + 1 + child_entry->name_len;

        find_recurse(path, path_len, child_entry->inode, target, target_len);
    }
}

void find(const char *input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');

    char *target;
    uint32_t target_len;
    for (uint8_t argno; argno < args.argc;) {
        if (memcmp(args.argv[argno].buffer, "-name", args.argv[argno].length) == 0) {
            argno++;
            target = args.argv[argno].buffer;
            target_len = args.argv[argno].length;
        }
        argno++;
    }

    uint8_t MAX_NAME_LEN = 255;
    uint8_t MAX_PATH_PATH_LEN = MAX_NAME_LEN * 8;   // <- Assumption
    
    char path[MAX_PATH_PATH_LEN];
    uint32_t path_len = 1;
    path[0] = '.';
    
    uint32_t inode = 2; // Root inode
    find_recurse(path, &path_len, inode, target, &target_len);
};

void ls(const char* input, uint32_t length) {
    if(length!=0){
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    struct EXT2DriverRequest request;
    if(depth==0){
        request.parent_inode = 2;
    } else{
        request.parent_inode = absolute_path[depth-1].inode_num;
    }
    char buff[512 * 2] = {0};
    char dot = '.';
    request.buf = buff;
    request.name = &dot;
    request.name_len = 1;
    request.is_directory = true;
    request.buffer_size = 512 * 2;
    
    
    int8_t retFile;
    syscall(1, &request, &retFile, 0);
    if(request.buf != NULL && retFile == 0) {
        struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry*)request.buf;
        entry = get_next_directory_entry_shell(entry);  
        entry = get_next_directory_entry_shell(entry);      
        while (entry->inode != 0) {
            char *name = get_entry_name_shell(entry);

            puts_terminal(name, entry->name_len, DEFAULT_COLOR, TERMINAL_BACKGROUND);
            puts_terminal("\n", 1, DEFAULT_COLOR, TERMINAL_BACKGROUND);

            if (entry->rec_len == 0) {
                break;
            }
            entry = get_next_directory_entry_shell(entry);
        }
    } else {
        puts_terminal("Unable to read directory\n", 25, 0x4, TERMINAL_BACKGROUND);
    }
    return;
}

void mkdir(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc != 1) {
        puts_terminal("Usage: mkdir <directory_name>\n", 30, 0x2, TERMINAL_BACKGROUND);
        return;
    }

    // Create directory request
    struct EXT2DriverRequest request = {
        .buf = NULL,
        .name = args.argv[0].buffer,
        .name_len = args.argv[0].length,
        .parent_inode = cwd_inode,
        .buffer_size = 0,
        .is_directory = true
    };

    int8_t result;
    syscall(2, &request, &result, 0);    
    if (result != 0) {
        puts_terminal("Failed to create directory\n", 27, 0x4, TERMINAL_BACKGROUND);
    } else {
        puts_terminal("Directory created successfully\n", 31, 0xE, TERMINAL_BACKGROUND);
    }
}

void rm(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc != 1) {
        puts_terminal("Usage: rm <file_or_directory_name>\n", 35, 0x2, TERMINAL_BACKGROUND);
        return;
    }
    
    // Check if the target exists and determine its type
    char check_buff[BLOCK_SIZE] = {0};
    struct EXT2DirectoryEntry entry = {0};
    struct EXT2DriverRequest check_request = {
        .buf = check_buff,
        .name = args.argv[0].buffer,
        .name_len = args.argv[0].length,
        .parent_inode = cwd_inode,
        .buffer_size = BLOCK_SIZE,
        .is_directory = false  // We'll check if it's a directory later
    };
    
    // Get the directory entry
    syscall(12, &check_request, &entry, 0);
    
    if (entry.inode == 0) {
        // Try again as a directory
        check_request.is_directory = true;
        syscall(12, &check_request, &entry, 0);
        
        if (entry.inode == 0) {
            puts_terminal("File or directory not found\n", 28, 0x4, TERMINAL_BACKGROUND);
            return;
        }
    }
    
    // Check if it's a directory or file
    bool is_directory = (entry.file_type == EXT2_FT_DIR);
    
    if (is_directory) {
        // Directory deletion
        int8_t result = delete_recur_dir(cwd_inode, args.argv[0].buffer);
        
        puts_terminal("Trying to remove directory...\n", 30, 0xE, TERMINAL_BACKGROUND);        
    } else {
        // File deletion
        struct EXT2DriverRequest delete_request = {
            .buf = NULL,
            .name = args.argv[0].buffer,
            .name_len = args.argv[0].length,
            .parent_inode = cwd_inode,
            .buffer_size = 0,
            .is_directory = false
        };

        int8_t result;
        syscall(3, &delete_request, &result, 0);

        puts_terminal("Trying to remove file...\n", 25, 0xE, TERMINAL_BACKGROUND);
    }
}

// Helper function to recursively delete a directory
int8_t delete_recur_dir(uint32_t parent_inode, char* cur_dir_name) {
    size_t cur_name_len = 0;
    while (cur_dir_name[cur_name_len] != '\0' && cur_name_len < MAX_ARG_LEN) cur_name_len++;
    
    uint8_t buffer[BLOCK_SIZE * 16];
    struct EXT2DriverRequest cur_dir = {
        .buf = buffer,
        .name = cur_dir_name,
        .name_len = cur_name_len,
        .parent_inode = parent_inode,
        .buffer_size = BLOCK_SIZE * 16,
        .is_directory = true
    };
    
    // Read directory contents
    int8_t read_res;
    syscall(1, &cur_dir, &read_res, 0);
    if (read_res != 0) return (read_res == -1) ? -1 : 1;
    
    struct EXT2DirectoryEntry* dir_entry = (struct EXT2DirectoryEntry*) buffer;
    uint32_t cur_dir_inode = dir_entry->inode;
    bool all_children_killed = true;
    
    while (true) {
        uint32_t cur_inode = dir_entry->inode;
        char* cur_name = get_entry_name_shell(dir_entry);
        
        if (cur_name == NULL) {
            // Invalid entry, move to next
            struct EXT2DirectoryEntry* temp_entry = get_next_directory_entry_shell(dir_entry);
            if (temp_entry == dir_entry) break;
            dir_entry = temp_entry;
            continue;
        }
        
        // Skip . and .. entries
        if ((dir_entry->name_len == 1 && cur_name[0] == '.') || 
            (dir_entry->name_len == 2 && cur_name[0] == '.' && cur_name[1] == '.')) {
            struct EXT2DirectoryEntry* temp_entry = get_next_directory_entry_shell(dir_entry);
            if (temp_entry == dir_entry) break;
            dir_entry = temp_entry;
            continue;
        }
        
        // Process based on file type
        if (dir_entry->file_type == EXT2_FT_DIR) {
            // Recursively delete subdirectory
            int8_t delete_res = delete_recur_dir(cur_dir_inode, cur_name);
            if (delete_res != 0) all_children_killed = false;
        } else if (dir_entry->file_type == EXT2_FT_REG_FILE) {
            // Delete regular file
            struct EXT2DriverRequest delete_req = {
                .buf = NULL,
                .name = cur_name,
                .name_len = dir_entry->name_len,
                .parent_inode = cur_dir_inode,
                .buffer_size = 0,
                .is_directory = false
            };
            int8_t delete_res;
            syscall(3, &delete_req, &delete_res, 0);
            if (delete_res != 0) all_children_killed = false;
        }
        
        // Move to next entry
        struct EXT2DirectoryEntry* temp_entry = get_next_directory_entry_shell(dir_entry);
        if (temp_entry == dir_entry) break;
        dir_entry = temp_entry;
    }
    
    // Delete the directory itself if all contents were successfully deleted
    if (all_children_killed) {
        struct EXT2DriverRequest delete_req = {
            .buf = NULL,
            .name = cur_dir_name,
            .name_len = cur_name_len,
            .parent_inode = parent_inode,
            .buffer_size = 0,
            .is_directory = true
        };
        int8_t delete_res;
        syscall(3, &delete_req, &delete_res, 0);
        return delete_res;
    } else {
        return 1;
    }
}

void cp(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=2){
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    struct EXT2CopyRequest copy = {
        .root_inode = cwd_inode,
        .source = args.argv[0].buffer,
        .destination = args.argv[1].buffer,
    };
    
    int retval = 0;
    syscall(14, &copy, &retval, 0);
    if(retval==1){
        puts_terminal("Source not found\n", 17, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==2){
        puts_terminal("Destination not found\n", 22, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==3){
        puts_terminal("Invalid parent\n", 15, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==4){
        puts_terminal("Failed to read source\n", 22, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==5){
        puts_terminal("Failed to write to destination\n", 31, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==6){
        puts_terminal("Directory child read/write failed\n", 34, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==-1){
        puts_terminal("Unknown error\n", 14, 0x4, TERMINAL_BACKGROUND);
    }
}

void mv(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=2){
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    struct EXT2CopyRequest copy = {
        .root_inode = cwd_inode,
        .source = args.argv[0].buffer,
        .destination = args.argv[1].buffer,
    };
    
    int retval = 0;
    syscall(15, &copy, &retval, 0);
    if(retval==1){
        puts_terminal("Source not found\n", 17, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==2){
        puts_terminal("Destination not found\n", 22, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==3){
        puts_terminal("Invalid parent\n", 15, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==4){
        puts_terminal("Failed to read source\n", 22, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==5){
        puts_terminal("Failed to write to destination\n", 31, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==6){
        puts_terminal("Directory child read/write failed\n", 34, 0x4, TERMINAL_BACKGROUND);
    } else if(retval==-1){
        puts_terminal("Unknown error\n", 14, 0x4, TERMINAL_BACKGROUND);
    }
}


void ps(const char* input, uint32_t length){
    char head[7] = "ID NAME";
    puts_terminal(head, 7, 0x2, TERMINAL_BACKGROUND);
    puts_terminal("\n", 1, 0x2, TERMINAL_BACKGROUND);

    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=0){
        puts_terminal("Command not found\n", 18, 0x4, TERMINAL_BACKGROUND);
        return;
    }
    struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];
    struct ProcessManagerState process_manager_state;

    syscall(16, (uint32_t)&_process_list, (uint32_t)&process_manager_state, 0);

    for (uint32_t i = 0; i < sizeof(process_manager_state.process_used); i++) {
        if (process_manager_state.process_used[i]){
            uint32_t name_len = _process_list[i].metadata.length;
            char id[2];
            int pid = (int)_process_list[i].metadata.pid;
            id[0] = (pid / 10) + '0'; // tens place
            id[1] = (pid % 10) + '0'; // units place
            char* name = _process_list[i].metadata.name;
            
            char teks[3 + name_len];
            teks[0] = id[0];
            teks[1] = id[1];
            teks[2] = ' ';
            for (uint32_t j = 0; j < name_len; j++) {
                teks[3 + j] = name[j];
            }

            puts_terminal(teks, 3 + name_len, 0x2, TERMINAL_BACKGROUND);
            puts_terminal("\n", 1, 0x2, TERMINAL_BACKGROUND);
        }
    }
}

void exec(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        puts_terminal("Command not found\n", 18, 0x2, TERMINAL_BACKGROUND);
        return;
    }
    char tes2[BLOCK_SIZE];
    // char copy_name[args.argv[0].length];
    // for(uint32_t i=0;i<args.argv[0].length;i++){
    //     copy_name[i] = args.argv[0].buffer[i];
    // }

    // struct EXT2DriverRequest requested_process = {
    //     .buf                   = tes2,
    //     .name                  = copy_name,
    //     .parent_inode          = cwd_inode,
    //     .buffer_size           = 0x100000,
    //     .name_len              = args.argv[0].length,
    //     .is_directory          = 0
    // };
    /*
    #define PROCESS_CREATE_SUCCESS                   0
    #define PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED 1
    #define PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT   2
    #define PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY    3
    #define PROCESS_CREATE_FAIL_FS_READ_FAILURE      4*/
    uint32_t retval;
    syscall(18, &args.argv[0].buffer, args.argv[0].length,&retval);
    if(retval==0){
        puts_terminal("Create success\n", 15, 0xE, TERMINAL_BACKGROUND);
    } else if (retval==1){
        puts_terminal("Maximum process count exceeded\n", 31, 0x2, TERMINAL_BACKGROUND);
    } else if (retval==2){
        puts_terminal("Invalid entrypoint\n", 19, 0x2, TERMINAL_BACKGROUND);
    } else if (retval==3){
        puts_terminal("Not enough memory\n", 18, 0x2, TERMINAL_BACKGROUND);
    } else if (retval==4){
        puts_terminal("Failed to read file\n", 20, 0x2, TERMINAL_BACKGROUND);
    }
}

void kill(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        puts_terminal("Command not found\n", 18, 0x2, TERMINAL_BACKGROUND);
        return;
    }
    int pid = strToInt(args.argv[0].buffer, args.argv[0].length);
    if(pid < 0 || pid >= PROCESS_COUNT_MAX) {
        puts_terminal("Invalid PID\n", 12, 0x2, TERMINAL_BACKGROUND);
        return;
    }
    bool is_process_running = false;
    syscall(19, pid, &is_process_running, 0);
    if (!is_process_running) {
        puts_terminal("Process not found\n", 18, 0x2, TERMINAL_BACKGROUND);
        return;
    } else {
        puts_terminal("Process killed successfully\n", 28, 0xE, TERMINAL_BACKGROUND);
        syscall(21, 0,0,0);
        return;
    }
}

int strToInt(char* str, uint32_t length){
    int result = 0;
    for(uint32_t i=0;i<length;i++){
        result = result*10 + (str[i] - '0');
    }
    return result;
}

void intToString(uint32_t num, char* str, uint32_t length) {
    // Safety check
    if (length == 0) return;

    // Null terminator at the end
    str[length] = '\0';

    for (int i = length - 1; i >= 0; i--) {
        str[i] = (num % 10) + '0';
        num /= 10;
    }
}

int decompress_frame(char* compressed, int compressed_len, char* output, int max_out_len) {
    int out_idx = 0;
    for (int i = 0; i < compressed_len && out_idx < max_out_len;) {
        char c = compressed[i++];
        int count = 0;

        // parse number
        while (i < compressed_len && compressed[i] >= '0' && compressed[i] <= '9') {
            count = count * 10 + (compressed[i++] - '0');
        }
        if (count == 0) count = 1;

        for (int j = 0; j < count && out_idx < max_out_len; j++) {
            output[out_idx++] = c;
        }
    }
    return out_idx;  // return the number of output characters
}


void badapple(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');

    // GET TEXT
    char bad[BLOCK_SIZE * 5000];
    struct EXT2DriverRequest applereq = {
        .name = "bafc12.txt",
        .name_len = 10,
        .parent_inode = 2,
        .buffer_size = BLOCK_SIZE * 5000,
        .is_directory = false,
        .buf = bad,
    };
    uint8_t res = 0;
    syscall(0, &applereq, &res, 0);
    if(res != 0) {
        puts_terminal("Bad Apple text not found\n", 24, 0x4, TERMINAL_BACKGROUND);
        return;
    }

    // PRINTING
    int i = 0;
    int frames = 0;
    while (i < BLOCK_SIZE * 1316) {
        char decompressed[2000];
        char compressed[1024];  // max needed is small (~8-10 chars for RLE)
        int comp_len = 0;
        int out_len = 0;

        // Fill compressed chunk until decompressed output is 2000 characters
        while (out_len < 2000 && (i + comp_len) < BLOCK_SIZE * 1316) {
            compressed[comp_len++] = bad[i + comp_len];
            out_len = decompress_frame(compressed, comp_len, decompressed, 2000);
        }

        i += comp_len;

        // Fully decompress into final buffer
        decompress_frame(compressed, comp_len, decompressed, 2000);
        puts_terminal(decompressed, out_len, 0xF, TERMINAL_BACKGROUND);

        for (volatile int delay = 0; delay < 5000000; delay++);
    }
    clear("", 0);
}