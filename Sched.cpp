#include "Sched.h"
#include <cstdio>
#include <unistd.h>

using namespace std;

scheduler::scheduler()
{
    head = NULL;
    current_task = NULL;
    next_available_task_id = 0;
    current_quantum = 300;
    total_tasks = 0;
    log_win = NULL;
}

scheduler::~scheduler()
{
    pthread_mutex_lock(&sched_lock);

    head = NULL;
    current_task = NULL;
    total_tasks = 0;

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::set_quantum(long quantum)
{
    current_quantum = quantum;
}

long scheduler::get_quantum()
{
    return current_quantum;
}

void scheduler::set_log_window(WINDOW *win)
{
    log_win = win;
}

tcb *scheduler::find_task(int the_taskid)
{
    if (head == NULL)
    {
        return NULL;
    }

    if (head->task_id == the_taskid)
    {
        return head;
    }

    tcb *temp = head->next;

    while (temp != head)
    {
        if (temp->task_id == the_taskid)
        {
            return temp;
        }
        temp = temp->next;
    }

    return NULL;
}

void scheduler::set_state(int the_taskid, TaskState the_state)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);
    if (task != NULL)
    {
        task->state = the_state;
    }

    pthread_mutex_unlock(&sched_lock);
}

TaskState scheduler::get_state(int the_taskid)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);

    TaskState result = DEAD;
    if (task != NULL)
    {
        result = task->state;
    }

    pthread_mutex_unlock(&sched_lock);
    return result;
}

int scheduler::get_task_id()
{
    pthread_mutex_lock(&sched_lock);

    int result = -1;

    if (current_task != NULL)
    {
        result = current_task->task_id;
    }

    pthread_mutex_unlock(&sched_lock);
    return result;
}

tcb *scheduler::get_current_task()
{
    return current_task;
}

tcb *scheduler::create_task(const string &task_name, WINDOW *task_win)
{
    pthread_mutex_lock(&sched_lock);

    tcb *new_task = new tcb;

    new_task->task_id = next_available_task_id;
    new_task->task_name = task_name;
    new_task->state = READY;
    new_task->start_time = 0;
    new_task->task_win = task_win;
    new_task->kill_signal = false;
    new_task->work_counter = 0;
    new_task->next = NULL;

    char buff[256];
    sprintf(buff, " Creating task # %d (%s)\n", next_available_task_id, task_name.c_str());
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    if (head == NULL)
    {
        head = new_task;
        new_task->next = head;
    }
    else
    {
        tcb *temp = head;

        while (temp->next != head)
        {
            temp = temp->next;
        }

        temp->next = new_task;
        new_task->next = head;
    }

    next_available_task_id++;
    total_tasks++;

    pthread_mutex_unlock(&sched_lock);
    return new_task;
}

void scheduler::start()
{
    pthread_mutex_lock(&sched_lock);

    if (head == NULL)
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
        write_window(log_win, " ................\n\n");
    }

    current_task = head;
    current_task->start_time = clock();
    current_task->state = RUNNING;

    if (total_tasks > 0)
    {
        set_quantum(1000 / total_tasks);
    }

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::wait_until_running(tcb *task)
{
    while (true)
    {
        pthread_mutex_lock(&sched_lock);

        if (task->state == RUNNING || task->state == DEAD || task->kill_signal)
        {
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        pthread_mutex_unlock(&sched_lock);
        sleep(1);
    }
}

void scheduler::yield()
{
    pthread_mutex_lock(&sched_lock);

    if (current_task == NULL)
    {
        if (log_win != NULL)
        {
            write_window(log_win, " No current task to yield.\n");
        }
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    char buff[256];
    sprintf(buff, " Current Task # %d is trying to yield...\n", current_task->task_id);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    if (current_task->state == BLOCKED || current_task->state == DEAD)
    {
        tcb *temp = current_task->next;

        while (temp != current_task)
        {
            if (temp->state == READY)
            {
                current_task = temp;
                current_task->start_time = clock();
                current_task->state = RUNNING;
                pthread_mutex_unlock(&sched_lock);
                return;
            }

            temp = temp->next;
        }

        pthread_mutex_unlock(&sched_lock);
        return;
    }

    if (current_task->state == RUNNING)
    {
        current_task->state = READY;
    }

    tcb *temp = current_task->next;

    while (temp != current_task)
    {
        if (temp->state == READY)
        {
            current_task = temp;
            current_task->start_time = clock();
            current_task->state = RUNNING;
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        temp = temp->next;
    }

    if (current_task->state == READY)
    {
        current_task->start_time = clock();
        current_task->state = RUNNING;
    }

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::dump(int level)
{
    pthread_mutex_lock(&sched_lock);

    std::string out;
    char buff[256];
    const char *state_text = "";

    out += " ---------------- PROCESS TABLE ---------------\n";
    sprintf(buff, " Quantum = %d\n", current_quantum);
    out += buff;
    out += " Task-Name\tTask-ID\tState\n";

    if (head == NULL)
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

    tcb *temp = head;
    do
    {
        if (temp->state == READY)
        {
            state_text = "READY";
        }
        else if (temp->state == RUNNING)
        {
            state_text = "RUNNING";
        }
        else if (temp->state == BLOCKED)
        {
            state_text = "BLOCKED";
        }
        else
        {
            state_text = "DEAD";
        }

        sprintf(buff, " %s\t\t%d\t%s",
                temp->task_name.c_str(),
                temp->task_id,
                state_text);
        out += buff;

        if (temp == current_task)
        {
            out += " <-- CURRENT PROCESS";
        }

        if (level > 1)
        {
            sprintf(buff, " [work=%d]", temp->work_counter);
            out += buff;
        }

        out += "\n";
        temp = temp->next;
    }
    while (temp != head);

    out += " ----------------------------------------------\n";

    pthread_mutex_unlock(&sched_lock);
    if (log_win != NULL)
    {
        write_window(log_win, out.c_str());
    }
}

void scheduler::kill_task(int the_taskid)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);

    if (task == NULL)
    {
        char buff[256];
        sprintf(buff, " kill_task FAILED: Task %d not found.\n", the_taskid);
        if (log_win != NULL)
        {
            write_window(log_win, buff);
        }
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    char buff[256];
    sprintf(buff, " Killing task # %d\n", the_taskid);
    if (log_win != NULL)
    {
        write_window(log_win, buff);
    }

    task->kill_signal = true;
    task->state = DEAD;

    if (task == current_task)
    {
        bool found_next = false;
        tcb *temp = current_task->next;

        while (temp != current_task)
        {
            if (temp->state == READY)
            {
                current_task = temp;
                current_task->start_time = clock();
                current_task->state = RUNNING;
                found_next = true;
                break;
            }

            temp = temp->next;
        }

        if (!found_next)
        {
            current_task = NULL;
        }
    }

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::garbage_collect()
{
    pthread_mutex_lock(&sched_lock);

    if (head == NULL)
    {
        if (log_win != NULL)
        {
            write_window(log_win, " No tasks to collect.\n");
        }
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    if (log_win != NULL)
    {
        write_window(log_win, " Running garbage collector...\n");
    }

    while (head != NULL && head->state == DEAD)
    {
        if (head->next == head)
        {
            head = NULL;
            current_task = NULL;
            total_tasks--;
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        tcb *last = head;
        while (last->next != head)
        {
            last = last->next;
        }

        tcb *dead_task = head;
        head = head->next;
        last->next = head;

        if (current_task == dead_task)
        {
            current_task = head;
        }

        total_tasks--;
    }

    if (head != NULL)
    {
        tcb *prev = head;
        tcb *temp = head->next;

        while (temp != head)
        {
            if (temp->state == DEAD)
            {
                tcb *dead_task = temp;
                prev->next = temp->next;
                temp = temp->next;

                if (current_task == dead_task)
                {
                    current_task = prev;
                }

                total_tasks--;
            }
            else
            {
                prev = temp;
                temp = temp->next;
            }
        }
    }

    pthread_mutex_unlock(&sched_lock);
}
