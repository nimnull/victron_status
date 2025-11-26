#ifndef UI_STATUS_H
#define UI_STATUS_H

#include "lvgl.h"
#include "esp_err.h"

/**
 * @brief Create the status display UI
 *
 * @param parent Parent LVGL object to add the status display to
 * @return ESP_OK on success
 */
esp_err_t ui_status_create(lv_obj_t *parent);

/**
 * @brief Update the AC grid connection status display
 *
 * This function should be called whenever the AC grid status changes.
 * It updates the UI in a thread-safe manner.
 */
void ui_status_update_grid(void);

/**
 * @brief Update WiFi connection status display
 *
 * @param connected true if WiFi is connected, false otherwise
 */
void ui_status_update_wifi(bool connected);

/**
 * @brief Update MQTT connection status display
 *
 * @param connected true if MQTT is connected, false otherwise
 */
void ui_status_update_mqtt(bool connected);

#endif // UI_STATUS_H