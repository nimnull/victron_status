#ifndef VICTRON_MQTT_H
#define VICTRON_MQTT_H

#include "esp_err.h"
#include "mqtt_client.h"
#include "victron_data.h"

/**
 * @brief MQTT connection states
 */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_state_t;

/**
 * @brief Callback function type for MQTT data reception
 *
 * @param system_id System identifier
 * @param topic Topic string
 * @param data Data payload
 * @param data_len Length of data payload
 */
typedef void (*mqtt_data_callback_t)(victron_system_id_t system_id, const char *topic, const char *data, int data_len);

/**
 * @brief Initialize MQTT clients for both systems
 *
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_init(void);

/**
 * @brief Start MQTT client for a specific system
 *
 * @param system_id System identifier
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_start(victron_system_id_t system_id);

/**
 * @brief Start all MQTT clients
 *
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_start_all(void);

/**
 * @brief Stop MQTT client for a specific system
 *
 * @param system_id System identifier
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_stop(victron_system_id_t system_id);

/**
 * @brief Stop all MQTT clients
 *
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_stop_all(void);

/**
 * @brief Get current MQTT connection state for a system
 *
 * @param system_id System identifier
 * @return Current MQTT state
 */
mqtt_state_t victron_mqtt_get_state(victron_system_id_t system_id);

/**
 * @brief Check if MQTT is connected for a system
 *
 * @param system_id System identifier
 * @return true if connected, false otherwise
 */
bool victron_mqtt_is_connected(victron_system_id_t system_id);

/**
 * @brief Register callback for data reception
 *
 * @param callback Callback function
 */
void victron_mqtt_register_data_callback(mqtt_data_callback_t callback);

/**
 * @brief Send keepalive message for a specific system
 *
 * @param system_id System identifier
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_send_keepalive(victron_system_id_t system_id);

/**
 * @brief Send keepalive messages for all connected systems
 *
 * @return ESP_OK on success
 */
esp_err_t victron_mqtt_send_keepalive_all(void);

esp_err_t mqtt_client_init(victron_system_id_t system_id);

#endif // VICTRON_MQTT_H