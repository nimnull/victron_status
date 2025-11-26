/*
 * Touch Driver - FT6336U Capacitive Touch via I2C
 * Handles touch hardware initialization and LVGL input device
 */

#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include "esp_err.h"
#include "esp_lcd_touch.h"

/**
 * Initialize touch controller hardware
 */
esp_err_t touch_driver_init(void);

/**
 * Register touch input device with LVGL
 * Must be called after touch_driver_init() and lvgl_port_init()
 */
void touch_driver_register_lvgl_indev(void);

/**
 * Get the touch controller handle
 */
esp_lcd_touch_handle_t touch_driver_get_handle(void);

#endif /* TOUCH_DRIVER_H */
