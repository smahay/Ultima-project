#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <ncurses.h>
#include <cstdio>
#include "Sched.h"
#include "Sema.h"
#include "Ipc.h"

using namespace std;

// Screen lock.
pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;

WINDOW *create_window(int height, int width, int starty, int startx);
void write_window(WINDOW * Win, const char* text);
void write_window(WINDOW * Win, int x, int y, const char* text);
void *perform_simple_output(void *arguments);

struct thread_data
{
    WINDOW *log_win;

    // Runtime pointers.
    scheduler *sched;
    ipc *messenger;
    tcb *task;
};

// Create boxed window.
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

// Append text.
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

// Print at x/y.
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

// Worker loop.

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

        snprintf(buff, sizeof(buff), " Task %d running phase 2 step #%d\n",
                 task->task_id, task->work_counter);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        // Task 0 acts like the receiver.
        if (task->task_id == 0)
        {
            Message msg;
            int result = messenger->Message_Receive(task->task_id, &msg);

            if (result == 1)
            {
                snprintf(buff, sizeof(buff),
                         " Received from Task %d | Type: %d | Text: %s\n",
                         msg.source_task_id,
                         msg.msg_type,
                         msg.msg_text.c_str());
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
            else
            {
                write_window(task->task_win, " No message available yet.\n");
            }
        }
        else
        {
            // Other tasks send to Task 0.
            snprintf(buff, sizeof(buff), " Hello from Task %d", task->task_id);
            int send_result = messenger->Message_Send(task->task_id, 0, buff, MSG_TEXT);

            if (send_result == 1)
            {
                snprintf(buff, sizeof(buff), " Sent message to Task 0\n");
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
            else
            {
                snprintf(buff, sizeof(buff), " Failed to send message to Task 0\n");
                write_window(task->task_win, buff);
                write_window(log_win, buff);
            }
        }

        //messenger->ipc_Message_Dump();
        sched->dump();

        task->work_counter++;

        if (task->task_id != 0 && task->work_counter >= 3)
        {
            snprintf(buff, sizeof(buff), " Task %d finished work\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);

            sched->kill_task(task->task_id);
            sched->dump();
            break;
        }

        if (task->task_id == 0 && task->work_counter >= 6)
        {
            snprintf(buff, sizeof(buff), " Task %d finished work\n", task->task_id);
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
    const int task_count = 4;

    // Thread handles.
    pthread_t threads[task_count];
    thread_data thread_args[task_count];

    // Ncurses initialization.
    initscr();
    cbreak();
    noecho();

    WINDOW *heading_win  = create_window(6, 90, 1, 2);
    WINDOW *resource_win = create_window(10, 50, 8, 2);
    WINDOW *task1_win    = create_window(10, 28, 8, 55);
    WINDOW *task2_win    = create_window(10, 28, 19, 2);
    WINDOW *task3_win    = create_window(10, 28, 19, 31);
    WINDOW *task4_win    = create_window(10, 28, 19, 60);
    WINDOW *log_win      = create_window(12, 90, 30, 2);

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
    ipc messenger(task_count, &swapper);

    swapper.set_log_window(log_win);
    resource1_sema.set_log_window(log_win);
    messenger.set_log_window(log_win);

    // Creating the tasks.
    tcb *tasks[task_count];
    for (int i = 0; i < task_count; ++i)
    {
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "Task%d", i + 1);
        tasks[i] = swapper.create_task(task_name, task_windows[i]);

        thread_args[i].log_win = log_win;
        thread_args[i].sched = &swapper;
        thread_args[i].messenger = &messenger;
        thread_args[i].task = tasks[i];
    }

    // Start threads.
    for (int i = 0; i < task_count; ++i)
    {
        pthread_create(&threads[i], NULL, perform_simple_output, &thread_args[i]);
        tasks[i]->thread = threads[i];
    }

    swapper.dump();
    resource1_sema.dump(1);
    messenger.ipc_Message_Dump();

    // Start scheduling.
    swapper.start();

    // Join threads.
    for (int i = 0; i < task_count; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    write_window(log_win, "All worker threads joined.\n");
    swapper.dump();
    resource1_sema.dump(1);
    messenger.ipc_Message_Dump();

    swapper.garbage_collect();
    write_window(log_win, "Running garbage collector...\n");
    swapper.dump();

    write_window(resource_win, "All tasks completed. Press any key to exit.\n");
    wgetch(resource_win);

    endwin();
    return 0;
}
