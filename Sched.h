#ifndef SCHED_H
#define SCHED_H

#include <iostream>
#include <string>
#include <ctime>
#include <pthread.h>
#include <ncurses.h>

void write_log(const char *text);

enum TaskState
{
    READY,
    RUNNING,
    BLOCKED,
    DEAD
};

std::string state_to_string(TaskState state);

struct tcb
{
    int task_id;
    std::string task_name;
    TaskState state;
    clock_t start_time;
    pthread_t thread;
    WINDOW *task_win;
    bool kill_signal;
    int work_counter;

    pthread_cond_t run_cond;

    tcb *next;
};

class scheduler
{
    private:
        tcb *head;
        tcb *current_task;
        int current_quantum;
        int next_available_task_id;
        int total_tasks;
        pthread_mutex_t sched_lock;

        tcb *find_next_ready_task(tcb *start_task);
        void switch_to_task(tcb *next_task);
        void remove_head_dead_tasks();
        void remove_internal_dead_tasks();

    public:
        scheduler();
        ~scheduler();

        void set_quantum(long quantum);
        long get_quantum();

        tcb *find_task(int the_taskid);
        void set_state(int the_taskid, TaskState the_state);
        TaskState get_state(int the_taskid);

        int get_task_id();
        tcb *get_current_task();
        tcb *create_task(const std::string &task_name, WINDOW *task_win);

        void start();
        void yield();
        void kill_task(int the_taskid);
        void garbage_collect();

        void dump();
        std::string dump_to_string();

        void wait_until_running(tcb *task);
        void wake_task(tcb *task);
};

#endif