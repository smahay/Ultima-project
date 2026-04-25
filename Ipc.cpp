#include "Ipc.h"
#include "Queue.h"
#include "Sched.h"
#include <pthread.h>
#include <cstring>
#include <cstdio>
#include <ncurses.h>

Queue<ipc::Message*> *g_mailboxes = NULL;
pthread_mutex_t *g_mailbox_locks = NULL;
int g_mailbox_count = 0;

pthread_mutex_t g_setup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_dump_lock = PTHREAD_MUTEX_INITIALIZER;

// Print one IPC line to the ncurses log window, or stdout if no window exists.
void ipc_output_line(const char *line)
{
    if (line == NULL)
    {
        return;
    }

    WINDOW *log_win = get_ipc_log_window();
    if (log_win != NULL)
    {
        write_window(log_win, line);
        return;
    }

    printf("%s", line);
}

// Copy text into a fixed-size buffer.
void copy_text_safely(char *dest, const char *src, int dest_size)
{
    if (dest == NULL || dest_size <= 0)
    {
        return;
    }

    if (src == NULL)
    {
        dest[0] = '\0';
        return;
    }

    int i = 0;
    while (i < dest_size - 1 && src[i] != '\0')
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

// Return text length up to max_len.
int text_length_with_limit(const char *text, int max_len)
{
    if (text == NULL)
    {
        return 0;
    }

    int count = 0;
    while (count < max_len && text[count] != '\0')
    {
        count++;
    }

    return count;
}

// Fill message type ID and readable label.
void set_message_type(ipc::Message_Type *msg_type, int type_id)
{
    if (msg_type == NULL)
    {
        return;
    }

    msg_type->Message_Type_Id = type_id;

    if (type_id == 0)
    {
        strcpy(msg_type->Message_Type_Description, "TEXT");
    }
    else if (type_id == 1)
    {
        strcpy(msg_type->Message_Type_Description, "SERVICE");
    }
    else if (type_id == 2)
    {
        strcpy(msg_type->Message_Type_Description, "NOTIFICATION");
    }
    else
    {
        strcpy(msg_type->Message_Type_Description, "UNKNOWN");
    }
}

// Delete and free every message currently in one mailbox.
void delete_all_messages_in_mailbox(int task_id)
{
    if (g_mailboxes == NULL)
    {
        return;
    }

    if (task_id < 0 || task_id >= g_mailbox_count)
    {
        return;
    }

    while (!g_mailboxes[task_id].isEmpty())
    {
        ipc::Message *msg = g_mailboxes[task_id].De_Q();
        delete msg;
    }
}

// Makes sure the mailbox arrays exist and match the current task count.
int ensure_mailbox_storage(int requested_count)
{
    int safe_count = requested_count;
    if (safe_count <= 0)
    {
        safe_count = 1;
    }

    pthread_mutex_lock(&g_setup_lock);

    if (g_mailboxes != NULL && g_mailbox_locks != NULL && g_mailbox_count == safe_count)
    {
        pthread_mutex_unlock(&g_setup_lock);
        return 1;
    }

    if (g_mailboxes != NULL && g_mailbox_locks != NULL)
    {
        for (int i = 0; i < g_mailbox_count; i++)
        {
            pthread_mutex_lock(&g_mailbox_locks[i]);
            delete_all_messages_in_mailbox(i);
            pthread_mutex_unlock(&g_mailbox_locks[i]);
        }

        delete[] g_mailboxes;
        delete[] g_mailbox_locks;
        g_mailboxes = NULL;
        g_mailbox_locks = NULL;
        g_mailbox_count = 0;
    }

    g_mailboxes = new Queue<ipc::Message*>[safe_count];
    g_mailbox_locks = new pthread_mutex_t[safe_count];
    g_mailbox_count = safe_count;

    pthread_mutex_t starter_lock = PTHREAD_MUTEX_INITIALIZER;
    for (int i = 0; i < g_mailbox_count; i++)
    {
        g_mailbox_locks[i] = starter_lock;
    }

    pthread_mutex_unlock(&g_setup_lock);
    return 1;
}

ipc::ipc(int the_max_tasks)
{
    max_tasks = the_max_tasks;
    if (max_tasks <= 0)
    {
        max_tasks = 1;
    }

    ensure_mailbox_storage(max_tasks);
}

// Send one full message struct into the destination task mailbox.
int ipc::Message_Send(Message *message)
{
    if (message == NULL)
    {
        return -1;
    }

    if (message->Source_Task_Id < 0 || message->Source_Task_Id >= max_tasks)
    {
        return -1;
    }

    if (message->Destination_Task_Id < 0 || message->Destination_Task_Id >= max_tasks)
    {
        return -1;
    }

    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return -1;
    }

    Message *new_message = new Message;
    new_message->Source_Task_Id = message->Source_Task_Id;
    new_message->Destination_Task_Id = message->Destination_Task_Id;
    new_message->Message_Arrival_Time = time(NULL);
    set_message_type(&new_message->Msg_Type, message->Msg_Type.Message_Type_Id);
    copy_text_safely(new_message->Msg_Text, message->Msg_Text, 33);
    new_message->Msg_Size = text_length_with_limit(new_message->Msg_Text, 32);

    int destination_id = new_message->Destination_Task_Id;

    // Lock mailbox -> enqueue -> unlock mailbox.
    pthread_mutex_lock(&g_mailbox_locks[destination_id]);
    g_mailboxes[destination_id].En_Q(new_message);
    pthread_mutex_unlock(&g_mailbox_locks[destination_id]);

    return 1;
}

// Send message using simple input arguments.
int ipc::Message_Send(int S_Id, int D_Id, char *Mess, int Mess_Type)
{
    if (Mess == NULL)
    {
        return -1;
    }

    Message temp_message;
    temp_message.Source_Task_Id = S_Id;
    temp_message.Destination_Task_Id = D_Id;
    temp_message.Message_Arrival_Time = time(NULL);
    set_message_type(&temp_message.Msg_Type, Mess_Type);
    copy_text_safely(temp_message.Msg_Text, Mess, 33);
    temp_message.Msg_Size = text_length_with_limit(temp_message.Msg_Text, 32);

    return Message_Send(&temp_message);
}

// Receive one message from a task mailbox into a full message struct.
int ipc::Message_Receive(int Task_Id, Message *message)
{
    if (message == NULL)
    {
        return -1;
    }

    if (Task_Id < 0 || Task_Id >= max_tasks)
    {
        return -1;
    }

    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return -1;
    }

    // Lock mailbox -> dequeue -> unlock mailbox.
    pthread_mutex_lock(&g_mailbox_locks[Task_Id]);

    if (g_mailboxes[Task_Id].isEmpty())
    {
        pthread_mutex_unlock(&g_mailbox_locks[Task_Id]);
        return 0;
    }

    Message *stored_message = g_mailboxes[Task_Id].De_Q();
    *message = *stored_message;
    delete stored_message;

    pthread_mutex_unlock(&g_mailbox_locks[Task_Id]);
    return 1;
}

// Receive only message text and message type ID.
int ipc::Message_Receive(int Task_Id, char *Mess, int *Mess_Type)
{
    if (Mess == NULL || Mess_Type == NULL)
    {
        return -1;
    }

    Message temp_message;
    int result = Message_Receive(Task_Id, &temp_message);

    if (result == 1)
    {
        strcpy(Mess, temp_message.Msg_Text);
        *Mess_Type = temp_message.Msg_Type.Message_Type_Id;
    }

    return result;
}

// Count messages in one mailbox without removing them.
int ipc::Message_Count(int Task_Id)
{
    if (Task_Id < 0 || Task_Id >= max_tasks)
    {
        return -1;
    }

    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return -1;
    }

    int count = 0;
    Queue<Message*> temp_queue;

    pthread_mutex_lock(&g_mailbox_locks[Task_Id]);

    while (!g_mailboxes[Task_Id].isEmpty())
    {
        Message *msg = g_mailboxes[Task_Id].De_Q();
        temp_queue.En_Q(msg);
        count++;
    }

    while (!temp_queue.isEmpty())
    {
        g_mailboxes[Task_Id].En_Q(temp_queue.De_Q());
    }

    pthread_mutex_unlock(&g_mailbox_locks[Task_Id]);
    return count;
}

// Count total messages across all mailboxes without removing them.
int ipc::Message_Count()
{
    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return -1;
    }

    int total = 0;

    for (int i = 0; i < max_tasks; i++)
    {
        Queue<Message*> temp_queue;

        pthread_mutex_lock(&g_mailbox_locks[i]);

        while (!g_mailboxes[i].isEmpty())
        {
            Message *msg = g_mailboxes[i].De_Q();
            temp_queue.En_Q(msg);
            total++;
        }

        while (!temp_queue.isEmpty())
        {
            g_mailboxes[i].En_Q(temp_queue.De_Q());
        }

        pthread_mutex_unlock(&g_mailbox_locks[i]);
    }

    return total;
}

// Print one mailbox without removing any messages.
void ipc::Message_Print(int Task_Id)
{
    if (Task_Id < 0 || Task_Id >= max_tasks)
    {
        return;
    }

    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return;
    }

    Queue<Message*> temp_queue;
    int count = 0;
    char line[256];

    sprintf(line, " -------- MAILBOX FOR TASK %d --------\n", Task_Id);
    ipc_output_line(line);

    pthread_mutex_lock(&g_mailbox_locks[Task_Id]);

    while (!g_mailboxes[Task_Id].isEmpty())
    {
        Message *msg = g_mailboxes[Task_Id].De_Q();
        temp_queue.En_Q(msg);
        count++;

        char time_text[64];
        struct tm *time_info = localtime(&msg->Message_Arrival_Time);

        if (time_info != NULL)
        {
            strftime(time_text, sizeof(time_text), "%H:%M:%S", time_info);
        }
        else
        {
            strcpy(time_text, "unknown");
        }

        sprintf(line,
                " From:%d To:%d Type:%d (%s) Size:%d Time:%s Text:%s\n",
                msg->Source_Task_Id,
                msg->Destination_Task_Id,
                msg->Msg_Type.Message_Type_Id,
                msg->Msg_Type.Message_Type_Description,
                msg->Msg_Size,
                time_text,
                msg->Msg_Text);
        ipc_output_line(line);
    }

    while (!temp_queue.isEmpty())
    {
        g_mailboxes[Task_Id].En_Q(temp_queue.De_Q());
    }

    pthread_mutex_unlock(&g_mailbox_locks[Task_Id]);

    sprintf(line, " Message Count: %d\n", count);
    ipc_output_line(line);
    if (count == 0)
    {
        ipc_output_line(" Mailbox is empty.\n");
    }
    ipc_output_line(" ------------------------------------\n");
}

// Delete all messages in one mailbox and return how many were deleted.
int ipc::Message_DeleteAll(int Task_Id)
{
    if (Task_Id < 0 || Task_Id >= max_tasks)
    {
        return -1;
    }

    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return -1;
    }

    int deleted_count = 0;

    pthread_mutex_lock(&g_mailbox_locks[Task_Id]);

    while (!g_mailboxes[Task_Id].isEmpty())
    {
        Message *msg = g_mailboxes[Task_Id].De_Q();
        delete msg;
        deleted_count++;
    }

    pthread_mutex_unlock(&g_mailbox_locks[Task_Id]);
    return deleted_count;
}

// Print all task mailboxes without removing messages.
void ipc::ipc_Message_Dump()
{
    if (ensure_mailbox_storage(max_tasks) != 1)
    {
        return;
    }

    pthread_mutex_lock(&g_dump_lock);

    ipc_output_line(" ========== IPC MESSAGE DUMP ==========\n");
    for (int i = 0; i < max_tasks; i++)
    {
        Message_Print(i);
    }
    ipc_output_line(" ======================================\n");

    pthread_mutex_unlock(&g_dump_lock);
}