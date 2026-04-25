#ifndef MMU_H
#define MMU_H

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdio>
#include <ncurses.h>

using namespace std;

// Provided by Ultima.cpp
void write_window(WINDOW *Win, const char* text);

struct MemBlock {
    string status;
    int handle;
    int start;
    int end;
    int size;
    int current_location;
    int task_id;
    MemBlock* next;
};

class mmu {
private:
    char* memory;
    int memory_size;
    int block_size;
    int next_handle;
    MemBlock* head;
    WINDOW *log_win;

    int round_up(int size);
    MemBlock* find_block(int task_id, int memory_handle);
    void split_block(MemBlock* block, int requested_size);
    void output_line(const char *text);

public:
    mmu(int size = 1024, char default_initial_value = '.', int page_size = 64);
    ~mmu();

    void set_log_window(WINDOW *win);

    int Mem_Alloc(int task_id, int size);
    int Mem_Free(int task_id, int memory_handle);

    int Mem_Read(int task_id, int memory_handle, char* ch);
    int Mem_Write(int task_id, int memory_handle, char ch);

    int Mem_Read(int task_id, int memory_handle, int offset_from_beg, int text_size, char* text);
    int Mem_Write(int task_id, int memory_handle, int offset_from_beg, int text_size, char* text);

    int Mem_Left();
    int Mem_Largest();
    int Mem_Smallest();
    int Mem_Coalesce();

    void Mem_Dump(int starting_from = 0, int num_bytes = 1024);
    void Print_Table();
};

#endif
