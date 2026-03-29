#include <iostream>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>
#include <stdarg.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdio>
#include "Sched.h"
#include "Sema.h"

using namespace std;

pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;

void display_screen_data();
WINDOW *create_window(int height, int width, int starty, int startx);
void write_window(WINDOW * Win, const char* text);
void write_window(WINDOW * Win, int x, int y, const char* text);
void display_help(WINDOW * Win);
void *perform_simple_output(void *arguments);

struct thread_data
{
    int thread_no;
    WINDOW *thread_win;
    WINDOW *resource_win;
    WINDOW *log_win;
    bool kill_signal;
    int sleep_time;
    int thread_results;

    scheduler *sched;
    semaphore *sem;
    tcb *task;
};

void display_screen_data()
{
    int Y;
    int X;
    int Max_X;
    int Max_Y;

    getmaxyx(stdscr, Max_Y, Max_X);
    getmaxyx(stdscr, Y, X);
    refresh();
}

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

void write_window(WINDOW * Win, const char* text)
{
    pthread_mutex_lock(&myMutex);

    if (Win != NULL && text != NULL)
    {
        wprintw(Win, text);
        box(Win, 0, 0);
        wrefresh(Win);
    }

    pthread_mutex_unlock(&myMutex);
}

void write_window(WINDOW * Win, int x, int y, const char* text)
{
    pthread_mutex_lock(&myMutex);

    if (Win != NULL && text != NULL)
    {
        mvwprintw(Win, y, x, text);
        box(Win, 0, 0);
        wrefresh(Win);
    }

    pthread_mutex_unlock(&myMutex);
}

void display_help(WINDOW * Win)
{
    wclear(Win);
    write_window(Win, 1, 1, "...Help...");
    write_window(Win, 2, 1, "q= Quit");
}

void *perform_simple_output(void *arguments)
{
    thread_data *td = (thread_data *) arguments;

    scheduler *sched = td->sched;
    semaphore *sem = td->sem;
    tcb *task = td->task;
    WINDOW *resource_win = td->resource_win;
    WINDOW *log_win = td->log_win;

    char buff[256];

    while (!task->kill_signal && task->work_counter < 5)
    {
        sched->wait_until_running(task);

        if (task->kill_signal || task->state == DEAD)
        {
            break;
        }

        sprintf(buff, " Task %d running #%d\n",
                task->task_id, task->work_counter);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        sem->down(task->task_id);

        sem->dump(1);
        sched->dump();

        if (task->state == BLOCKED)
        {
            sprintf(buff, " Task %d waiting resource\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);
            continue;
        }

        sprintf(buff, " Task %d got resource\n", task->task_id);
        write_window(resource_win, buff);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        sleep(1);

        sprintf(buff, " Task %d released resource\n", task->task_id);
        write_window(resource_win, buff);
        write_window(log_win, buff);

        sem->up();

        sem->dump(1);
        sched->dump();

        task->work_counter++;

        if (task->work_counter >= 5)
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

    return(NULL);
}

int main()
{
    pthread_t thread_1;
    pthread_t thread_2;
    pthread_t thread_3;
    pthread_t thread_4;

    thread_data thread_args_1;
    thread_data thread_args_2;
    thread_data thread_args_3;
    thread_data thread_args_4;

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

    write_window(heading_win, 2, 1, "ULTIMA 2.0 - Phase 1 Scheduler and Semaphore");
    write_window(heading_win, 2, 2, "by Shivansh Mahay and Moises Navarro");
    write_window(resource_win, 2, 1, "Shared Resource Window");
    write_window(log_win, 2, 1, "Log Window");

    WINDOW *task_windows[4];
    task_windows[0] = task1_win;
    task_windows[1] = task2_win;
    task_windows[2] = task3_win;
    task_windows[3] = task4_win;

    int task_count = sizeof(task_windows) / sizeof(task_windows[0]);

    scheduler swapper(task_count);
    semaphore resource1_sema(1, "resource1", &swapper);
    swapper.set_log_window(log_win);
    resource1_sema.set_log_window(log_win);

    tcb *t1 = swapper.create_task("Task1", task1_win);
    tcb *t2 = swapper.create_task("Task2", task2_win);
    tcb *t3 = swapper.create_task("Task3", task3_win);
    tcb *t4 = swapper.create_task("Task4", task4_win);

    thread_args_1.thread_no = 1;
    thread_args_1.thread_win = task1_win;
    thread_args_1.resource_win = resource_win;
    thread_args_1.log_win = log_win;
    thread_args_1.kill_signal = false;
    thread_args_1.sleep_time = 0;
    thread_args_1.thread_results = 0;
    thread_args_1.sched = &swapper;
    thread_args_1.sem = &resource1_sema;
    thread_args_1.task = t1;

    thread_args_2.thread_no = 2;
    thread_args_2.thread_win = task2_win;
    thread_args_2.resource_win = resource_win;
    thread_args_2.log_win = log_win;
    thread_args_2.kill_signal = false;
    thread_args_2.sleep_time = 0;
    thread_args_2.thread_results = 0;
    thread_args_2.sched = &swapper;
    thread_args_2.sem = &resource1_sema;
    thread_args_2.task = t2;

    thread_args_3.thread_no = 3;
    thread_args_3.thread_win = task3_win;
    thread_args_3.resource_win = resource_win;
    thread_args_3.log_win = log_win;
    thread_args_3.kill_signal = false;
    thread_args_3.sleep_time = 0;
    thread_args_3.thread_results = 0;
    thread_args_3.sched = &swapper;
    thread_args_3.sem = &resource1_sema;
    thread_args_3.task = t3;

    thread_args_4.thread_no = 4;
    thread_args_4.thread_win = task4_win;
    thread_args_4.resource_win = resource_win;
    thread_args_4.log_win = log_win;
    thread_args_4.kill_signal = false;
    thread_args_4.sleep_time = 0;
    thread_args_4.thread_results = 0;
    thread_args_4.sched = &swapper;
    thread_args_4.sem = &resource1_sema;
    thread_args_4.task = t4;

    pthread_create(&thread_1, NULL, perform_simple_output, &thread_args_1);
    pthread_create(&thread_2, NULL, perform_simple_output, &thread_args_2);
    pthread_create(&thread_3, NULL, perform_simple_output, &thread_args_3);
    pthread_create(&thread_4, NULL, perform_simple_output, &thread_args_4);

    t1->thread = thread_1;
    t2->thread = thread_2;
    t3->thread = thread_3;
    t4->thread = thread_4;

    swapper.dump();
    resource1_sema.dump(1);

    swapper.start();

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
    pthread_join(thread_3, NULL);
    pthread_join(thread_4, NULL);

    write_window(log_win, "All worker threads joined.\n");
    swapper.dump();
    resource1_sema.dump(1);

    swapper.garbage_collect();
    write_window(log_win, "Running garbage collector...\n");
    swapper.dump();

    write_window(resource_win, "All tasks completed. Press any key to exit.\n");
    wgetch(resource_win);

    endwin();
    return 0;
}
