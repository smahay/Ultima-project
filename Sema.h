#ifndef SEMA_H
#define SEMA_H

#include <iostream>
#include <string>
#include "Queue.h"
#include "Sched.h"

using namespace std;

class semaphore
{
private:
    string resource_name;
    int sema_value;
    int lucky_task;

    Queue<int> sema_queue;
    scheduler *sched_ptr;
    WINDOW *log_win;

public:
    semaphore(int starting_value, string name, scheduler *theScheduler);
    ~semaphore();
    void set_log_window(WINDOW *win);

    void down(int taskID);
    void up();

    void dump(int level);
};

#endif
