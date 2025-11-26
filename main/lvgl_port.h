/*
 * LVGL Port - Runtime Management
 * Handles LVGL tick timer, mutex synchronization, and main task
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdbool.h>
#include "esp_err.h"

/* LVGL timing configuration */
#define LVGL_TICK_PERIOD_MS     2
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  1
#define LVGL_TASK_STACK_SIZE    (8 * 1024)
#define LVGL_TASK_PRIORITY      2

/**
 * Initialize LVGL port (tick timer and mutex)
 * Must be called after lv_init() and before starting LVGL task
 */
esp_err_t lvgl_port_init(void);

/**
 * Acquire LVGL mutex for thread-safe access
 * @param timeout_ms Timeout in ms, -1 for infinite wait
 * @return true if lock acquired, false on timeout
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * Release LVGL mutex
 */
void lvgl_port_unlock(void);

/**
 * Start the LVGL task
 * Task handles lv_timer_handler() and screen timeout checks
 */
void lvgl_port_start_task(void);

#endif /* LVGL_PORT_H */
