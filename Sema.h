#ifndef SEMA_H
#define SEMA_H

#include <iostream>
#include <string>
#include "Queue.h"
#include "Sched.h"

class semaphore
{
private:
    std::string resource_name;
    int sema_value;
    int owner_task_id;

    Queue<int> sema_queue;
    scheduler *sched_ptr;

public:
    semaphore(int starting_value, std::string name, scheduler *theScheduler);
    ~semaphore();

    bool down(int taskID);
    void up();

    void dump(int level);
    std::string dump_to_string(int level);
};

#endif
