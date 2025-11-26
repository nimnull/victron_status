/*
 * Screen Timeout Module
 * Turns off display after inactivity and wakes on touch
 */

#ifndef SCREEN_TIMEOUT_H
#define SCREEN_TIMEOUT_H

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"

#define SCREEN_TIMEOUT_MS 15000  // 15 seconds

/**
 * Initialize the screen timeout module
 * @param display LVGL display handle
 * @param panel LCD panel handle
 * @return ESP_OK on success
 */
esp_err_t screen_timeout_init(lv_display_t *display, esp_lcd_panel_handle_t panel);

/**
 * Check and update screen timeout state
 * Call this periodically from LVGL port task
 * Must be called with LVGL mutex held
 */
void screen_timeout_check(void);

/**
 * Handle touch event for wake detection
 * Call this from touch callback BEFORE processing touch
 * @param pressed true if touch is pressed
 * @return true if touch should be passed to LVGL, false if consumed for wake
 */
bool screen_timeout_process_touch(bool pressed);

/**
 * Query if screen is currently on
 */
bool screen_timeout_is_on(void);

/**
 * Manually turn screen on (e.g., for notifications)
 */
void screen_timeout_wake(void);

#endif // SCREEN_TIMEOUT_H
