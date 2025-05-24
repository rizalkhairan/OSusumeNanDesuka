#ifndef _TERMINAL_H
#define _TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_LINE_LENGTH 1000
#define MAX_HISTORY 64

typedef struct {
    char buffer[MAX_LINE_LENGTH];
    int length;
} InputLine;

typedef struct {
    InputLine history[MAX_HISTORY];
    int hist_length;
    int current_line; 
    int viewed_line;
    int current_line_row;
    int current_line_col;
    int cursor_row;
    int cursor_col;
} InputBuffer;

static InputBuffer terminal_buffer;

void terminal_initialize();
void write_path(bool after_command);
void redraw_current_line();
void terminal_handle_input(char);
void add_line_to_history();
bool is_same_line(TerminalLine, TerminalLine);
void terminal_line_insert_char(InputLine*, char, int);
void terminal_line_delete_char(InputLine*, int);

// --------------- utilities  ---------------
#define MAX_ARGC 8
#define MAX_ARG_LEN 255

typedef struct{
    char buffer[MAX_ARG_LEN];
    int length;
} Arg;

typedef struct{
    int argc;
    Arg argv[MAX_ARGC];
} ParsedInput;

typedef struct{
    const char* name;
    int length;
} Command;

typedef struct {
    uint32_t base_inode;
    char *path;
    uint32_t *res_inode;
} existEXT2Arg;

typedef struct{
    char name[255];
    uint32_t length;
    uint32_t inode_num;
} Path;

ParsedInput parse_input_all(const char *input, uint32_t length, char delimiter);
ParsedInput parse_input_n(const char *input, uint32_t length, int n);

struct EXT2DirectoryEntry *get_next_directory_entry_shell(struct EXT2DirectoryEntry *entry);
char *get_entry_name_shell(void *entry);
void get_absolute_path();
uint32_t get_absolute_path_length();
void updateAbsolutePath();

int8_t delete_recur_dir(uint32_t parent_inode, char* cur_dir_name);

// --------------- commands ---------------
bool execute(const char* line, uint32_t length);
void clear(const char* input, uint32_t length);
void cd(const char* input, uint32_t length);
void cat(const char* input, uint32_t length);
void ls(const char* input, uint32_t length);
void mkdir(const char* input, uint32_t length);
void rm(const char* input, uint32_t length);

#endif;