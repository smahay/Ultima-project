#ifndef IPC_H
#define IPC_H

#include <iostream>
#include <string>
#include <ctime>
#include "Queue.h"
#include "Sched.h"
#include "Sema.h"

using namespace std;

// Message types.
const int MSG_TEXT = 0;
const int MSG_SERVICE = 1;
const int MSG_NOTIFICATION = 2;

// Message record.
struct Message
{
    int source_task_id;
    int destination_task_id;
    time_t arrival_time;
    int msg_type;
    int msg_size;
    string msg_text;
};

// IPC system.
class ipc
{
private:
    // One mailbox per task.
    struct mailbox
    {
        Queue<Message*> msg_queue;
        semaphore *mailbox_sema;
    };

    mailbox *mailboxes;
    int max_tasks;
    scheduler *sched_ptr;
    WINDOW *log_win;

public:
    ipc(int max_tasks, scheduler *theScheduler);
    ~ipc();

    void set_log_window(WINDOW *win);

    int Message_Send(Message *msg);
    int Message_Send(int s_id, int d_id, const char *mess, int mess_type);

    int Message_Receive(int task_id, Message *msg);
    int Message_Receive(int task_id, char *mess, int *mess_type);

    int Message_Count(int task_id);
    int Message_Count();

    void Message_Print(int task_id);
    int Message_DeleteAll(int task_id);
    void ipc_Message_Dump();
};

#endif