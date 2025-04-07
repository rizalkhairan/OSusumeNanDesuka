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

void terminal_initialize();
void terminal_handle_input(char);
void add_line_to_history();
bool is_same_line(TerminalLine, TerminalLine);
void terminal_line_insert_char(TerminalLine*, char, int);
void terminal_line_delete_char(TerminalLine*, int);

#endif