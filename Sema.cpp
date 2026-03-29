#include "Sema.h"
#include <cstdio>

using namespace std;

semaphore::semaphore(int starting_value, string name, scheduler *theScheduler)
{
    sema_value = starting_value;
    resource_name = name;
    owner_task_id = -1;
    sched_ptr = theScheduler;
    log_win = NULL;
}

semaphore::~semaphore()
{
}

void semaphore::set_log_window(WINDOW *win)
{
    log_win = win;
}

bool semaphore::down(int taskID)
{
    // Phase 1: task already owns this resource.
    if (taskID == owner_task_id)
    {
        char buff[256];
        sprintf(buff, " Task # %d already owns the resource.\n", owner_task_id);
        if (log_win != NULL)
        {
            write_window(log_win, buff);
        }
        dump(1);
        return true;
    }

    // Phase 2: resource is free, so acquire it immediately.
    if (sema_value >= 1)
    {
        sema_value--;
        owner_task_id = taskID;
        dump(1);
        return true;
    }

    // Phase 3: resource is busy, so queue and block.
    sema_queue.En_Q(taskID);
    sched_ptr->set_state(taskID, BLOCKED);
    char buff[256];
    sprintf(buff, " Task %d blocked on semaphore %s\n", taskID, resource_name.c_str());
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }
    dump(1);

    if (sched_ptr->get_task_id() == taskID)
    {
        sched_ptr->yield();
    }

    return false;
}

void semaphore::up()
{
    // Phase 1: only the owner can release.
    if (sched_ptr->get_task_id() != owner_task_id)
    {
        if (log_win != NULL)
        {
            write_window(log_win, " Invalid semaphore UP(). Current task does not own the resource.\n");
        }
        dump(1);
        return;
    }

    // Phase 2: if nobody is waiting, just release.
    if (sema_queue.isEmpty())
    {
        sema_value++;
        owner_task_id = -1;
        dump(1);
        return;
    }

    // Phase 3: transfer ownership to next queued task.
    int next_owner_id = sema_queue.De_Q();
    sched_ptr->set_state(next_owner_id, READY);
    owner_task_id = next_owner_id;

    char buff[256];
    sprintf(buff, " Unblocking task %d from semaphore queue.\n", next_owner_id);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }
    dump(1);

    sched_ptr->yield();
}

void semaphore::dump(int level)
{
    std::string out;
    char buff[256];
    out += " ---------------- SEMAPHORE DUMP ----------------\n";

    if (level == 0)
    {
        sprintf(buff, " Resource: %s\n", resource_name.c_str());
        out += buff;
        sprintf(buff, " Sema_Value: %d\n", sema_value);
        out += buff;
        sprintf(buff, " Owner Task-ID: %d\n", owner_task_id);
        out += buff;
    }
    else if (level == 1)
    {
        sprintf(buff, " Resource: %s\n", resource_name.c_str());
        out += buff;
        sprintf(buff, " Sema_Value: %d\n", sema_value);
        out += buff;
        sprintf(buff, " Owner Task-ID: %d\n", owner_task_id);
        out += buff;
        out += " Sema-Queue: ";
        out += sema_queue.to_string();
        out += "\n";
    }
    else
    {
        out += " ERROR in semaphore dump level\n";
    }

    out += " ------------------------------------------------\n";
    if (log_win != NULL)
    {
        write_window(log_win, out.c_str());
    }
}
