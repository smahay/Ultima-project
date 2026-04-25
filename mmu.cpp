#include "mmu.h"

mmu::mmu(int size, char default_initial_value, int page_size)
{
    memory_size = size;
    block_size = page_size;
    next_handle = 1;
    log_win = NULL;

    if (memory_size <= 0)
    {
        memory_size = 1024;
    }

    if (block_size <= 0)
    {
        block_size = 64;
    }

    memory = new char[memory_size];

    for (int i = 0; i < memory_size; i++)
    {
        memory[i] = default_initial_value;
    }

    head = new MemBlock;
    head->status = "Free";
    head->handle = 0;
    head->start = 0;
    head->end = memory_size - 1;
    head->size = memory_size;
    head->current_location = -1;
    head->task_id = -1;
    head->next = NULL;
}

mmu::~mmu()
{
    delete[] memory;

    while (head != NULL)
    {
        MemBlock* temp = head;
        head = head->next;
        delete temp;
    }
}

void mmu::set_log_window(WINDOW *win)
{
    log_win = win;
}

void mmu::output_line(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    if (log_win != NULL)
    {
        write_window(log_win, text);
    }
    else
    {
        cout << text;
    }
}

int mmu::round_up(int size)
{
    if (size <= 0)
    {
        return 0;
    }

    if (size % block_size == 0)
    {
        return size;
    }

    return ((size / block_size) + 1) * block_size;
}

MemBlock* mmu::find_block(int task_id, int memory_handle)
{
    MemBlock* curr = head;

    while (curr != NULL)
    {
        if (curr->status == "Used" &&
            curr->handle == memory_handle &&
            curr->task_id == task_id)
        {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

void mmu::split_block(MemBlock* block, int requested_size)
{
    if (block == NULL)
    {
        return;
    }

    if (block->size <= requested_size)
    {
        return;
    }

    MemBlock* newBlock = new MemBlock;

    newBlock->status = "Free";
    newBlock->handle = 0;
    newBlock->start = block->start + requested_size;
    newBlock->end = block->end;
    newBlock->size = newBlock->end - newBlock->start + 1;
    newBlock->current_location = -1;
    newBlock->task_id = -1;
    newBlock->next = block->next;

    block->end = block->start + requested_size - 1;
    block->size = requested_size;
    block->next = newBlock;
}

int mmu::Mem_Alloc(int task_id, int size)
{
    int needed = round_up(size);
    char buff[256];

    if (needed <= 0 || needed > memory_size)
    {
        snprintf(buff, sizeof(buff),
                 " MMU Alloc FAILED: Task %d requested invalid size %d\n",
                 task_id, size);
        output_line(buff);
        return -1;
    }

    MemBlock* curr = head;

    while (curr != NULL)
    {
        if (curr->status == "Free" && curr->size >= needed)
        {
            split_block(curr, needed);

            curr->status = "Used";
            curr->handle = next_handle++;
            curr->task_id = task_id;
            curr->current_location = curr->start;

            for (int i = curr->start; i <= curr->end; i++)
            {
                memory[i] = '.';
            }

            snprintf(buff, sizeof(buff),
                     " MMU Alloc: Task %d received handle %d, bytes %d, range %d-%d\n",
                     task_id, curr->handle, curr->size, curr->start, curr->end);
            output_line(buff);

            return curr->handle;
        }

        curr = curr->next;
    }

    snprintf(buff, sizeof(buff),
             " MMU Alloc FAILED: Task %d requested %d bytes but not enough contiguous memory exists.\n",
             task_id, size);
    output_line(buff);

    return -1;
}

int mmu::Mem_Free(int task_id, int memory_handle)
{
    MemBlock* block = find_block(task_id, memory_handle);
    char buff[256];

    if (block == NULL)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d tried to free invalid handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    for (int i = block->start; i <= block->end; i++)
    {
        memory[i] = '#';
    }

    snprintf(buff, sizeof(buff),
             " MMU Free: Task %d freed handle %d, range %d-%d\n",
             task_id, memory_handle, block->start, block->end);
    output_line(buff);

    block->status = "Free";
    block->handle = 0;
    block->task_id = -1;
    block->current_location = -1;

    Mem_Coalesce();

    return 0;
}

int mmu::Mem_Read(int task_id, int memory_handle, char* ch)
{
    MemBlock* block = find_block(task_id, memory_handle);
    char buff[256];

    if (block == NULL)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d tried to read invalid handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    if (block->current_location > block->end)
    {
        snprintf(buff, sizeof(buff),
                 " MMU Read FAILED: Task %d reached LIMIT for handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    *ch = memory[block->current_location];
    block->current_location++;

    return 0;
}

int mmu::Mem_Write(int task_id, int memory_handle, char ch)
{
    MemBlock* block = find_block(task_id, memory_handle);
    char buff[256];

    if (block == NULL)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d tried to write invalid handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    if (block->current_location > block->end)
    {
        snprintf(buff, sizeof(buff),
                 " MMU Write FAILED: Task %d reached LIMIT for handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    memory[block->current_location] = ch;
    block->current_location++;

    return 0;
}

int mmu::Mem_Read(int task_id, int memory_handle, int offset_from_beg, int text_size, char* text)
{
    MemBlock* block = find_block(task_id, memory_handle);
    char buff[256];

    if (block == NULL)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d tried to read invalid handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    if (text == NULL || offset_from_beg < 0 || text_size < 0)
    {
        return -1;
    }

    int read_start = block->start + offset_from_beg;
    int read_end = read_start + text_size - 1;

    if (read_start < block->start || read_end > block->end)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d read outside BASE/LIMIT for handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    for (int i = 0; i < text_size; i++)
    {
        text[i] = memory[read_start + i];
    }

    text[text_size] = '\0';

    return 0;
}

int mmu::Mem_Write(int task_id, int memory_handle, int offset_from_beg, int text_size, char* text)
{
    MemBlock* block = find_block(task_id, memory_handle);
    char buff[256];

    if (block == NULL)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d tried to write invalid handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    if (text == NULL || offset_from_beg < 0 || text_size < 0)
    {
        return -1;
    }

    int write_start = block->start + offset_from_beg;
    int write_end = write_start + text_size - 1;

    if (write_start < block->start || write_end > block->end)
    {
        snprintf(buff, sizeof(buff),
                 " Segmentation Fault: Task %d write outside BASE/LIMIT for handle %d\n",
                 task_id, memory_handle);
        output_line(buff);
        return -1;
    }

    for (int i = 0; i < text_size; i++)
    {
        memory[write_start + i] = text[i];
    }

    snprintf(buff, sizeof(buff),
             " MMU Write: Task %d wrote %d bytes to handle %d at offset %d\n",
             task_id, text_size, memory_handle, offset_from_beg);
    output_line(buff);

    return 0;
}

int mmu::Mem_Left()
{
    int total = 0;
    MemBlock* curr = head;

    while (curr != NULL)
    {
        if (curr->status == "Free")
        {
            total += curr->size;
        }

        curr = curr->next;
    }

    return total;
}

int mmu::Mem_Largest()
{
    int largest = 0;
    MemBlock* curr = head;

    while (curr != NULL)
    {
        if (curr->status == "Free" && curr->size > largest)
        {
            largest = curr->size;
        }

        curr = curr->next;
    }

    return largest;
}

int mmu::Mem_Smallest()
{
    int smallest = -1;
    MemBlock* curr = head;

    while (curr != NULL)
    {
        if (curr->status == "Free")
        {
            if (smallest == -1 || curr->size < smallest)
            {
                smallest = curr->size;
            }
        }

        curr = curr->next;
    }

    if (smallest == -1)
    {
        return 0;
    }

    return smallest;
}

int mmu::Mem_Coalesce()
{
    int merges = 0;
    MemBlock* curr = head;

    while (curr != NULL && curr->next != NULL)
    {
        if (curr->status == "Free" && curr->next->status == "Free")
        {
            MemBlock* temp = curr->next;

            curr->end = temp->end;
            curr->size = curr->end - curr->start + 1;
            curr->next = temp->next;

            for (int i = curr->start; i <= curr->end; i++)
            {
                memory[i] = '.';
            }

            delete temp;
            merges++;
        }
        else
        {
            curr = curr->next;
        }
    }

    if (merges > 0)
    {
        output_line(" MMU Coalesce: neighboring free blocks were merged.\n");
    }

    return merges;
}

void mmu::Mem_Dump(int starting_from, int num_bytes)
{
    char buff[256];

    if (starting_from < 0 || starting_from >= memory_size)
    {
        output_line(" Invalid dump starting location.\n");
        return;
    }

    if (num_bytes <= 0)
    {
        output_line(" Invalid dump size.\n");
        return;
    }

    int ending = starting_from + num_bytes;

    if (ending > memory_size)
    {
        ending = memory_size;
    }

    output_line("\n Memory CORE Dump:\n");

    string line = " ";

    for (int i = starting_from; i < ending; i++)
    {
        line += memory[i];

        if ((i + 1) % 64 == 0)
        {
            line += "\n";
            output_line(line.c_str());
            line = " ";
        }
    }

    if (line.length() > 1)
    {
        line += "\n";
        output_line(line.c_str());
    }

    snprintf(buff, sizeof(buff),
             " Memory Left: %d | Largest Free: %d | Smallest Free: %d\n",
             Mem_Left(), Mem_Largest(), Mem_Smallest());
    output_line(buff);
}

void mmu::Print_Table()
{
    char buff[256];

    output_line("\n Memory Usage Table:\n");
    output_line(" Status    Handle    Start      End      Size    Current   Task-ID\n");

    MemBlock* curr = head;

    while (curr != NULL)
    {
        char current_text[16];
        char task_text[16];

        if (curr->current_location == -1)
        {
            snprintf(current_text, sizeof(current_text), "NA");
        }
        else
        {
            snprintf(current_text, sizeof(current_text), "%d", curr->current_location);
        }

        if (curr->task_id == -1)
        {
            snprintf(task_text, sizeof(task_text), "MMU");
        }
        else
        {
            snprintf(task_text, sizeof(task_text), "%d", curr->task_id);
        }

        snprintf(buff, sizeof(buff),
                 " %-8s  %-8d  %-8d  %-8d  %-6d  %-8s  %-8s\n",
                 curr->status.c_str(),
                 curr->handle,
                 curr->start,
                 curr->end,
                 curr->size,
                 current_text,
                 task_text);
        output_line(buff);

        curr = curr->next;
    }

    output_line("\n");
}
