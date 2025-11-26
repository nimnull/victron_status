#include "victron_mqtt.h"
#include "victron_data.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>

static const char *TAG = "victron_mqtt";

/**
 * @brief MQTT instance structure for each system
 */
typedef struct {
    esp_mqtt_client_handle_t client;
    mqtt_state_t state;
    victron_system_id_t system_id;
    const char *broker_url;
    const char *system_name;
    const char *victron_id;
} victron_mqtt_instance_t;

// Array of MQTT instances for each system
static victron_mqtt_instance_t mqtt_instances[VICTRON_SYSTEM_MAX] = {
    [VICTRON_SYSTEM_1] = {
        .client = NULL,
        .state = MQTT_STATE_DISCONNECTED,
        .system_id = VICTRON_SYSTEM_1,
        .broker_url = CONFIG_MQTT_BROKER_URL_1,
        .system_name = CONFIG_VICTRON_SYSTEM_NAME_1,
        .victron_id = CONFIG_VICTRON_SYSTEM_ID_1
    },
    [VICTRON_SYSTEM_2] = {
        .client = NULL,
        .state = MQTT_STATE_DISCONNECTED,
        .system_id = VICTRON_SYSTEM_2,
        .broker_url = CONFIG_MQTT_BROKER_URL_2,
        .system_name = CONFIG_VICTRON_SYSTEM_NAME_2,
        .victron_id = CONFIG_VICTRON_SYSTEM_ID_2
    }
};

static mqtt_data_callback_t data_callback = NULL;

/**
 * @brief Parse AC grid status from JSON payload
 */
static void parse_ac_grid_status(victron_system_id_t system_id, const char *topic, const char *data, int data_len)
{
    // Check if this is the AC grid connected topic
    if (strstr(topic, "/Ac/ActiveIn/Connected") != NULL) {
        // Simple parsing for JSON {"value": 0} or {"value": 1}
        const char *value_str = strstr(data, "\"value\"");
        if (value_str) {
            const char *colon = strchr(value_str, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') {
                    colon++;
                }

                bool connected = false;

                // Check for numeric value (0 or 1) or boolean (true/false)
                if (*colon == '1' || strncmp(colon, "true", 4) == 0) {
                    connected = true;
                } else if (*colon == '0' || strncmp(colon, "false", 5) == 0) {
                    connected = false;
                } else if (*colon == '"') {
                    // String value "1" or "0"
                    colon++;
                    if (*colon == '1') {
                        connected = true;
                    }
                }

                // Update the global status for this system
                victron_data_update_grid_status(system_id, connected);

                ESP_LOGI(TAG, "System %d AC Grid Status: %s",
                         system_id, connected ? "Connected" : "Disconnected");
            }
        } else {
            ESP_LOGW(TAG, "Failed to find 'value' field in JSON payload for system %d", system_id);
        }
    }
}

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    // Find which system this event belongs to
    victron_mqtt_instance_t *instance = NULL;
    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        if (mqtt_instances[i].client == client) {
            instance = &mqtt_instances[i];
            break;
        }
    }

    if (!instance) {
        ESP_LOGE(TAG, "Unknown MQTT client in event handler");
        return;
    }

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        instance->state = MQTT_STATE_CONNECTED;
        ESP_LOGI(TAG, "System %s: MQTT Connected to broker", instance->system_name);

        // Subscribe to AC grid status topics for this system
        char topic[128];

        // Format: N/{system_id}/vebus/276/Ac/ActiveIn/Connected/#
        snprintf(topic, sizeof(topic), "N/%s/vebus/276/Ac/ActiveIn/Connected/#", instance->victron_id);
        int msg_id = esp_mqtt_client_subscribe(client, topic, 1);
        ESP_LOGI(TAG, "System %s: Subscribed to topic: %s, msg_id=%d", instance->system_name, topic, msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        instance->state = MQTT_STATE_DISCONNECTED;
        ESP_LOGI(TAG, "System %s: MQTT Disconnected", instance->system_name);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "System %s: MQTT Data received", instance->system_name);
        ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);

        // Parse the AC grid status
        parse_ac_grid_status(instance->system_id, event->topic, event->data, event->data_len);

        // Call user callback if registered
        if (data_callback) {
            data_callback(instance->system_id, event->topic, event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        instance->state = MQTT_STATE_ERROR;
        ESP_LOGE(TAG, "System %s: MQTT Error", instance->system_name);
        break;

    default:
        ESP_LOGI(TAG, "System %s: Other event id:%d", instance->system_name, event->event_id);
        break;
    }
}

esp_err_t victron_mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT clients for both systems...");

    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        victron_mqtt_instance_t *instance = &mqtt_instances[i];

        ESP_LOGI(TAG, "Initializing MQTT client for system %s", instance->system_name);
        ESP_LOGI(TAG, "  Broker: %s", instance->broker_url);
        ESP_LOGI(TAG, "  System ID: %s", instance->victron_id);

        const esp_mqtt_client_config_t mqtt_cfg = {
            .broker = {
                .address.uri = instance->broker_url,
            },
            .session = {
                .keepalive = 60,
                .disable_clean_session = false,
            },
            .network = {
                .reconnect_timeout_ms = 10000,
                .timeout_ms = 10000,
            },
        };

        instance->client = esp_mqtt_client_init(&mqtt_cfg);
        if (instance->client == NULL) {
            ESP_LOGE(TAG, "Failed to initialize MQTT client for system %s", instance->system_name);
            return ESP_FAIL;
        }

        esp_err_t ret = esp_mqtt_client_register_event(instance->client, ESP_EVENT_ANY_ID,
                                                       mqtt_event_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register MQTT event handler for system %s: %s",
                     instance->system_name, esp_err_to_name(ret));
            esp_mqtt_client_destroy(instance->client);
            instance->client = NULL;
            return ret;
        }
    }

    ESP_LOGI(TAG, "All MQTT clients initialized successfully");
    return ESP_OK;
}

esp_err_t victron_mqtt_start(victron_system_id_t system_id)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        ESP_LOGE(TAG, "Invalid system ID: %d", system_id);
        return ESP_ERR_INVALID_ARG;
    }

    victron_mqtt_instance_t *instance = &mqtt_instances[system_id];

    if (instance->client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized for system %s", instance->system_name);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting MQTT client for system %s...", instance->system_name);
    instance->state = MQTT_STATE_CONNECTING;

    esp_err_t ret = esp_mqtt_client_start(instance->client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client for system %s: %s",
                 instance->system_name, esp_err_to_name(ret));
        instance->state = MQTT_STATE_ERROR;
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started for system %s", instance->system_name);
    return ESP_OK;
}

esp_err_t victron_mqtt_start_all(void)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        esp_err_t system_ret = victron_mqtt_start((victron_system_id_t)i);
        if (system_ret != ESP_OK) {
            ret = system_ret;  // Keep track of any failures
        }
    }

    return ret;
}

esp_err_t victron_mqtt_stop(victron_system_id_t system_id)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        ESP_LOGE(TAG, "Invalid system ID: %d", system_id);
        return ESP_ERR_INVALID_ARG;
    }

    victron_mqtt_instance_t *instance = &mqtt_instances[system_id];

    if (instance->client == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MQTT client for system %s...", instance->system_name);

    esp_err_t ret = esp_mqtt_client_stop(instance->client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client for system %s: %s",
                 instance->system_name, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_mqtt_client_destroy(instance->client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy MQTT client for system %s: %s",
                 instance->system_name, esp_err_to_name(ret));
        return ret;
    }

    instance->client = NULL;
    instance->state = MQTT_STATE_DISCONNECTED;
    ESP_LOGI(TAG, "MQTT client stopped for system %s", instance->system_name);
    return ESP_OK;
}

esp_err_t victron_mqtt_stop_all(void)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        esp_err_t system_ret = victron_mqtt_stop((victron_system_id_t)i);
        if (system_ret != ESP_OK) {
            ret = system_ret;  // Keep track of any failures
        }
    }

    return ret;
}

mqtt_state_t victron_mqtt_get_state(victron_system_id_t system_id)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        return MQTT_STATE_ERROR;
    }

    return mqtt_instances[system_id].state;
}

bool victron_mqtt_is_connected(victron_system_id_t system_id)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        return false;
    }

    return mqtt_instances[system_id].state == MQTT_STATE_CONNECTED;
}

void victron_mqtt_register_data_callback(mqtt_data_callback_t callback)
{
    data_callback = callback;
}

esp_err_t victron_mqtt_send_keepalive(victron_system_id_t system_id)
{
    if (system_id >= VICTRON_SYSTEM_MAX) {
        ESP_LOGE(TAG, "Invalid system ID: %d", system_id);
        return ESP_ERR_INVALID_ARG;
    }

    victron_mqtt_instance_t *instance = &mqtt_instances[system_id];

    // Only send keepalive if connected
    if (instance->state != MQTT_STATE_CONNECTED || instance->client == NULL) {
        ESP_LOGW(TAG, "System %s not connected, skipping keepalive", instance->system_name);
        return ESP_ERR_INVALID_STATE;
    }

    // Construct the keepalive topic: R/{system_id}/keepalive
    char topic[128];
    snprintf(topic, sizeof(topic), "R/%s/keepalive", instance->victron_id);

    // Send empty message (just empty JSON object)
    const char *payload = "{}";

    int msg_id = esp_mqtt_client_publish(instance->client, topic, payload, strlen(payload), 1, 0);

    if (msg_id >= 0) {
        ESP_LOGD(TAG, "System %s: Keepalive sent to %s (msg_id=%d)",
                 instance->system_name, topic, msg_id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "System %s: Failed to send keepalive to %s",
                 instance->system_name, topic);
        return ESP_FAIL;
    }
}

esp_err_t victron_mqtt_send_keepalive_all(void)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < VICTRON_SYSTEM_MAX; i++) {
        esp_err_t system_ret = victron_mqtt_send_keepalive((victron_system_id_t)i);
        // Continue sending to other systems even if one fails
        if (system_ret != ESP_OK && system_ret != ESP_ERR_INVALID_STATE) {
            ret = system_ret;
        }
    }

    return ret;
}