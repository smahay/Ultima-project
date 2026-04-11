#include "Ipc.h"
#include <cstdio>
#include <cstring>

using namespace std;

// Constructor.
ipc::ipc(int the_max_tasks, scheduler *theScheduler)
{
    max_tasks = the_max_tasks;
    sched_ptr = theScheduler;
    log_win = NULL;

    if (max_tasks <= 0)
    {
        max_tasks = 1;
    }

    mailboxes = new mailbox[max_tasks];

    for (int i = 0; i < max_tasks; i++)
    {
        char buff[64];
        snprintf(buff, sizeof(buff), "Mailbox_%d", i);

        mailboxes[i].mailbox_sema = new semaphore(1, buff, sched_ptr);

        if (log_win != NULL)
        {
            mailboxes[i].mailbox_sema->set_log_window(log_win);
        }
    }
}

// Destructor.
ipc::~ipc()
{
    for (int i = 0; i < max_tasks; i++)
    {
        while (!mailboxes[i].msg_queue.isEmpty())
        {
            Message *temp = mailboxes[i].msg_queue.De_Q();
            delete temp;
        }

        delete mailboxes[i].mailbox_sema;
    }

    delete[] mailboxes;
}

// Set log window.
void ipc::set_log_window(WINDOW *win)
{
    log_win = win;

    for (int i = 0; i < max_tasks; i++)
    {
        mailboxes[i].mailbox_sema->set_log_window(win);
    }
}

// Send using full message record.
int ipc::Message_Send(Message *msg)
{
    if (msg == NULL)
    {
        return -1;
    }

    int d_id = msg->destination_task_id;

    if (d_id < 0 || d_id >= max_tasks)
    {
        return -1;
    }

    if (sched_ptr->find_task(msg->source_task_id) == NULL ||
        sched_ptr->find_task(msg->destination_task_id) == NULL)
    {
        return -1;
    }

    Message *new_msg = new Message;
    new_msg->source_task_id = msg->source_task_id;
    new_msg->destination_task_id = msg->destination_task_id;
    new_msg->arrival_time = time(NULL);
    new_msg->msg_type = msg->msg_type;
    new_msg->msg_text = msg->msg_text;
    new_msg->msg_size = new_msg->msg_text.length();

    mailboxes[d_id].mailbox_sema->down(msg->source_task_id);

    if (sched_ptr->get_state(msg->source_task_id) == BLOCKED)
    {
        delete new_msg;
        return -1;
    }

    mailboxes[d_id].msg_queue.En_Q(new_msg);

    if (log_win != NULL)
    {
        char buff[256];
        snprintf(buff, sizeof(buff),
                 " Message sent from Task %d to Task %d | Type: %d | Size: %d\n",
                 new_msg->source_task_id,
                 new_msg->destination_task_id,
                 new_msg->msg_type,
                 new_msg->msg_size);
        write_window(log_win, buff);
    }

    mailboxes[d_id].mailbox_sema->up();
    return 1;
}

// Overloaded send.
int ipc::Message_Send(int s_id, int d_id, const char *mess, int mess_type)
{
    if (mess == NULL)
    {
        return -1;
    }

    Message temp_msg;
    temp_msg.source_task_id = s_id;
    temp_msg.destination_task_id = d_id;
    temp_msg.arrival_time = time(NULL);
    temp_msg.msg_type = mess_type;
    temp_msg.msg_text = mess;
    temp_msg.msg_size = temp_msg.msg_text.length();

    return Message_Send(&temp_msg);
}

// Receive into full message record.
int ipc::Message_Receive(int task_id, Message *msg)
{
    if (msg == NULL)
    {
        return -1;
    }

    if (task_id < 0 || task_id >= max_tasks)
    {
        return -1;
    }

    if (sched_ptr->find_task(task_id) == NULL)
    {
        return -1;
    }

    if (mailboxes[task_id].msg_queue.isEmpty())
    {
        return 0;
    }

    mailboxes[task_id].mailbox_sema->down(task_id);

    if (sched_ptr->get_state(task_id) == BLOCKED)
    {
        return -1;
    }

    if (mailboxes[task_id].msg_queue.isEmpty())
    {
        mailboxes[task_id].mailbox_sema->up();
        return 0;
    }

    Message *temp = mailboxes[task_id].msg_queue.De_Q();

    msg->source_task_id = temp->source_task_id;
    msg->destination_task_id = temp->destination_task_id;
    msg->arrival_time = temp->arrival_time;
    msg->msg_type = temp->msg_type;
    msg->msg_size = temp->msg_size;
    msg->msg_text = temp->msg_text;

    if (log_win != NULL)
    {
        char buff[256];
        snprintf(buff, sizeof(buff),
                 " Message received by Task %d from Task %d | Type: %d | Size: %d\n",
                 msg->destination_task_id,
                 msg->source_task_id,
                 msg->msg_type,
                 msg->msg_size);
        write_window(log_win, buff);
    }

    delete temp;
    mailboxes[task_id].mailbox_sema->up();

    return 1;
}

// Overloaded receive.
int ipc::Message_Receive(int task_id, char *mess, int *mess_type)
{
    if (mess == NULL || mess_type == NULL)
    {
        return -1;
    }

    Message temp_msg;
    int result = Message_Receive(task_id, &temp_msg);

    if (result == 1)
    {
        strcpy(mess, temp_msg.msg_text.c_str());
        *mess_type = temp_msg.msg_type;
    }

    return result;
}

// Count messages in one mailbox.
int ipc::Message_Count(int task_id)
{
    if (task_id < 0 || task_id >= max_tasks)
    {
        return -1;
    }

    if (sched_ptr->find_task(task_id) == NULL)
    {
        return -1;
    }

    int count = 0;
    Queue<Message*> temp_queue;

    while (!mailboxes[task_id].msg_queue.isEmpty())
    {
        Message *msg = mailboxes[task_id].msg_queue.De_Q();
        temp_queue.En_Q(msg);
        count++;
    }

    while (!temp_queue.isEmpty())
    {
        mailboxes[task_id].msg_queue.En_Q(temp_queue.De_Q());
    }

    return count;
}

// Count all messages.
int ipc::Message_Count()
{
    int total = 0;

    for (int i = 0; i < max_tasks; i++)
    {
        if (sched_ptr->find_task(i) != NULL)
        {
            int count = Message_Count(i);
            if (count > 0)
            {
                total += count;
            }
        }
    }

    return total;
}

// Print one mailbox without deleting.
void ipc::Message_Print(int task_id)
{
    if (task_id < 0 || task_id >= max_tasks)
    {
        return;
    }

    if (sched_ptr->find_task(task_id) == NULL)
    {
        return;
    }

    if (log_win == NULL)
    {
        return;
    }

    string out;
    char buff[512];

    snprintf(buff, sizeof(buff), " -------- MAILBOX FOR TASK %d --------\n", task_id);
    out += buff;

    int count = Message_Count(task_id);
    snprintf(buff, sizeof(buff), " Message Count: %d\n", count);
    out += buff;

    if (count == 0)
    {
        out += " Mailbox is empty.\n";
        out += " ------------------------------------\n";
        write_window(log_win, out.c_str());
        return;
    }

    Queue<Message*> temp_queue;

    while (!mailboxes[task_id].msg_queue.isEmpty())
    {
        Message *msg = mailboxes[task_id].msg_queue.De_Q();

        char time_buff[64];
        struct tm *time_info = localtime(&msg->arrival_time);
        strftime(time_buff, sizeof(time_buff), "%H:%M:%S", time_info);

        snprintf(buff, sizeof(buff),
                 " From:%d To:%d Type:%d Size:%d Time:%s Text:%s\n",
                 msg->source_task_id,
                 msg->destination_task_id,
                 msg->msg_type,
                 msg->msg_size,
                 time_buff,
                 msg->msg_text.c_str());
        out += buff;

        temp_queue.En_Q(msg);
    }

    while (!temp_queue.isEmpty())
    {
        mailboxes[task_id].msg_queue.En_Q(temp_queue.De_Q());
    }

    out += " ------------------------------------\n";
    write_window(log_win, out.c_str());
}

// Delete all messages for one task.
int ipc::Message_DeleteAll(int task_id)
{
    if (task_id < 0 || task_id >= max_tasks)
    {
        return -1;
    }

    if (sched_ptr->find_task(task_id) == NULL)
    {
        return -1;
    }

    int deleted_count = 0;

    while (!mailboxes[task_id].msg_queue.isEmpty())
    {
        Message *msg = mailboxes[task_id].msg_queue.De_Q();
        delete msg;
        deleted_count++;
    }

    if (log_win != NULL)
    {
        char buff[256];
        snprintf(buff, sizeof(buff),
                 " Deleted %d messages from Task %d mailbox\n",
                 deleted_count,
                 task_id);
        write_window(log_win, buff);
    }

    return deleted_count;
}

// Dump all mailboxes.
void ipc::ipc_Message_Dump()
{
    if (log_win == NULL)
    {
        return;
    }

    write_window(log_win, " ========== IPC MESSAGE DUMP ==========\n");

    for (int i = 0; i < max_tasks; i++)
    {
        if (sched_ptr->find_task(i) != NULL)
        {
            Message_Print(i);
        }
    }

    write_window(log_win, " ======================================\n");
}
