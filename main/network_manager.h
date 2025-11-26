#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include "esp_event.h"

/**
 * @brief WiFi connection states
 */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR
} wifi_state_t;

/**
 * @brief Initialize and start WiFi connection
 *
 * This function initializes the WiFi subsystem and attempts to connect
 * to the configured network. It will block until connection is established
 * or maximum retries are exceeded.
 *
 * @return ESP_OK on successful connection, ESP_FAIL otherwise
 */
esp_err_t network_init(void);

/**
 * @brief Get current WiFi connection state
 * @return Current WiFi state
 */
wifi_state_t network_get_state(void);

/**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool network_is_connected(void);

/**
 * @brief Disconnect and stop WiFi
 * @return ESP_OK on success
 */
esp_err_t network_deinit(void);

#endif // NETWORK_MANAGER_H