#ifndef SCHED_H
#define SCHED_H

#include <iostream>
#include <string>
#include <ctime>
#include <pthread.h>
#include <ncurses.h>

using namespace std;

// Shared window writer.
void write_window(WINDOW * Win, const char* text);

// Task states.
const string READY = "READY";
const string RUNNING = "RUNNING";
const string BLOCKED = "BLOCKED";
const string DEAD = "DEAD";

// Task record.
struct tcb
{
    int task_id;
    string task_name;
    string state;
    clock_t start_time;
    pthread_t thread;
    WINDOW *task_win;
    bool kill_signal;
    int work_counter;
};

class scheduler
{
    private:
        // Running index.
        int current_task;
        // Time slice.
        long current_quantum;
        // Active slots.
        int next_available_task_id;
        // Table capacity.
        int max_tasks;
        // Dynamic table.
        tcb *task_table;
        // Log window.
        WINDOW *log_win;
        // Scheduler lock.
        pthread_mutex_t sched_lock = PTHREAD_MUTEX_INITIALIZER;

    public:
        scheduler(int table_size);
        ~scheduler();

        void set_quantum(long quantum);
        long get_quantum();
        void set_log_window(WINDOW *win);

        tcb *find_task(int the_taskid);
        void set_state(int the_taskid, string the_state);
        string get_state(int the_taskid);

        int get_task_id();
        tcb *get_current_task();
        tcb *create_task(const string &task_name, WINDOW *task_win);

        void start();
        void yield();
        void kill_task(int the_taskid);
        void garbage_collect();

        void dump(int level = 1);

        void wait_until_running(tcb *task);
};

#endif
