#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <ncurses.h>
#include <cstdio>
#include "Sched.h"
#include "Sema.h"
#include "Ipc.h"

using namespace std;

// Shared lock for ncurses screen updates.
pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;

WINDOW *ipc_log_window = NULL;

// Return the IPC log window pointer.
WINDOW *get_ipc_log_window()
{
    return ipc_log_window;
}

WINDOW *create_window(int height, int width, int starty, int startx);
void write_window(WINDOW * Win, const char* text);
void write_window(WINDOW * Win, int x, int y, const char* text);
void *perform_simple_output(void *arguments);

struct thread_data
{
    WINDOW *log_win;

    scheduler *sched;
    ipc *messenger;
    tcb *task;
};

// Create and returns one ncurses window.
WINDOW *create_window(int height, int width, int starty, int startx)
{
    pthread_mutex_lock(&myMutex);

    WINDOW *Win = newwin(height, width, starty, startx);

    scrollok(Win, TRUE);
    scroll(Win);
    box(Win, 0, 0);
    wrefresh(Win);

    pthread_mutex_unlock(&myMutex);
    return Win;
}

// Appends text to a window.
void write_window(WINDOW * Win, const char* text)
{
    pthread_mutex_lock(&myMutex);

    if (Win != NULL && text != NULL)
    {
        wprintw(Win, "%s", text);
        box(Win, 0, 0);
        wrefresh(Win);
    }

    pthread_mutex_unlock(&myMutex);
}

// Print text at a specific x/y in a window.
void write_window(WINDOW * Win, int x, int y, const char* text)
{
    pthread_mutex_lock(&myMutex);

    if (Win != NULL && text != NULL)
    {
        mvwprintw(Win, y, x, "%s", text);
        box(Win, 0, 0);
        wrefresh(Win);
    }

    pthread_mutex_unlock(&myMutex);
}

// send/receive demo messages and update task state.
void *perform_simple_output(void *arguments)
{
    thread_data *td = (thread_data *) arguments;

    scheduler *sched = td->sched;
    ipc *messenger = td->messenger;
    tcb *task = td->task;
    WINDOW *log_win = td->log_win;

    char buff[256];

    while (!task->kill_signal && task->work_counter < 6)
    {
        sched->wait_until_running(task);

        if (task->kill_signal || task->state == DEAD)
        {
            break;
        }

        sprintf(buff, " Task %d running phase 2 step #%d\n",
                task->task_id, task->work_counter);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        if (task->task_id == 0)
        {
            // Task 0 receives messages.
            ipc::Message msg;

            int result = messenger->Message_Receive(task->task_id, &msg);

            if (result == 1)
            {
                sprintf(buff,
                        " Received from Task %d | Type: %d (%s) | Text: %s\n",
                        msg.Source_Task_Id,
                        msg.Msg_Type.Message_Type_Id,
                        msg.Msg_Type.Message_Type_Description,
                        msg.Msg_Text);
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
            else if (result == 0)
            {
                write_window(task->task_win, " No message available yet.\n");
            }
            else
            {
                write_window(task->task_win, " Receive returned error (-1).\n");
            }
        }
        else
        {
            // Other tasks send messages to task 0.
            char msg_text[64];

            int msg_type_id = 0;
            if (task->task_id == 1)
            {
                msg_type_id = 1;
            }
            else if (task->task_id == 2)
            {
                msg_type_id = 2;
            }
            else
            {
                msg_type_id = 0;
            }

            sprintf(msg_text, "T%d step%d type%d",
                    task->task_id, task->work_counter, msg_type_id);

            int send_result = messenger->Message_Send(task->task_id, 0, msg_text, msg_type_id);

            if (send_result == 1)
            {
                sprintf(buff, " Sent to Task 0 | Type: %d | Text: %s\n",
                        msg_type_id, msg_text);
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
            else
            {
                sprintf(buff, " Failed to send to Task 0 (result=%d)\n",
                        send_result);
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
        }

        messenger->ipc_Message_Dump();
        sched->dump();

        task->work_counter++;

        if (task->task_id != 0 && task->work_counter >= 3)
        {
            sprintf(buff, " Task %d finished work\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);

            sched->kill_task(task->task_id);
            sched->dump();
            break;
        }

        if (task->task_id == 0 && task->work_counter >= 6)
        {
            sprintf(buff, " Task %d finished work\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);

            sched->kill_task(task->task_id);
            sched->dump();
            break;
        }

        sched->yield();
        sched->dump();

        sleep(1);
    }

    write_window(task->task_win, " TERMINATED\n");
    write_window(log_win, " A task terminated.\n");

    return NULL;
}

int main()
{
    char status_buff[256];

    const int task_count = 4;

    pthread_t threads[task_count];
    thread_data thread_args[task_count];

    initscr();
    cbreak();
    noecho();

    WINDOW *heading_win  = create_window(6, 155, 1, 2);
    WINDOW *resource_win = create_window(10, 60, 8, 2);
    WINDOW *task1_win    = create_window(10, 90, 8, 65);
    WINDOW *task2_win    = create_window(10, 50, 19, 2);
    WINDOW *task3_win    = create_window(10, 50, 19, 55);
    WINDOW *task4_win    = create_window(10, 50, 19, 107);
    WINDOW *log_win      = create_window(12, 155, 30, 2);

    ipc_log_window = log_win;

    write_window(heading_win, 2, 1, "ULTIMA 2.0 - Phase 2 Message Passing (IPC)");
    write_window(heading_win, 2, 2, "by Shivansh Mahay and Moises Navarro");
    write_window(resource_win, 2, 1, "Shared Resource Window");
    write_window(log_win, 2, 1, "Log Window");

    WINDOW *task_windows[task_count];
    task_windows[0] = task1_win;
    task_windows[1] = task2_win;
    task_windows[2] = task3_win;
    task_windows[3] = task4_win;

    scheduler swapper(task_count);

    semaphore resource1_sema(1, "resource1", &swapper);

    ipc messenger(task_count);

    swapper.set_log_window(log_win);
    resource1_sema.set_log_window(log_win);

    tcb *tasks[task_count];
    // Create tasks and thread argument data.
    for (int i = 0; i < task_count; ++i)
    {
        char task_name[16];
        sprintf(task_name, "Task%d", i + 1);
        tasks[i] = swapper.create_task(task_name, task_windows[i]);

        thread_args[i].log_win = log_win;
        thread_args[i].sched = &swapper;
        thread_args[i].messenger = &messenger;
        thread_args[i].task = tasks[i];
    }

    for (int i = 0; i < task_count; ++i)
    {
        pthread_create(&threads[i], NULL, perform_simple_output, &thread_args[i]);
        tasks[i]->thread = threads[i];
    }

    swapper.dump();
    resource1_sema.dump(1);
    messenger.ipc_Message_Dump();

    swapper.start();

    // Waiting for all threads.
    for (int i = 0; i < task_count; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    write_window(log_win, "All worker threads joined.\n");
    swapper.dump();
    resource1_sema.dump(1);
    messenger.ipc_Message_Dump();

    // IPC count/delete checking.
    int total_before_delete = messenger.Message_Count();
    sprintf(status_buff,
            " IPC total messages before delete: %d\n",
            total_before_delete);
    write_window(log_win, status_buff);

    int task0_before_delete = messenger.Message_Count(0);
    sprintf(status_buff,
            " IPC Task 0 message count before delete: %d\n",
            task0_before_delete);
    write_window(log_win, status_buff);

    int deleted_from_task0 = messenger.Message_DeleteAll(0);
    sprintf(status_buff,
            " IPC deleted from Task 0 mailbox: %d\n",
            deleted_from_task0);
    write_window(log_win, status_buff);

    int task0_after_delete = messenger.Message_Count(0);
    sprintf(status_buff,
            " IPC Task 0 message count after delete: %d\n",
            task0_after_delete);
    write_window(log_win, status_buff);

    messenger.ipc_Message_Dump();

    swapper.garbage_collect();
    write_window(log_win, "Running garbage collector...\n");
    swapper.dump();

    write_window(resource_win, "All tasks completed. Press any key to exit.\n");
    wgetch(resource_win);

    endwin();
    return 0;
}
