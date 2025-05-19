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
} TerminalLine;

typedef struct {
    TerminalLine history[MAX_HISTORY];
    int hist_length;
    int current_line; 
    int viewed_line;
    int current_line_row;
    int current_line_col;
    int cursor_row;
    int cursor_col;
} TerminalBuffer;

static TerminalBuffer terminal_buffer;

void terminal_initialize();
void terminal_handle_input(char);
void add_line_to_history();
bool is_same_line(TerminalLine, TerminalLine);
void terminal_line_insert_char(TerminalLine*, char, int);
void terminal_line_delete_char(TerminalLine*, int);

// --------------- utilities for parsing commands ---------------
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
    int min_args;
    int max_args;
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
void execute(const char* line, uint32_t length);
void split_path(const char *path, char *parent_out, char *leaf_out);

uint32_t get_absolute_path_length();
#endif;