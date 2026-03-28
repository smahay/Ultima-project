#include "Sema.h"
#include <cstdio>

using namespace std;

semaphore::semaphore(int starting_value, string name, scheduler *theScheduler)
{
    sema_value = starting_value;
    resource_name = name;
    owner_task_id = -1;
    sched_ptr = theScheduler;
}

semaphore::~semaphore()
{
}

bool semaphore::down(int taskID)
{
    // Phase 1: task already owns this resource.
    if (taskID == owner_task_id)
    {
        char buff[256];
        sprintf(buff, "Task # %d already owns the resource.\n", owner_task_id);
        write_log(buff);
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
    sprintf(buff, "Task %d blocked on semaphore %s\n", taskID, resource_name.c_str());
    write_log(buff);
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
        write_log("Invalid semaphore UP(). Current task does not own the resource.\n");
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
    sprintf(buff, "Unblocking task %d from semaphore queue.\n", next_owner_id);
    write_log(buff);
    dump(1);

    sched_ptr->yield();
}

std::string semaphore::dump_to_string(int level)
{
    std::string out;
    char buff[256];
    out += "---------------- SEMAPHORE DUMP ----------------\n";

    switch (level)
    {
        case 0:
            sprintf(buff, "Resource: %s\n", resource_name.c_str());
            out += buff;
            sprintf(buff, "Sema_Value: %d\n", sema_value);
            out += buff;
            sprintf(buff, "Owner Task-ID: %d\n", owner_task_id);
            out += buff;
            break;

        case 1:
            sprintf(buff, "Resource: %s\n", resource_name.c_str());
            out += buff;
            sprintf(buff, "Sema_Value: %d\n", sema_value);
            out += buff;
            sprintf(buff, "Owner Task-ID: %d\n", owner_task_id);
            out += buff;
            out += "Sema-Queue: ";
            out += sema_queue.to_string();
            out += "\n";
            break;

        default:
            out += "ERROR in semaphore dump level\n";
    }

    out += "------------------------------------------------\n";
    return out;
}

void semaphore::dump(int level)
{
    std::string text = dump_to_string(level);
    write_log(text.c_str());
}
