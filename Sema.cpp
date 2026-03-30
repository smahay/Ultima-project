#include "Sema.h"
#include <cstdio>

using namespace std;

// Constructor.
semaphore::semaphore(int starting_value, string name, scheduler *theScheduler)
{
    sema_value = starting_value;
    resource_name = name;
    lucky_task = -1;
    sched_ptr = theScheduler;
    log_win = NULL;
}

semaphore::~semaphore()
{
}

// Set log window.
void semaphore::set_log_window(WINDOW *win)
{
    log_win = win;
}

// Acquire or block.
void semaphore::down(int taskID)
{
    // Already owner.
    if (taskID == lucky_task)
    {
        if (log_win != NULL)
        {
            char buff[256];
            sprintf(buff, " Task # %d already owns the resource! Ignore request.\n", lucky_task);
            write_window(log_win, buff);
        }

        dump(1);
        return;
    }

    // Acquire resource.
    if (sema_value >= 1)
    {
        sema_value--;
        lucky_task = taskID;
        dump(1);
        return;
    }

    // Queue and block.
    sema_queue.En_Q(taskID);
    sched_ptr->set_state(taskID, BLOCKED);
    dump(1);

    // Switch now.
    if (sched_ptr->get_task_id() == taskID)
    {
        sched_ptr->yield();
    }

    dump(1);
}

// Release or handoff.
void semaphore::up()
{
    // Only owner can up().
    if (sched_ptr->get_task_id() == lucky_task)
    {
        // No waiters.
        if (sema_queue.isEmpty())
        {
            sema_value++;
            lucky_task = -1;
            dump(1);
        }
        else
        {
            // Wake next waiter.
            int task_id = sema_queue.De_Q();
            sched_ptr->set_state(task_id, READY);
            if (log_win != NULL)
            {
                char buff[256];
                sprintf(buff, " Unblocking task_id %d and release from the queue\n", task_id);
                write_window(log_win, buff);
            }

            lucky_task = task_id;
            dump(1);

            // Yield after handoff.
            sched_ptr->yield();
            dump(1);
        }
    }
    else
    {
        if (log_win != NULL)
        {
            char buff[256];
            sprintf(buff, " Invalid Semaphore UP(). TaskID:%d does not own the resource\n", sched_ptr->get_task_id());
            write_window(log_win, buff);
        }

        dump(1);
    }
}

// Print semaphore info.
void semaphore::dump(int level)
{
    if (log_win == NULL)
    {
        return;
    }

    string out;
    char buff[256];

    out += " --------- SEMAPHORE DUMP ---------\n";

    switch (level)
    {
        case 0:
            sprintf(buff, " Sema_Value: %d\n", sema_value);
            out += buff;
            sprintf(buff, " Sema_Name: %s\n", resource_name.c_str());
            out += buff;
            sprintf(buff, " Obtained by Task-ID: %d\n", lucky_task);
            out += buff;
            break;

        case 1:
            sprintf(buff, " Sema_Value: %d\n", sema_value);
            out += buff;
            sprintf(buff, " Sema_Name: %s\n", resource_name.c_str());
            out += buff;
            sprintf(buff, " Obtained by Task-ID: %d\n", lucky_task);
            out += buff;
            out += " Sema-Queue: ";
            out += sema_queue.to_string();
            out += "\n";
            break;

        default:
            out += " ERROR in SEMAPHORE DUMP level\n";
            break;
    }

    out += " ----------------------------------\n";
    write_window(log_win, out.c_str());
}
