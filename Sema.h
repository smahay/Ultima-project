#ifndef SEMA_H
#define SEMA_H

#include <iostream>
#include <string>
#include "Queue.h"
#include "Sched.h"

using namespace std;

// Binary semaphore.
class semaphore
{
private:
    // Resource name.
    string resource_name;
    // 1 free, 0 busy.
    int sema_value;
    // Current owner id.
    int lucky_task;

    // Wait queue.
    Queue<int> sema_queue;
    // Scheduler link.
    scheduler *sched_ptr;
    // Log window.
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
