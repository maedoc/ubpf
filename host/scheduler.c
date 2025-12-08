#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "scheduler.h"

#define MAX_TASKS 10
#define STACK_SIZE (64 * 1024)

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_BLOCKED,
    TASK_FINISHED
} task_state_t;

struct sim_task {
    ucontext_t ctx;
    char stack[STACK_SIZE];
    task_state_t state;
    uint64_t wake_time;
    int id;
};

static struct sim_task tasks[MAX_TASKS];
static int current_task_idx = -1;
static int active_tasks = 0;
static ucontext_t sched_ctx;
static uint64_t tick_count = 0;

// --- Time Helper ---
// For simulation, we can either fast-forward time or wait real time.
// Let's fast-forward if all tasks are blocked, otherwise consume real time?
// To be simple: We'll just increment tick_count when running the idle loop.
// But for "feeling" the delay, let's use real system time as the base.

static uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint64_t start_time = 0;

uint64_t sim_get_tick(void) {
    return get_time_ms() - start_time;
}

// --- Scheduler ---

void sim_init(void) {
    memset(tasks, 0, sizeof(tasks));
    start_time = get_time_ms();
    active_tasks = 0;
    current_task_idx = -1;
}

static void task_entry_wrapper(void (*func)(void*), void *arg) {
    func(arg);
    tasks[current_task_idx].state = TASK_FINISHED;
    active_tasks--;
    // Switch back to scheduler
    swapcontext(&tasks[current_task_idx].ctx, &sched_ctx);
}

void sim_task_create(void (*func)(void*), void *arg) {
    if (active_tasks >= MAX_TASKS) {
        fprintf(stderr, "Sim: Max tasks reached\n");
        return;
    }

    int idx = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_FINISHED) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return;

    getcontext(&tasks[idx].ctx);
    tasks[idx].ctx.uc_stack.ss_sp = tasks[idx].stack;
    tasks[idx].ctx.uc_stack.ss_size = STACK_SIZE;
    tasks[idx].ctx.uc_link = &sched_ctx;
    
    // makecontext requires void(*)(void), we pass args via wrapper
    makecontext(&tasks[idx].ctx, (void (*)())task_entry_wrapper, 2, func, arg);

    tasks[idx].state = TASK_READY;
    tasks[idx].id = idx;
    tasks[idx].wake_time = 0;
    active_tasks++;
}

void sim_task_delay(int ms) {
    if (current_task_idx < 0) return;
    
    tasks[current_task_idx].state = TASK_BLOCKED;
    tasks[current_task_idx].wake_time = sim_get_tick() + ms;
    
    // Yield
    swapcontext(&tasks[current_task_idx].ctx, &sched_ctx);
}

void sim_run(void) {
    printf("[Sim] Scheduler starting with %d tasks\n", active_tasks);

    while (active_tasks > 0) {
        uint64_t now = sim_get_tick();
        int runnable_found = 0;

        // Simple Round Robin
        for (int i = 0; i < MAX_TASKS; i++) {
            int idx = (current_task_idx + 1 + i) % MAX_TASKS;
            
            if (tasks[idx].state == TASK_BLOCKED) {
                if (now >= tasks[idx].wake_time) {
                    tasks[idx].state = TASK_READY;
                }
            }

            if (tasks[idx].state == TASK_READY) {
                current_task_idx = idx;
                runnable_found = 1;
                swapcontext(&sched_ctx, &tasks[idx].ctx);
                // Return here after task yields
                break; 
            }
        }

        if (!runnable_found && active_tasks > 0) {
            // All tasks blocked? Sleep a bit to avoid burning CPU
            usleep(1000);
        }
    }
    printf("[Sim] All tasks finished.\n");
}
