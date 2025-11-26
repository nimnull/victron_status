/*
 * Application Logic - Victron MQTT Monitor
 * Handles WiFi, MQTT connectivity, keepalive, and UI initialization
 */

#ifndef APP_VICTRON_H
#define APP_VICTRON_H

#include "esp_err.h"

/**
 * Initialize the Victron monitor application
 * Sets up WiFi, MQTT clients, data callbacks, and UI
 */
esp_err_t app_victron_init(void);

/**
 * Run the application main loop
 * Updates connection statuses and logs diagnostics periodically
 * This function does not return under normal operation
 */
void app_victron_run(void);

#endif /* APP_VICTRON_H */
