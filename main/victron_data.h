#ifndef VICTRON_DATA_H
#define VICTRON_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief System identifier enum
 */
typedef enum {
    VICTRON_SYSTEM_1 = 0,    /**< Green system */
    VICTRON_SYSTEM_2 = 1,    /**< Yellow system */
    VICTRON_SYSTEM_MAX       /**< Maximum number of systems */
} victron_system_id_t;

/**
 * @brief Structure to hold Victron AC grid status information for one system
 */
typedef struct {
    bool ac_grid_connected;      /**< AC grid connection status */
    float ac_voltage;            /**< AC voltage (V) */
    float ac_power;              /**< AC power (W) */
    time_t last_update;          /**< Timestamp of last update */
    const char *system_name;     /**< Display name for the system */
} victron_ac_status_t;

/**
 * @brief Multi-system Victron data structure
 */
typedef struct {
    victron_ac_status_t systems[VICTRON_SYSTEM_MAX];  /**< Array of system statuses */
    SemaphoreHandle_t data_mutex;                     /**< Mutex for thread-safe access */
} victron_multi_system_t;

/**
 * @brief Global multi-system Victron data
 */
extern victron_multi_system_t g_victron_data;

/**
 * @brief Initialize Victron data structures
 * @return ESP_OK on success
 */
esp_err_t victron_data_init(void);

/**
 * @brief Update AC grid connection status for a specific system
 * @param system_id System identifier
 * @param connected True if grid is connected
 */
void victron_data_update_grid_status(victron_system_id_t system_id, bool connected);

/**
 * @brief Get AC grid connection status for a specific system (thread-safe)
 * @param system_id System identifier
 * @param[out] connected Pointer to store connection status
 * @return ESP_OK on success
 */
esp_err_t victron_data_get_grid_status(victron_system_id_t system_id, bool *connected);

/**
 * @brief Get combined grid status (both systems)
 * @param[out] system1_connected System 1 connection status
 * @param[out] system2_connected System 2 connection status
 * @return ESP_OK on success
 */
esp_err_t victron_data_get_combined_status(bool *system1_connected, bool *system2_connected);

#endif // VICTRON_DATA_H