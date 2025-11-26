/*
 * Display Driver - SH8601 AMOLED via QSPI
 * Handles display hardware initialization and LVGL integration
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

/* Display resolution (rotated 90 degrees) */
#define DISPLAY_H_RES   600
#define DISPLAY_V_RES   450

/**
 * Initialize display hardware and LVGL display driver
 * Creates LVGL display with double-buffered DMA rendering
 */
esp_err_t display_driver_init(void);

/**
 * Get the LCD panel handle for power control
 */
esp_lcd_panel_handle_t display_driver_get_panel(void);

/**
 * Get the LVGL display handle
 */
lv_display_t *display_driver_get_lvgl_display(void);

#endif /* DISPLAY_DRIVER_H */
