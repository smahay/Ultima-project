// This is the file Sched.cpp 

#include <iostream>
#include <string>
#include <ctime>
#include <cstdio>
#include <unistd.h>
#include "Queue.h"

using namespace std;

const string READY = "READY";
const string RUNNING = "RUNNING";
const string BLOCKED = "BLOCKED";
const string DEAD = "DEAD";

struct tcb 
{
    int task_id;
    string state;
    clock_t start_time;
    tcb *next;
};

class scheduler 
{
    tcb *head;
    tcb *current_task; 
    int current_quantum;
    int next_available_task_id;
    int total_tasks;

    public: 

    scheduler()
    {
        head = NULL;
        current_task = NULL;
        next_available_task_id = 0;
        current_quantum = 300; 
        total_tasks = 0;
    }

    ~scheduler()
    {
        if (head != NULL)
        {
            tcb *temp = head->next;

            while (temp != head)
            {
                tcb *delete_me = temp;
                temp = temp->next;
                delete delete_me;
            }

            delete head;
        }

        cout << "Exiting Scheduler...." << endl;
    }

    void set_quantum(long quantum)
    {
        current_quantum = quantum;
    }

    long get_quantum()
    {
        return(current_quantum);
    }

    tcb* find_task(int the_taskid)
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

    void set_state(int the_taskid, string the_state)
    {
        tcb *task = find_task(the_taskid);

        if (task != NULL)
        {
            task->state = the_state;
        }
    }

    string get_state(int the_taskid)
    {
        tcb *task = find_task(the_taskid);

        if (task != NULL)
        {
            return task->state;
        }
        return "NOT FOUND";
    }

    int get_task_id()
    {
        if (current_task == NULL)
        {
            return -1;
        }
        return current_task->task_id;
    }

    int create_task()
    {
        tcb *new_task = new tcb;

        new_task->task_id = next_available_task_id;
        new_task->state = READY;
        new_task->start_time = 0;

        cout << "Creating task # " << next_available_task_id << endl;

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

        return new_task->task_id;
    }

    void start() 
    {
        if (head == NULL)
        {
            cout << "There are no tasks to start!" << endl;
            return;
        }

        cout << "................" << endl;
        cout << "................SCHEDULING STARTED" << endl;
        cout << "................\n" << endl; 

        current_task = head;
        current_task->start_time = clock();
        current_task->state = RUNNING;

        if (total_tasks > 0)
        {
            set_quantum(1000 / total_tasks);
        }

        sleep(1);
    }

    void yield()
    {
        if (current_task == NULL)
        {
            cout << "No current task to yield." << endl;
            return;
        }

        cout << "Current Task # " << current_task->task_id << " is trying to yield..." << endl;

        // If blocked or dead, switch immediately
        if (current_task->state == BLOCKED || current_task->state == DEAD)
        {
            cout << "Task is not runnable. Switching immediately..." << endl;

            tcb *temp = current_task->next;

            while (temp != current_task)
            {
                if (temp->state == READY)
                {
                    current_task = temp;
                    current_task->start_time = clock();
                    current_task->state = RUNNING;
                    cout << "Started Running task # " << current_task->task_id << endl;
                    return;
                }

                temp = temp->next;
            }

            cout << "POSSIBLE DEAD LOCK" << endl;
            return;
        }

        clock_t elapsed_time = clock() - current_task->start_time;
        cout << "Task: " << current_task->task_id << ", Elapsed time: " << elapsed_time << endl;
        cout << "Current Quantum: " << current_quantum << endl;

        if (elapsed_time >= current_quantum)
        {
            cout << "Yielding....(Switching from task # " << current_task->task_id << " to next ready task)" << endl;

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
                    cout << "Started Running task # " << current_task->task_id << endl;
                    return;
                }

                temp = temp->next;
            }

            if (current_task->state == READY)
            {
                current_task->start_time = clock();
                current_task->state = RUNNING;
                cout << "Started Running task # " << current_task->task_id << endl;
            }
            else
            {
                cout << "POSSIBLE DEAD LOCK" << endl;
            }
        }
        else
        {
            cout << "NO Yield! (task: " << current_task->task_id << " Still has some quantum left)" << endl;
        }
    }
    void dump()
    {
        cout << "---------------- PROCESS TABLE ---------------" << endl;
        cout << "Quantum = " << current_quantum << endl;
        cout << "Task-ID\t Elapsed Time\t State" << endl;

        if (head == NULL)
        {
            cout << "No tasks in scheduler." << endl;
            cout << "----------------------------------------------\n" << endl;
            return;
        }

        tcb *temp = head;

        do
        {
            clock_t elapsed_time = 0;

            if (temp->start_time != 0)
            {
                elapsed_time = clock() - temp->start_time;
            }

            printf(" %6d\t%8d\t%s", temp->task_id, (int)elapsed_time, temp->state.c_str());

            if (temp == current_task)
            {
                cout << " <-- CURRENT PROCESS";
            }

            cout << endl;
            temp = temp->next;
        }
        while (temp != head);

        cout << "----------------------------------------------\n" << endl;
    }
};

class semaphore 
{
    string resource_name; 
    int sema_value;
    int lucky_task;

    Queue<int> sema_queue;
    scheduler *sched_ptr;

    public: 

    semaphore(int starting_value, string name, scheduler *theScheduler)
    {
        sema_value = starting_value;
        resource_name = name; 
        lucky_task = -1;
        
        sched_ptr = theScheduler; 
    }

    ~semaphore() 
    {

    }

    void down(int taskID)
    {
        if (taskID == lucky_task) 
        {
            cout << "Task # " << lucky_task << " already has the resource! Ignore request." << endl;
            dump(1);
        }
        else 
        {
            if(sema_value >= 1) 
            {
                sema_value--;
                lucky_task = taskID;
                dump(1);
            }
            else 
            {
                sema_queue.En_Q(taskID);
                sched_ptr->set_state(taskID, BLOCKED);
                dump(1);

                sched_ptr->yield();
                dump(1);
            }
        }
    }

    void up()
    {
        int task_id; 
        cout << "TaskID : " << sched_ptr->get_task_id() << ", LuckID : " << lucky_task << endl;

        if (sched_ptr->get_task_id() == lucky_task)
        {
            if (sema_queue.isEmpty())
            {
                sema_value++;
                lucky_task = -1; 
                dump(1);
            }
            else 
            {
                task_id = sema_queue.De_Q();
                sched_ptr->set_state(task_id, READY);
                cout << "UnBlock  : " << task_id << " and release from the queue" << endl;
                lucky_task = task_id; 
                cout << "Luck Task = " << lucky_task << endl;
                dump(1); 
                sched_ptr->yield();
                dump(1);
            }
        }
        else 
        {
            cout << "Invalid Semaphore UP(). TaskID : " << sched_ptr->get_task_id() << " Does not own the resource" << endl;
            dump(1);
        }
    }

    void dump(int level)
    {
        cout << "---------------- SEMAPHORE DUMP ----------------" << endl;

        switch(level)
        {
            case 0: 
                cout << "Sema_Value: " << sema_value << endl;
                cout << "Sema_Name: " << resource_name << endl;
                cout << "Obtained by Task-ID: " << lucky_task << endl;
                break; 

            case 1: 
                cout << "Sema_Value: " << sema_value << endl;
                cout << "Sema_Name: " << resource_name << endl;
                cout << "Obtained by Task-ID: " << lucky_task << endl;
                cout << "Sema-Queue: " << endl;
                sema_queue.Print();
                break;

            default: 
                cout << "ERROR in SEMAPHORE DUMP level";
        }

        cout << "------------------------------------------------" << endl;
    }
};

void waste_time(int x) 
{
    cout << "Waste time................Simulating CPU work!..................." << endl;
    unsigned long long Int64 = 0; 

    for (unsigned short i = 0; i < 10000 * x; ++i)
    {
        for(unsigned short j = i; j > 0; --j)
        {
            Int64 += j + i; 
        }
    }
}

int main()
{
    scheduler swapper;
    int t_id; 

    for (int i = 0; i < 4; i++)
    {
        t_id = swapper.create_task();
    }

    swapper.dump();
    swapper.start();
    swapper.dump();

    for (int i = 0; i < 3; i++)
    {
        waste_time(3);
        swapper.yield();
        swapper.dump();
    }

    semaphore resource1_sema(1, "resource1", &swapper);
    resource1_sema.dump(0);

    t_id = swapper.get_task_id();
    cout << "Task " << t_id << " is trying to obtain the semaphore (Resource1)" << endl;
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    cout << "Task " << t_id << " is trying to obtain the semaphore (Resource1)" << endl;
    resource1_sema.down(t_id);
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    t_id = swapper.get_task_id();
    cout << "Task " << t_id << " is trying to obtain the semaphore (Resource1)" << endl;
    resource1_sema.down(t_id); 
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    while (swapper.get_task_id() != 3)
    {
        waste_time(3);
        swapper.yield();
        swapper.dump();
    }

    t_id = swapper.get_task_id();
    cout << "Task " << t_id << " is trying to release the semaphore (Resource1)" << endl;
    swapper.dump();
    resource1_sema.up();
    swapper.dump();
    waste_time(3);
    swapper.yield();
    swapper.dump();

    return 0;
}