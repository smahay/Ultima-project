#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include "Sched.h"
#include "Sema.h"

using namespace std;

pthread_mutex_t screen_lock = PTHREAD_MUTEX_INITIALIZER;
WINDOW *global_log_win = NULL;

void get_content_bounds(WINDOW *Win, int &top, int &left, int &bottom, int &right)
{
    int max_y = 0;
    int max_x = 0;
    getmaxyx(Win, max_y, max_x);

    top = 1;
    left = 1;
    bottom = max_y - 2;
    right = max_x - 2;
}

int clamp_value(int value, int low, int high)
{
    if (value < low)
    {
        return low;
    }
    if (value > high)
    {
        return high;
    }
    return value;
}

void advance_cursor(WINDOW *Win, int top, int left, int bottom, int &y, int &x)
{
    if (y >= bottom)
    {
        wscrl(Win, 1);
        box(Win, 0, 0);
        y = bottom;
    }
    else
    {
        y++;
    }
    x = left;
}

void append_window_text_locked(WINDOW *Win, const char *text)
{
    if (Win == NULL || text == NULL)
    {
        return;
    }

    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
    get_content_bounds(Win, top, left, bottom, right);

    if (bottom < top || right < left)
    {
        return;
    }

    int row = 0;
    int col = 0;
    getyx(Win, row, col);

    // Keep cursor inside window content area.
    if (row < top || row > bottom || col < left || col > right)
    {
        row = top;
        col = left;
    }

    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        const char ch = *cursor;

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            advance_cursor(Win, top, left, bottom, row, col);
            continue;
        }

        // Wrap line when we run out of columns.
        if (col > right)
        {
            advance_cursor(Win, top, left, bottom, row, col);
        }

        mvwaddch(Win, row, col, ch);
        col++;
    }

    if (col > right)
    {
        advance_cursor(Win, top, left, bottom, row, col);
    }

    wmove(Win, row, col);
    wrefresh(Win);
}

WINDOW *create_window(int height, int width, int starty, int startx)
{
    WINDOW *Win = newwin(height, width, starty, startx);
    if (Win == NULL)
    {
        return NULL;
    }

    if (height >= 3 && width >= 3)
    {
        wsetscrreg(Win, 1, height - 2);
        scrollok(Win, TRUE);
        wmove(Win, 1, 1);
    }

    box(Win, 0, 0);
    wrefresh(Win);
    return Win;
}

void write_window(WINDOW *Win, const char *text)
{
    pthread_mutex_lock(&screen_lock);
    append_window_text_locked(Win, text);
    pthread_mutex_unlock(&screen_lock);
}

void write_window(WINDOW *Win, int x, int y, const char *text)
{
    pthread_mutex_lock(&screen_lock);

    if (Win != NULL && text != NULL)
    {
        int top = 0;
        int left = 0;
        int bottom = 0;
        int right = 0;
        get_content_bounds(Win, top, left, bottom, right);

        if (bottom >= top && right >= left)
        {
            int draw_y = clamp_value(y, top, bottom);
            int draw_x = clamp_value(x, left, right);

            const int max_chars = right - draw_x + 1;

            if (max_chars > 0)
            {
                mvwaddnstr(Win, draw_y, draw_x, text, max_chars);
                wmove(Win, draw_y, draw_x);
                wrefresh(Win);
            }
        }
    }

    pthread_mutex_unlock(&screen_lock);
}

void write_log(const char *text)
{
    if (global_log_win == NULL || text == NULL)
    {
        return;
    }

    write_window(global_log_win, text);
}

struct worker_args
{
    scheduler *sched;
    semaphore *sem;
    tcb *task;
    WINDOW *resource_win;
    WINDOW *log_win;
};

void *task_worker(void *arguments)
{
    worker_args *args = (worker_args *)arguments;
    scheduler *sched = args->sched;
    semaphore *sem = args->sem;
    tcb *task = args->task;
    WINDOW *resource_win = args->resource_win;
    WINDOW *log_win = args->log_win;

    char buff[256];

    while (!task->kill_signal && task->work_counter < 5)
    {
        sched->wait_until_running(task);

        if (task->kill_signal || task->state == DEAD)
        {
            break;
        }

        sprintf(buff, "Task %d is running. Iteration %d\n",
                task->task_id, task->work_counter);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        bool got_resource = sem->down(task->task_id);

        write_window(log_win, sem->dump_to_string(1).c_str());
        write_window(log_win, sched->dump_to_string().c_str());

        if (!got_resource)
        {
            sprintf(buff, "Task %d blocked waiting for resource.\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);
            continue;
        }

        sprintf(buff, "Task %d obtained shared resource.\n", task->task_id);
        write_window(resource_win, buff);
        write_window(task->task_win, buff);
        write_window(log_win, buff);

        sleep(1);

        sprintf(buff, "Task %d releasing shared resource.\n", task->task_id);
        write_window(resource_win, buff);
        write_window(log_win, buff);

        sem->up();

        write_window(log_win, sem->dump_to_string(1).c_str());
        write_window(log_win, sched->dump_to_string().c_str());

        task->work_counter++;

        if (task->work_counter >= 5)
        {
            sprintf(buff, "Task %d finished all work.\n", task->task_id);
            write_window(task->task_win, buff);
            write_window(log_win, buff);

            sched->kill_task(task->task_id);
            write_window(log_win, sched->dump_to_string().c_str());
            break;
        }

        sched->yield();
        write_window(log_win, sched->dump_to_string().c_str());
        sleep(1);
    }

    write_window(task->task_win, "TERMINATED\n");
    write_window(log_win, "A task terminated.\n");
    return NULL;
}

int main()
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    WINDOW *heading_win  = create_window(6, 90, 1, 2);
    WINDOW *resource_win = create_window(10, 50, 8, 2);
    WINDOW *task1_win    = create_window(10, 28, 8, 55);
    WINDOW *task2_win    = create_window(10, 28, 19, 2);
    WINDOW *task3_win    = create_window(10, 28, 19, 31);
    WINDOW *task4_win    = create_window(10, 28, 19, 60);
    WINDOW *log_win      = create_window(12, 90, 30, 2);

    write_window(heading_win, 2, 1, "ULTIMA 2.0 - Phase 1 Scheduler and Semaphore");
    write_window(heading_win, 2, 2, "pthread + ncurses baseline");
    write_window(resource_win, 2, 1, "Shared Resource Window");
    write_window(log_win, 2, 1, "Log Window");

    scheduler swapper;
    semaphore resource1_sema(1, "resource1", &swapper);
    global_log_win = log_win;

    tcb *t1 = swapper.create_task("Task1", task1_win);
    tcb *t2 = swapper.create_task("Task2", task2_win);
    tcb *t3 = swapper.create_task("Task3", task3_win);
    tcb *t4 = swapper.create_task("Task4", task4_win);

    worker_args a1{&swapper, &resource1_sema, t1, resource_win, log_win};
    worker_args a2{&swapper, &resource1_sema, t2, resource_win, log_win};
    worker_args a3{&swapper, &resource1_sema, t3, resource_win, log_win};
    worker_args a4{&swapper, &resource1_sema, t4, resource_win, log_win};

    pthread_create(&t1->thread, NULL, task_worker, &a1);
    pthread_create(&t2->thread, NULL, task_worker, &a2);
    pthread_create(&t3->thread, NULL, task_worker, &a3);
    pthread_create(&t4->thread, NULL, task_worker, &a4);

    write_window(log_win, swapper.dump_to_string().c_str());
    write_window(log_win, resource1_sema.dump_to_string(1).c_str());

    swapper.start();

    pthread_join(t1->thread, NULL);
    pthread_join(t2->thread, NULL);
    pthread_join(t3->thread, NULL);
    pthread_join(t4->thread, NULL);

    write_window(log_win, "All worker threads joined.\n");
    write_window(log_win, swapper.dump_to_string().c_str());
    write_window(log_win, resource1_sema.dump_to_string(1).c_str());

    swapper.garbage_collect();
    write_window(log_win, "Running garbage collector...\n");
    write_window(log_win, swapper.dump_to_string().c_str());

    write_window(resource_win, "All tasks completed. Press any key to exit.\n");
    wgetch(resource_win);

    endwin();
    return 0;
}
