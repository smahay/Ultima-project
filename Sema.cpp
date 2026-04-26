#include "Sema.h"
#include <cstdio>
#include <pthread.h>

using namespace std;

static pthread_mutex_t sema_global_lock = PTHREAD_MUTEX_INITIALIZER;

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
    bool was_queued = false;

    while (true)
    {
        pthread_mutex_lock(&sema_global_lock);

        // Already owner.
        if (taskID == lucky_task)
        {
            pthread_mutex_unlock(&sema_global_lock);
            return;
        }

        // Acquire the resource now.
        if (sema_value >= 1)
        {
            sema_value--;
            lucky_task = taskID;
            pthread_mutex_unlock(&sema_global_lock);
            dump(1);
            return;
        }

        // Queue once and block until scheduler runs us again.
        if (!was_queued)
        {
            sema_queue.En_Q(taskID);
            was_queued = true;
        }
        pthread_mutex_unlock(&sema_global_lock);

        if (sched_ptr != NULL)
        {
            sched_ptr->set_state(taskID, BLOCKED);
        }
        dump(1);

        if (sched_ptr != NULL && sched_ptr->get_task_id() == taskID)
        {
            sched_ptr->yield();
        }

        if (sched_ptr != NULL)
        {
            tcb *task = sched_ptr->find_task(taskID);
            sched_ptr->wait_until_running(task);
        }
    }
}

// Release or handoff.
void semaphore::up(int taskID)
{
    pthread_mutex_lock(&sema_global_lock);

    // Only owner can up().
    if (taskID == lucky_task)
    {
        // No waiters.
        if (sema_queue.isEmpty())
        {
            sema_value++;
            lucky_task = -1;
            pthread_mutex_unlock(&sema_global_lock);
            dump(1);
        }
        else
        {
            // Wake next waiter.
            int task_id = sema_queue.De_Q();
            lucky_task = task_id;
            pthread_mutex_unlock(&sema_global_lock);

            if (sched_ptr != NULL)
            {
                sched_ptr->set_state(task_id, READY);
            }
            if (log_win != NULL)
            {
                char buff[256];
                snprintf(buff, sizeof(buff),
                         " Unblocking task_id %d and release from the queue\n",
                         task_id);
                write_window(log_win, buff);
            }
            dump(1);

            // Yield after handoff.
            if (sched_ptr != NULL)
            {
                sched_ptr->yield();
            }
            dump(1);
        }
    }
    else
    {
        pthread_mutex_unlock(&sema_global_lock);

        if (log_win != NULL)
        {
            char buff[256];
            snprintf(buff, sizeof(buff),
                     " Invalid Semaphore UP(). TaskID:%d does not own the resource\n",
                     taskID);
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

    pthread_mutex_lock(&sema_global_lock);

    out += " --------- SEMAPHORE DUMP ---------\n";

    switch (level)
    {
        case 0:
            snprintf(buff, sizeof(buff), " Sema_Value: %d\n", sema_value);
            out += buff;
            out += " Sema_Name: ";
            out += resource_name;
            out += "\n";
            snprintf(buff, sizeof(buff), " Obtained by Task-ID: %d\n", lucky_task);
            out += buff;
            break;

        case 1:
            snprintf(buff, sizeof(buff), " Sema_Value: %d\n", sema_value);
            out += buff;
            out += " Sema_Name: ";
            out += resource_name;
            out += "\n";
            snprintf(buff, sizeof(buff), " Obtained by Task-ID: %d\n", lucky_task);
            out += buff;
            out += " Sema-Queue: ";
            out += sema_queue.to_string();
            out += "\n";
            break;

        default:
            out += " ERROR in SEMAPHORE DUMP level\n";
            break;
    }

    pthread_mutex_unlock(&sema_global_lock);

    out += " ----------------------------------\n";

    char out_buffer[4096];
    int i = 0;
    while (i < (int) out.length() && i < 4095)
    {
        out_buffer[i] = out[i];
        i++;
    }
    out_buffer[i] = '\0';

    write_window(log_win, out_buffer);
}
