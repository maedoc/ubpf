#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/**
 * @brief Initialize the simulation scheduler.
 */
void sim_init(void);

/**
 * @brief Create a simulated task (green thread).
 * 
 * @param func Entry function
 * @param arg Argument to pass to function
 */
void sim_task_create(void (*func)(void*), void *arg);

/**
 * @brief Delay the current task (yield control).
 * 
 * @param ms Milliseconds to delay
 */
void sim_task_delay(int ms);

/**
 * @brief Run the scheduler loop. Returns when all tasks have exited.
 */
void sim_run(void);

/**
 * @brief Get current simulated tick count (ms)
 */
uint64_t sim_get_tick(void);

#endif
