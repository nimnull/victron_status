/*
 * Application Logic - Victron MQTT Monitor
 * Handles WiFi, MQTT connectivity, keepalive, and UI initialization
 */

#include "app_victron.h"
#include "display_driver.h"
#include "lvgl_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "network_manager.h"
#include "victron_mqtt.h"
#include "victron_data.h"
#include "ui_status.h"

static const char *TAG = "app_victron";

/* Keepalive timer handle */
static esp_timer_handle_t s_keepalive_timer = NULL;

/* Timer interval in microseconds (15 seconds) */
#define KEEPALIVE_INTERVAL_US   (15000 * 1000)

/* Status update interval in ticks (10 seconds) */
#define STATUS_UPDATE_INTERVAL  pdMS_TO_TICKS(10000)

static void keepalive_timer_callback(void *arg)
{
    ESP_LOGD(TAG, "Sending MQTT keepalive messages");
    esp_err_t ret = victron_mqtt_send_keepalive_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send some keepalive messages");
    }
}

static esp_err_t start_keepalive_timer(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &keepalive_timer_callback,
        .name = "mqtt_keepalive"
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_keepalive_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create keepalive timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_timer_start_periodic(s_keepalive_timer, KEEPALIVE_INTERVAL_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start keepalive timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT keepalive timer started (15s interval)");
    return ESP_OK;
}

static void create_ui(void)
{
    if (lvgl_port_lock(-1)) {
        lv_obj_clean(lv_screen_active());
        ui_status_create(lv_screen_active());
        lvgl_port_unlock();
    }
}

esp_err_t app_victron_init(void)
{
    esp_err_t ret;

    /* Initialize Victron data structures */
    ret = victron_data_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Victron data: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create the status UI */
    create_ui();

    ESP_LOGI(TAG, "Starting Victron MQTT Monitor...");

    /* Initialize WiFi connection */
    ESP_LOGI(TAG, "Initializing WiFi...");
    ret = network_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi connected successfully");
    ui_status_update_wifi(true);

    /* Initialize and start MQTT clients for both systems */
    ESP_LOGI(TAG, "Initializing MQTT clients for both systems...");
    ret = victron_mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT init warning: %s", esp_err_to_name(ret));
    }

    ret = victron_mqtt_start_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT start warning: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "MQTT clients started successfully for both systems");

    /* Wait for MQTT connections to establish */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Update MQTT status */
    bool system1_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_1);
    bool system2_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_2);
    ui_status_update_mqtt(system1_connected || system2_connected);

    /* Register callback to update UI when data is received */
    victron_mqtt_register_data_callback((mqtt_data_callback_t)ui_status_update_grid);

    /* Send initial keepalive */
    victron_mqtt_send_keepalive_all();

    /* Start keepalive timer */
    start_keepalive_timer();

    ESP_LOGI(TAG, "Victron MQTT Monitor initialized");
    return ESP_OK;
}

void app_victron_run(void)
{
    while (1) {
        /* Update WiFi connection status */
        ui_status_update_wifi(network_is_connected());

        /* Update MQTT status for both systems */
        bool system1_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_1);
        bool system2_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_2);
        ui_status_update_mqtt(system1_connected || system2_connected);

        /* Log diagnostics */
        ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "System connections - Green: %s, Yellow: %s",
                 system1_connected ? "Connected" : "Disconnected",
                 system2_connected ? "Connected" : "Disconnected");

        vTaskDelay(STATUS_UPDATE_INTERVAL);
    }
}
