#include <stdint.h>
#include "header/filesystem/ext2.h"
#include "header/terminal/terminal.h"
#include "header/text/framebuffer.h"
#include "header/keyboard/keyboard.h"

#define BLOCK_COUNT 16

Path absolute_path[8] = {0};
uint8_t depth = 0;
char fullpath[2040];
uint32_t fullpath_length = 1;

uint32_t cwd_inode = 2;
char cwd_name[255];
uint16_t cwd_name_len;
uint32_t filepath_len = 22;
static TerminalBuffer terminal_buffer;

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
};
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

    terminal_initialize();
    syscall(7, 0, 0, 0);
    // execute("cd kusanagi", 11);
    // execute("cp yoisaki asahina", 18);
    while(true){
        char c;
        syscall(4, &c, 0, 0);
        if(c){
            terminal_handle_input(c);
        }
    }

    return 0;
}

// --------------- CLI ---------------

void terminal_initialize(){
    syscall(8, 0, 0, 0);

    char* filepath = "OSusumeWaNanDesuka?:.$ ";
    syscall(6, filepath, filepath_len, 0xB);

    // Set cursor correct position
    terminal_buffer.hist_length = 0;
    terminal_buffer.current_line = 0;
    terminal_buffer.viewed_line = 0;
    terminal_buffer.current_line_row = 0; // will change depending on offset
    terminal_buffer.current_line_col = filepath_len + 1; // will change depending on offset
    terminal_buffer.cursor_row = terminal_buffer.current_line_row;
    terminal_buffer.cursor_col = terminal_buffer.current_line_col;
    TerminalLine* line = &terminal_buffer.history[0];
    line->length = 0;
    
    syscall(9, terminal_buffer.cursor_row, terminal_buffer.cursor_col, 0);
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
}

void write_path(){
    char result[2040+filepath_len];
    char* filepath = "OSusumeWaNanDesuka?:";
    TerminalLine* line = &terminal_buffer.history[terminal_buffer.current_line];
    get_absolute_path();
    fullpath_length = get_absolute_path_length();
    
    memcpy(result, filepath, 20);
    memcpy(result + 20, fullpath, fullpath_length);
    memcpy(result + 20 + fullpath_length, "$ ", 2);

    // filepath_len = 22 + fullpath_length;

    terminal_buffer.current_line_col = 0;
    terminal_buffer.current_line_row += (line->length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    syscall(6, result, 22 + fullpath_length, 0xB);
    terminal_buffer.current_line_col = 22 + fullpath_length;
   
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
        char* name = (char*)(entry + sizeof(struct EXT2DirectoryEntry));
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

void execute(const char* input, uint32_t length){
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
    } else{
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x4);
    }
}


// --------------- commands ---------------
void clear(const char* input, uint32_t length){
    if(length!=0){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "invalid arg", 11, 0x4);
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
        depth = 0;
        return;
    }
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "invalid arg", 11, 0x4);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "invalid arg2", 12, 0x4);
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
    
    get_absolute_path();
    fullpath_length = get_absolute_path_length();
    // filepath_len = 22 + fullpath_length;
}

void cat(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=1){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x4);
        return;
    }
    struct EXT2DriverRequest request;
    char buff[BLOCK_COUNT * BLOCK_SIZE] = {0};
    request.buf = buff;
    request.name = args.argv[0].buffer;
    request.name_len = args.argv[0].length;
    request.is_directory = false;
    request.parent_inode = cwd_inode;
    request.buffer_size = BLOCK_SIZE * BLOCK_COUNT;
    uint8_t is_file_exists = 0;
    syscall(0, &request, &is_file_exists, 0);
    if(is_file_exists != 0) {
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "File not found", 14, 0x4);
        return;
    }
    terminal_buffer.current_line_col = 0;
    terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    syscall(6, request.buf, request.buffer_size, 0xE);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "unknown error", 13, 0x4);
        return;
    }
    uint32_t retval = 0;
    uint8_t *point = &retval;
    syscall(11, &new.inode, point, 0);

    // terminal_buffer.current_line_col = 0;
    // terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
    // syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    // syscall(6, cwd_name, cwd_name_len, 0xD);
}

// --------------- comands/find ---------------

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
           
           terminal_buffer.current_line_col = 0;
           terminal_buffer.current_line_row += (entry->name_len + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
           syscall(10, (uint32_t)&terminal_buffer, 0, 0);
           syscall(6, result, length, 0x2);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x4);
        return;
    }
    struct EXT2DriverRequest request;
    uint32_t retFile = 0;
    if(depth==0){
        request.parent_inode = 2;
    } else{
        request.parent_inode = absolute_path[depth-1].inode_num;
    }
    char buff[BLOCK_COUNT * BLOCK_SIZE] = {0};
    request.buf = buff;
    request.name = cwd_name;
    request.name_len = cwd_name_len;
    request.is_directory = true;
    request.buffer_size = BLOCK_COUNT * BLOCK_SIZE;

    syscall(1, (uint32_t)&request, &retFile, 0);
    if(request.buf != NULL) {
        struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry*)request.buf;
        if(entry->inode != -1) {
            char *name = get_entry_name_shell(entry);
            terminal_buffer.current_line_col = 0;
            terminal_buffer.current_line_row += (entry->name_len + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
            syscall(10, (uint32_t)&terminal_buffer, 0, 0);
            syscall(6, (uint32_t)name, entry->name_len, 0x7);
        }
        while (entry->rec_len != 0) {
            entry = get_next_directory_entry_shell(entry);
            if(entry->inode != -1) {
                char *name = get_entry_name_shell(entry);
                terminal_buffer.current_line_col = 0;
                terminal_buffer.current_line_row += (entry->name_len + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
                syscall(10, (uint32_t)&terminal_buffer, 0, 0);
                syscall(6, (uint32_t)name, entry->name_len, 0x7);
            }
        }
    } else {
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (2 + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "unable to read directory", 24, 0x4);
    }
}

void mkdir(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc != 1) {
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "Usage: mkdir <directory_name>", 29, 0x2);
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

    // Check result
    terminal_buffer.current_line_col = 0;
    terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
    syscall(10, (uint32_t)&terminal_buffer, 0, 0);
    
    if (result != 0) {
        syscall(6, "Failed to create directory", 26, 0x4);
    } else {
        syscall(6, "Directory created successfully", 30, 0xE);
    }
}

void rm(const char* input, uint32_t length) {
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc != 1) {
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "Usage: rm <file_or_directory_name>", 33, 0x2);
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
            terminal_buffer.current_line_col = 0;
            terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
            syscall(10, (uint32_t)&terminal_buffer, 0, 0);
            syscall(6, "File or directory not found", 27, 0x4);
            return;
        }
    }
    
    // Check if it's a directory or file
    bool is_directory = (entry.file_type == EXT2_FT_DIR);
    
    if (is_directory) {
        // Directory deletion
        int8_t result = delete_recur_dir(cwd_inode, args.argv[0].buffer);
        
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        
        syscall(6, "Trying to remove directory...", 29, 0xE);
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

        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (args.argv[0].length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH;
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);    
        
        syscall(6, "Trying to remove file...", 24, 0xE);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x2);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "source not found", 16, 0x2);
    } else if(retval==2){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "destination not found", 21, 0x2);
    } else if(retval==3){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "invalid parent", 14, 0x2);
    } else if(retval==4){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "failed to read source", 21, 0x2);
    } else if(retval==5){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "failed to write to destination", 30, 0x2);
    } else if(retval==6){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "directory child read/write failed", 33, 0x2);
    } else if(retval==-1){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "unknown error", 13, 0x2);
    }
}

void mv(const char* input, uint32_t length){
    ParsedInput args = parse_input_all(input, length, ' ');
    if(args.argc!=2){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "command not found", 17, 0x2);
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
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "source not found", 16, 0x2);
    } else if(retval==2){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "destination not found", 21, 0x2);
    } else if(retval==3){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "invalid parent", 14, 0x2);
    } else if(retval==4){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "failed to read source", 21, 0x2);
    } else if(retval==5){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "failed to write to destination", 30, 0x2);
    } else if(retval==6){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "directory child read/write failed", 33, 0x2);
    } else if(retval==-1){
        terminal_buffer.current_line_col = 0;
        terminal_buffer.current_line_row += (length + filepath_len + FRAMEBUFFER_ROW_LENGTH - 1)/FRAMEBUFFER_ROW_LENGTH; // TODO: VALIDASI TEMBUS LAYAR
        syscall(10, (uint32_t)&terminal_buffer, 0, 0);
        syscall(6, "unknown error", 13, 0x2);
    }
}
