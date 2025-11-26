#include "victron_data.h"
#include "esp_log.h"

static const char *TAG = "victron_data";

// Global multi-system Victron data
victron_multi_system_t g_victron_data = {0};

esp_err_t victron_data_init(void)
{
    ESP_LOGI(TAG, "Initializing Victron multi-system data structures...");

    // Initialize the mutex
    g_victron_data.data_mutex = xSemaphoreCreateMutex();
    if (g_victron_data.data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create data mutex");
        return ESP_FAIL;
    }

    // Initialize default values for both systems
    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        g_victron_data.systems[i].ac_grid_connected = false;
        g_victron_data.systems[i].ac_voltage = 0.0f;
        g_victron_data.systems[i].ac_power = 0.0f;
        g_victron_data.systems[i].last_update = 0;
    }

    // Set system names
    g_victron_data.systems[VICTRON_SYSTEM_1].system_name = CONFIG_VICTRON_SYSTEM_NAME_1;
    g_victron_data.systems[VICTRON_SYSTEM_2].system_name = CONFIG_VICTRON_SYSTEM_NAME_2;

    ESP_LOGI(TAG, "Victron multi-system data structures initialized");
    return ESP_OK;
}

void victron_data_update_grid_status(victron_system_id_t system_id, bool connected)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        ESP_LOGE(TAG, "Invalid system ID: %d", system_id);
        return;
    }

    if (g_victron_data.data_mutex == NULL) {
        ESP_LOGE(TAG, "Data mutex not initialized");
        return;
    }

    if (xSemaphoreTake(g_victron_data.data_mutex, portMAX_DELAY) == pdTRUE) {
        g_victron_data.systems[system_id].ac_grid_connected = connected;
        g_victron_data.systems[system_id].last_update = time(NULL);
        xSemaphoreGive(g_victron_data.data_mutex);

        ESP_LOGI(TAG, "System %s grid status updated: %s",
                 g_victron_data.systems[system_id].system_name,
                 connected ? "Connected" : "Disconnected");
    } else {
        ESP_LOGE(TAG, "Failed to take data mutex");
    }
}

esp_err_t victron_data_get_grid_status(victron_system_id_t system_id, bool *connected)
{
    if (connected == NULL || system_id >= VICTRON_SYSTEM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_victron_data.data_mutex == NULL) {
        ESP_LOGE(TAG, "Data mutex not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(g_victron_data.data_mutex, portMAX_DELAY) == pdTRUE) {
        *connected = g_victron_data.systems[system_id].ac_grid_connected;
        xSemaphoreGive(g_victron_data.data_mutex);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take data mutex");
        return ESP_FAIL;
    }
}

esp_err_t victron_data_get_combined_status(bool *system1_connected, bool *system2_connected)
{
    if (system1_connected == NULL || system2_connected == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_victron_data.data_mutex == NULL) {
        ESP_LOGE(TAG, "Data mutex not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(g_victron_data.data_mutex, portMAX_DELAY) == pdTRUE) {
        *system1_connected = g_victron_data.systems[VICTRON_SYSTEM_1].ac_grid_connected;
        *system2_connected = g_victron_data.systems[VICTRON_SYSTEM_2].ac_grid_connected;
        xSemaphoreGive(g_victron_data.data_mutex);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take data mutex");
        return ESP_FAIL;
    }
}