#include "Sched.h"
#include <cstdio>
#include <unistd.h>

using namespace std;

// Init table.
scheduler::scheduler(int table_size)
{
    current_task = -1;
    next_available_task_id = 0;
    current_quantum = 300;
    log_win = NULL;
    max_tasks = table_size;

    if (max_tasks <= 0)
    {
        max_tasks = 1;
    }

    task_table = new tcb[max_tasks];

    for (int i = 0; i < max_tasks; i++)
    {
        task_table[i].task_id = -1;
        task_table[i].task_name = "";
        task_table[i].state = DEAD;
        task_table[i].start_time = 0;
        task_table[i].thread = 0;
        task_table[i].task_win = NULL;
        task_table[i].kill_signal = false;
        task_table[i].work_counter = 0;
    }
}

// Reset fields.
scheduler::~scheduler()
{
    pthread_mutex_lock(&sched_lock);
    current_task = -1;
    next_available_task_id = 0;
    max_tasks = 0;
    task_table = NULL;
    pthread_mutex_unlock(&sched_lock);
}

// Set quantum.
void scheduler::set_quantum(long quantum)
{
    current_quantum = quantum;
}

// Get quantum.
long scheduler::get_quantum()
{
    return current_quantum;
}

// Set log window.
void scheduler::set_log_window(WINDOW *win)
{
    log_win = win;
}

// Find by id.
tcb *scheduler::find_task(int the_taskid)
{
    if (the_taskid < 0)
    {
        return NULL;
    }

    for (int i = 0; i < next_available_task_id; i++)
    {
        if (task_table[i].task_id == the_taskid)
        {
            return &task_table[i];
        }
    }

    return NULL;
}

// Set state.
void scheduler::set_state(int the_taskid, string the_state)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);
    if (task != NULL)
    {
        task->state = the_state;
    }

    pthread_mutex_unlock(&sched_lock);
}

// Get state.
string scheduler::get_state(int the_taskid)
{
    pthread_mutex_lock(&sched_lock);

    string result = DEAD;

    tcb *task = find_task(the_taskid);
    if (task != NULL)
    {
        result = task->state;
    }

    pthread_mutex_unlock(&sched_lock);
    return result;
}

// Current index.
int scheduler::get_task_id()
{
    pthread_mutex_lock(&sched_lock);
    int result = current_task;
    pthread_mutex_unlock(&sched_lock);
    return result;
}

// Current task ptr.
tcb *scheduler::get_current_task()
{
    pthread_mutex_lock(&sched_lock);

    tcb *result = NULL;

    if (current_task >= 0 && current_task < next_available_task_id)
    {
        result = &task_table[current_task];
    }

    pthread_mutex_unlock(&sched_lock);
    return result;
}

// Add task.
tcb *scheduler::create_task(const string &task_name, WINDOW *task_win)
{
    pthread_mutex_lock(&sched_lock);

    // Capacity check.
    if (next_available_task_id >= max_tasks)
    {
        if (log_win != NULL)
        {
            char buff[256];
            sprintf(buff, " Create_task() FAILED: Available tasks exceeded. MAX_TASKS = %d\n", max_tasks);
            write_window(log_win, buff);
        }

        pthread_mutex_unlock(&sched_lock);
        return NULL;
    }

    int idx = next_available_task_id;

    // Init new slot.
    task_table[idx].task_id = idx;
    task_table[idx].task_name = task_name;
    task_table[idx].state = READY;
    task_table[idx].start_time = 0;
    task_table[idx].task_win = task_win;
    task_table[idx].kill_signal = false;
    task_table[idx].work_counter = 0;

    if (log_win != NULL)
    {
        char buff[256];
        sprintf(buff, " Creating task # %d (%s)\n", idx, task_name.c_str());
        write_window(log_win, buff);
    }

    next_available_task_id++;

    pthread_mutex_unlock(&sched_lock);
    return &task_table[idx];
}

// Start scheduler.
void scheduler::start()
{
    pthread_mutex_lock(&sched_lock);

    if (next_available_task_id == 0)
    {
        if (log_win != NULL)
        {
            write_window(log_win, " There are no tasks to start!\n");
        }

        pthread_mutex_unlock(&sched_lock);
        return;
    }

    if (log_win != NULL)
    {
        write_window(log_win, " ................\n");
        write_window(log_win, " ................SCHEDULING STARTED\n");
        write_window(log_win, " ................\n");
    }

    current_task = 0;
    task_table[current_task].start_time = clock();
    task_table[current_task].state = RUNNING;

    if (next_available_task_id > 0)
    {
        set_quantum(1000 / next_available_task_id);
    }

    pthread_mutex_unlock(&sched_lock);
}

// Wait until RUNNING.
void scheduler::wait_until_running(tcb *task)
{
    while (true)
    {
        pthread_mutex_lock(&sched_lock);

        if (task == NULL)
        {
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        if (task->state == RUNNING || task->state == DEAD || task->kill_signal)
        {
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        pthread_mutex_unlock(&sched_lock);
        sleep(1);
    }
}

// Round-robin switch.
void scheduler::yield()
{
    pthread_mutex_lock(&sched_lock);

    if (current_task < 0 || current_task >= next_available_task_id)
    {
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    int counter = 0;
    char buff[256];

    sprintf(buff, " Current Task # %d is trying to yield\n", current_task);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    // Blocked/dead fast path.
    if (task_table[current_task].state == BLOCKED || task_table[current_task].state == DEAD)
    {
        int next_task = (current_task + 1) % next_available_task_id;

        while (task_table[next_task].state != READY && counter < next_available_task_id)
        {
            next_task = (next_task + 1) % next_available_task_id;
            counter++;
        }

        if (counter < next_available_task_id && task_table[next_task].state == READY)
        {
            current_task = next_task;
            task_table[current_task].start_time = clock();
            task_table[current_task].state = RUNNING;
        }
        else
        {
            current_task = -1;
            if (log_win != NULL)
            {
                write_window(log_win, " POSSIBLE DEAD LOCK\n");
            }
        }

        pthread_mutex_unlock(&sched_lock);
        return;
    }

    // Time slice used.
    clock_t elapsed_time = clock() - task_table[current_task].start_time;

    sprintf(buff, " Task: %d, elapsed_time: %ld\n", current_task, (long) elapsed_time);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    sprintf(buff, " Current Quantum: %ld\n", current_quantum);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    if (elapsed_time >= current_quantum)
    {
        if (task_table[current_task].state == RUNNING)
        {
            task_table[current_task].state = READY;
        }

        // Find next READY.
        int next_task = (current_task + 1) % next_available_task_id;

        while (task_table[next_task].state != READY && counter < next_available_task_id - 1)
        {
            next_task = (next_task + 1) % next_available_task_id;
            counter++;
        }

        if (counter < next_available_task_id && task_table[next_task].state == READY)
        {
            current_task = next_task;
            task_table[current_task].start_time = clock();
            task_table[current_task].state = RUNNING;

            sprintf(buff, " Started Running task # %d\n", current_task);
            if (log_win != NULL)
            {
                write_window(log_win, buff);
            }
        }
        else
        {
            if (log_win != NULL)
            {
                write_window(log_win, " POSSIBLE DEAD LOCK\n");
            }
        }
    }
    else
    {
        sprintf(buff, " No Yield! (task: %d still has some quantum left)\n", current_task);
        if (log_win != NULL)
        {
            write_window(log_win, buff);
        }
    }

    pthread_mutex_unlock(&sched_lock);
}

// Print table.
void scheduler::dump(int level)
{
    pthread_mutex_lock(&sched_lock);

    string out;
    char buff[256];

    out += " ---------------- PROCESS TABLE ---------------\n";
    sprintf(buff, " Quantum = %ld\n", current_quantum);
    out += buff;
    out += " Task-Name\tTask-ID\tState\n";

    if (next_available_task_id == 0)
    {
        out += " No tasks in scheduler.\n";
        out += " ----------------------------------------------\n";

        pthread_mutex_unlock(&sched_lock);

        if (log_win != NULL)
        {
            write_window(log_win, out.c_str());
        }

        return;
    }

    for (int i = 0; i < next_available_task_id; i++)
    {
        sprintf(buff, " %s\t\t%d\t%s",
                task_table[i].task_name.c_str(),
                task_table[i].task_id,
                task_table[i].state.c_str());
        out += buff;

        if (i == current_task)
        {
            out += " <-- CURRENT PROCESS";
        }

        if (level > 1)
        {
            sprintf(buff, " [work=%d]", task_table[i].work_counter);
            out += buff;
        }

        out += "\n";
    }

    out += " ----------------------------------------------\n";

    pthread_mutex_unlock(&sched_lock);

    if (log_win != NULL)
    {
        write_window(log_win, out.c_str());
    }
}

// Kill task.
void scheduler::kill_task(int the_taskid)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);
    if (task == NULL)
    {
        if (log_win != NULL)
        {
            char buff[256];
            sprintf(buff, " kill_task FAILED: Task %d not found.\n", the_taskid);
            write_window(log_win, buff);
        }

        pthread_mutex_unlock(&sched_lock);
        return;
    }

    task->kill_signal = true;
    task->state = DEAD;

    if (log_win != NULL)
    {
        char buff[256];
        sprintf(buff, " Killing task # %d\n", the_taskid);
        write_window(log_win, buff);
    }

    if (current_task == the_taskid)
    {
        int counter = 0;
        int next_task = (current_task + 1) % next_available_task_id;

        while (task_table[next_task].state != READY && counter < next_available_task_id)
        {
            next_task = (next_task + 1) % next_available_task_id;
            counter++;
        }

        if (counter < next_available_task_id && task_table[next_task].state == READY)
        {
            current_task = next_task;
            task_table[current_task].state = RUNNING;
            task_table[current_task].start_time = clock();
        }
        else
        {
            current_task = -1;
        }
    }

    pthread_mutex_unlock(&sched_lock);
}

// Remove DEAD tasks.
void scheduler::garbage_collect()
{
    pthread_mutex_lock(&sched_lock);

    if (next_available_task_id == 0)
    {
        if (log_win != NULL)
        {
            write_window(log_win, " No tasks in scheduler.\n");
        }

        pthread_mutex_unlock(&sched_lock);
        return;
    }

    if (log_win != NULL)
    {
        write_window(log_win, " Running garbage collector...\n");
    }

    int write_idx = 0;

    // Pack live tasks.
    for (int read_idx = 0; read_idx < next_available_task_id; read_idx++)
    {
        if (task_table[read_idx].state != DEAD)
        {
            if (write_idx != read_idx)
            {
                task_table[write_idx] = task_table[read_idx];
                task_table[write_idx].task_id = write_idx;
            }

            write_idx++;
        }
    }

    // Clear tail slots.
    for (int i = write_idx; i < max_tasks; i++)
    {
        task_table[i].task_id = -1;
        task_table[i].task_name = "";
        task_table[i].state = DEAD;
        task_table[i].start_time = 0;
        task_table[i].thread = 0;
        task_table[i].task_win = NULL;
        task_table[i].kill_signal = false;
        task_table[i].work_counter = 0;
    }

    next_available_task_id = write_idx;

    if (next_available_task_id == 0)
    {
        current_task = -1;
    }
    else
    {
        current_task = 0;

        for (int i = 0; i < next_available_task_id; i++)
        {
            if (task_table[i].state == RUNNING)
            {
                current_task = i;
                break;
            }
        }
    }

    pthread_mutex_unlock(&sched_lock);
}
