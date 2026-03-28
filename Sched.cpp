#include "Sched.h"
#include <unistd.h>
#include <cstdio>

using namespace std;

string state_to_string(TaskState state)
{
    switch (state)
    {
        case READY:   return "READY";
        case RUNNING: return "RUNNING";
        case BLOCKED: return "BLOCKED";
        case DEAD:    return "DEAD";
        default:      return "UNKNOWN";
    }
}

scheduler::scheduler()
{
    head = NULL;
    current_task = NULL;
    next_available_task_id = 0;
    current_quantum = 300;
    total_tasks = 0;

    pthread_mutex_init(&sched_lock, NULL);
}

// Find the next READY task after start_task in the circular list.
tcb *scheduler::find_next_ready_task(tcb *start_task)
{
    if (start_task == NULL)
    {
        return NULL;
    }

    tcb *next_task = start_task->next;
    while (next_task != start_task)
    {
        if (next_task->state == READY)
        {
            return next_task;
        }
        next_task = next_task->next;
    }

    return NULL;
}

// Move CPU ownership to next_task and wake its worker thread.
void scheduler::switch_to_task(tcb *next_task)
{
    if (next_task == NULL)
    {
        return;
    }

    current_task = next_task;
    current_task->start_time = clock();
    current_task->state = RUNNING;
    pthread_cond_signal(&current_task->run_cond);
}

// Remove DEAD nodes from the front (head) of the circular list.
void scheduler::remove_head_dead_tasks()
{
    while (head != NULL && head->state == DEAD)
    {
        if (head->next == head)
        {
            pthread_cond_destroy(&head->run_cond);
            delete head;
            head = NULL;
            current_task = NULL;
            total_tasks--;
            return;
        }

        tcb *last = head;
        while (last->next != head)
        {
            last = last->next;
        }

        tcb *delete_me = head;
        head = head->next;
        last->next = head;

        if (current_task == delete_me)
        {
            current_task = head;
        }

        pthread_cond_destroy(&delete_me->run_cond);
        delete delete_me;
        total_tasks--;
    }
}

// Remove DEAD nodes from the rest of the circular list.
void scheduler::remove_internal_dead_tasks()
{
    if (head == NULL)
    {
        return;
    }

    tcb *prev = head;
    tcb *task = head->next;

    while (task != head)
    {
        if (task->state == DEAD)
        {
            tcb *delete_me = task;
            prev->next = task->next;
            task = task->next;

            if (current_task == delete_me)
            {
                current_task = prev;
            }

            pthread_cond_destroy(&delete_me->run_cond);
            delete delete_me;
            total_tasks--;
        }
        else
        {
            prev = task;
            task = task->next;
        }
    }
}

scheduler::~scheduler()
{
    pthread_mutex_lock(&sched_lock);

    if (head != NULL)
    {
        tcb *temp = head->next;

        while (temp != head)
        {
            tcb *delete_me = temp;
            temp = temp->next;
            pthread_cond_destroy(&delete_me->run_cond);
            delete delete_me;
        }

        pthread_cond_destroy(&head->run_cond);
        delete head;
    }

    pthread_mutex_unlock(&sched_lock);
    pthread_mutex_destroy(&sched_lock);

}

void scheduler::set_quantum(long quantum)
{
    current_quantum = quantum;
}

long scheduler::get_quantum()
{
    return current_quantum;
}

tcb *scheduler::find_task(int the_taskid)
{
    if (head == NULL)
    {
        return NULL;
    }

    tcb *temp = head;
    do
    {
        if (temp->task_id == the_taskid)
        {
            return temp;
        }
        temp = temp->next;
    }
    while (temp != head);

    return NULL;
}

void scheduler::set_state(int the_taskid, TaskState the_state)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);
    if (task != NULL)
    {
        task->state = the_state;
        if (the_state == READY)
        {
            pthread_cond_signal(&task->run_cond);
        }
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
    pthread_cond_init(&new_task->run_cond, NULL);

    char buff[256];
    sprintf(buff, "Creating task # %d (%s)\n", next_available_task_id, task_name.c_str());
    write_log(buff);

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
        write_log("There are no tasks to start!\n");
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    write_log("................\n");
    write_log("................SCHEDULING STARTED\n");
    write_log("................\n\n");

    current_task = head;
    current_task->start_time = clock();
    current_task->state = RUNNING;

    if (total_tasks > 0)
    {
        set_quantum(1000 / total_tasks);
    }

    pthread_cond_signal(&current_task->run_cond);
    pthread_mutex_unlock(&sched_lock);
}

void scheduler::wait_until_running(tcb *task)
{
    pthread_mutex_lock(&sched_lock);

    while (task->state != RUNNING && !task->kill_signal && task->state != DEAD)
    {
        pthread_cond_wait(&task->run_cond, &sched_lock);
    }

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::wake_task(tcb *task)
{
    pthread_mutex_lock(&sched_lock);
    pthread_cond_signal(&task->run_cond);
    pthread_mutex_unlock(&sched_lock);
}

void scheduler::yield()
{
    pthread_mutex_lock(&sched_lock);

    if (current_task == NULL)
    {
        write_log("No current task to yield.\n");
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    char buff[256];
    sprintf(buff, "Current Task # %d is trying to yield...\n", current_task->task_id);
    write_log(buff);

    // If current task cannot run, switch away immediately.
    if (current_task->state == BLOCKED || current_task->state == DEAD)
    {
        tcb *next_task = find_next_ready_task(current_task);
        if (next_task != NULL)
        {
            switch_to_task(next_task);
        }
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    // Move current task back to READY before round-robin search.
    if (current_task->state == RUNNING)
    {
        current_task->state = READY;
    }

    // Pick the next READY task in the circular list.
    tcb *next_task = find_next_ready_task(current_task);
    if (next_task != NULL)
    {
        switch_to_task(next_task);
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    // If no other READY task exists, continue running current task.
    if (current_task->state == READY)
    {
        switch_to_task(current_task);
    }

    pthread_mutex_unlock(&sched_lock);
}

std::string scheduler::dump_to_string()
{
    pthread_mutex_lock(&sched_lock);

    std::string out;
    char buff[256];

    out += "---------------- PROCESS TABLE ---------------\n";
    sprintf(buff, "Quantum = %d\n", current_quantum);
    out += buff;
    out += "Task-Name\tTask-ID\tState\n";

    if (head == NULL)
    {
        out += "No tasks in scheduler.\n";
        out += "----------------------------------------------\n";
        pthread_mutex_unlock(&sched_lock);
        return out;
    }

    tcb *temp = head;
    do
    {
        sprintf(buff, "%s\t\t%d\t%s",
                temp->task_name.c_str(),
                temp->task_id,
                state_to_string(temp->state).c_str());
        out += buff;

        if (temp == current_task)
        {
            out += " <-- CURRENT PROCESS";
        }

        out += "\n";
        temp = temp->next;
    }
    while (temp != head);

    out += "----------------------------------------------\n";

    pthread_mutex_unlock(&sched_lock);
    return out;
}

void scheduler::dump()
{
    std::string text = dump_to_string();
    write_log(text.c_str());
}

void scheduler::kill_task(int the_taskid)
{
    pthread_mutex_lock(&sched_lock);

    tcb *task = find_task(the_taskid);

    if (task == NULL)
    {
        char buff[256];
        sprintf(buff, "kill_task FAILED: Task %d not found.\n", the_taskid);
        write_log(buff);
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    char buff[256];
    sprintf(buff, "Killing task # %d\n", the_taskid);
    write_log(buff);
    task->kill_signal = true;
    task->state = DEAD;
    pthread_cond_signal(&task->run_cond);

    // If we killed the running task, hand off CPU to the next READY task.
    if (task == current_task)
    {
        tcb *next_task = find_next_ready_task(current_task);
        if (next_task != NULL)
        {
            switch_to_task(next_task);
            pthread_mutex_unlock(&sched_lock);
            return;
        }

        current_task = NULL;
    }

    pthread_mutex_unlock(&sched_lock);
}

void scheduler::garbage_collect()
{
    pthread_mutex_lock(&sched_lock);

    if (head == NULL)
    {
        write_log("No tasks to collect.\n");
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    write_log("Running garbage collector...\n");
    remove_head_dead_tasks();

    if (head == NULL)
    {
        pthread_mutex_unlock(&sched_lock);
        return;
    }

    remove_internal_dead_tasks();

    pthread_mutex_unlock(&sched_lock);
}
