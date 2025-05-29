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

void terminal_handle_input(char);
void add_line_to_history();
bool is_same_line(TerminalLine, TerminalLine);
void terminal_line_insert_char(InputLine*, char, int);
void terminal_line_delete_char(InputLine*, int);


// --------------- CLI (Rizal's version) ---------------
#define TERMINAL_BUFFER_SIZE 0x8000
#define HOSTNAME_COLOR 0xB
#define DEFAULT_COLOR 0xF
#define TERMINAL_BACKGROUND 0x0
#define NEWLINE '\n'

struct TerminalBuffer {
    char terminal_buffer_data[TERMINAL_BUFFER_SIZE];    // Stream of characters. Will overwrite the first characters in a circular buffer fashion
    uint16_t terminal_colors[TERMINAL_BUFFER_SIZE];     // 2 bytes per character (foreground and then background, interleaved)

    struct LineFeeds {
        uint16_t positions[TERMINAL_BUFFER_SIZE]; // Points to all characters in the buffer
        uint16_t head;  // Point to the index before the first line feed
        uint16_t tail;
    } line_feeds;
    
    // These pointers is able to index the entirety of the buffer
    uint16_t first_char;    // Point to the first saved character in the terminal buffer
    uint16_t last_char;     // Point to the last saved character in the terminal buffer
    uint16_t first_displayed_char;  // Point to the first character in the currently displayed terminal buffer
    uint16_t end_screen;    // Points to the first character in the last screen
    uint16_t input_char;    // Point to the character before the first inputted string
    
    uint8_t allocated_rows;
    uint8_t allocated_columns;
    uint16_t screen_size;

    struct Cursor {
        uint8_t row;    // Current row of the cursor
        uint8_t col;    // Current column of the cursor
        uint16_t dist;  // Distance from input_char
    } cursor;

    uint8_t last_flushed_row;
    uint8_t last_flushed_col;
};

void terminal_init();
void terminal_clear();
void terminal_flush();
void scroll_up();
void scroll_down();
void move_to_last_screen();
void redraw_input(char* command, uint32_t command_length);
void print_path();
void initialize_line_feeds();
void calculate_last_screen();
void record_line_feed(uint16_t pos);
void delete_first_line();
void puts_terminal(char* s, uint32_t strlen, uint8_t fg_color, uint8_t bg_color);

void set_cursor(uint16_t dist);
void cursor_left();
void cursor_right();

// --------------- history ---------------
struct History {
    InputLine history[MAX_HISTORY];
    uint8_t hist_length;
    uint8_t current_line;   // Index of the current line
    uint8_t viewed_line;
};
void prev_input();
void next_input();

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