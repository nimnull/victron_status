#include "ui_status.h"
#include "lvgl_port.h"
#include "victron_data.h"
#include "victron_mqtt.h"
#include "screen_timeout.h"
#include "esp_log.h"
#include <stdio.h>
#include "lvgl.h"

static const char *TAG = "ui_status";

// UI elements
static lv_obj_t *wifi_status_label = NULL;
static lv_obj_t *wifi_led = NULL;
static lv_obj_t *system1_mqtt_label = NULL;
static lv_obj_t *system1_mqtt_led = NULL;
static lv_obj_t *system1_grid_label = NULL;
static lv_obj_t *system1_grid_led = NULL;
static lv_obj_t *system2_mqtt_label = NULL;
static lv_obj_t *system2_mqtt_led = NULL;
static lv_obj_t *system2_grid_label = NULL;
static lv_obj_t *system2_grid_led = NULL;
static lv_obj_t *status_container = NULL;
static lv_obj_t *connections_container = NULL;

// LVGL timers for LED blink restoration
static lv_timer_t *system1_blink_timer = NULL;
static lv_timer_t *system2_blink_timer = NULL;

// LED brightness constants
#define LED_BRIGHTNESS_NORMAL  150
#define LED_BRIGHTNESS_BLINK   255
#define LED_BLINK_DURATION_MS  100

// Status LED colors #6af022
static const lv_color_t COLOR_CONNECTED = {.red = 0x6a, .green = 0xf0, .blue = 0x22};    // Dark green
static const lv_color_t COLOR_DISCONNECTED = {.red = 0x6e, .green = 0x1b, .blue = 0x1b}; // Dark red
static const lv_color_t COLOR_UNKNOWN = {.red = 0x70, .green = 0x70, .blue = 0x70};      // Gray


esp_err_t ui_status_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating multi-system status display UI");

    if (parent == NULL) {
        ESP_LOGE(TAG, "Parent object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Create a container for all status elements
    status_container = lv_obj_create(parent);
    lv_obj_set_size(status_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(status_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(status_container, 8, 0);
    lv_obj_set_style_pad_gap(status_container, 8, 0);

    // Create title label
    lv_obj_t *title = lv_label_create(status_container);
    lv_label_set_text(title, "Victron Multi-System Monitor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    connections_container = lv_obj_create(status_container);
    lv_obj_set_size(connections_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(connections_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(connections_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);


    // Create WiFi status with LED
    lv_obj_t *wifi_container = lv_obj_create(connections_container);
    lv_obj_set_size(wifi_container, LV_PCT(33), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wifi_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifi_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(wifi_container, 5, 0);
    lv_obj_set_style_bg_opa(wifi_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_container, 0, 0);

    wifi_status_label = lv_label_create(wifi_container);
    lv_label_set_text(wifi_status_label, "WiFi:");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFFFFF), 0);

    wifi_led = lv_led_create(wifi_container);
    lv_obj_set_size(wifi_led, 20, 20);
    lv_led_set_color(wifi_led, COLOR_UNKNOWN);
    lv_led_set_brightness(wifi_led, 150);

    // System 1 MQTT status with LED
    lv_obj_t *system1_mqtt_container = lv_obj_create(connections_container);
    lv_obj_set_size(system1_mqtt_container, LV_PCT(33), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(system1_mqtt_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(system1_mqtt_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(system1_mqtt_container, 5, 0);
    lv_obj_set_style_bg_opa(system1_mqtt_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system1_mqtt_container, 0, 0);

    system1_mqtt_label = lv_label_create(system1_mqtt_container);
    lv_label_set_text(system1_mqtt_label, "  MQTT 1:");
    lv_obj_set_style_text_color(system1_mqtt_label, lv_color_hex(0xFFFFFF), 0);

    system1_mqtt_led = lv_led_create(system1_mqtt_container);
    lv_obj_set_size(system1_mqtt_led, 20, 20);
    lv_led_set_color(system1_mqtt_led, COLOR_DISCONNECTED);
    lv_led_set_brightness(system1_mqtt_led, 150);

    // System 2 MQTT status with LED
    lv_obj_t *system2_mqtt_container = lv_obj_create(connections_container);
    lv_obj_set_size(system2_mqtt_container, LV_PCT(33), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(system2_mqtt_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(system2_mqtt_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(system2_mqtt_container, 5, 0);
    lv_obj_set_style_bg_opa(system2_mqtt_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system2_mqtt_container, 0, 0);

    system2_mqtt_label = lv_label_create(system2_mqtt_container);
    lv_label_set_text(system2_mqtt_label, "  MQTT 2:");
    lv_obj_set_style_text_color(system2_mqtt_label, lv_color_hex(0xFFFFFF), 0);

    system2_mqtt_led = lv_led_create(system2_mqtt_container);
    lv_obj_set_size(system2_mqtt_led, 20, 20);
    lv_led_set_color(system2_mqtt_led, COLOR_DISCONNECTED);
    lv_led_set_brightness(system2_mqtt_led, 150);

    // Add separator
    lv_obj_t *sep1 = lv_obj_create(status_container);
    lv_obj_set_size(sep1, LV_PCT(80), 1);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(0x404040), 0);

    // System 1 (Green) Section
    lv_obj_t *system1_title = lv_label_create(status_container);
    lv_label_set_text(system1_title, CONFIG_VICTRON_SYSTEM_NAME_1 " System:");
    lv_obj_set_style_text_font(system1_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(system1_title, lv_color_hex(0xFFFFFF), 0);

    // System 1 Grid status with LED
    lv_obj_t *system1_grid_container = lv_obj_create(status_container);
    lv_obj_set_size(system1_grid_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(system1_grid_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(system1_grid_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(system1_grid_container, 5, 0);
    lv_obj_set_style_bg_opa(system1_grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system1_grid_container, 0, 0);

    system1_grid_label = lv_label_create(system1_grid_container);
    lv_label_set_text(system1_grid_label, "  Grid:");
    lv_obj_set_style_text_font(system1_grid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(system1_grid_label, lv_color_hex(0xFFFFFF), 0);

    system1_grid_led = lv_led_create(system1_grid_container);
    lv_obj_set_size(system1_grid_led, 20, 20);
    lv_led_set_color(system1_grid_led, COLOR_UNKNOWN);
    lv_led_set_brightness(system1_grid_led, 150);

    // Add separator
    lv_obj_t *sep2 = lv_obj_create(status_container);
    lv_obj_set_size(sep2, LV_PCT(80), 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x404040), 0);

    // System 2 (Yellow) Section
    lv_obj_t *system2_title = lv_label_create(status_container);
    lv_label_set_text(system2_title, CONFIG_VICTRON_SYSTEM_NAME_2 " System:");
    lv_obj_set_style_text_font(system2_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(system2_title, lv_color_hex(0xFFFFFF), 0);


    // System 2 Grid status with LED
    lv_obj_t *system2_grid_container = lv_obj_create(status_container);
    lv_obj_set_size(system2_grid_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(system2_grid_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(system2_grid_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(system2_grid_container, 5, 0);
    lv_obj_set_style_bg_opa(system2_grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system2_grid_container, 0, 0);

    system2_grid_label = lv_label_create(system2_grid_container);
    lv_label_set_text(system2_grid_label, "  Grid:");
    lv_obj_set_style_text_font(system2_grid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(system2_grid_label, lv_color_hex(0xFFFFFF), 0);

    system2_grid_led = lv_led_create(system2_grid_container);
    lv_obj_set_size(system2_grid_led, 20, 20);
    lv_led_set_color(system2_grid_led, COLOR_UNKNOWN);
    lv_led_set_brightness(system2_grid_led, 150);

    // Add separator
    lv_obj_t *sep3 = lv_obj_create(status_container);
    lv_obj_set_size(sep3, LV_PCT(80), 1);
    lv_obj_set_style_bg_color(sep3, lv_color_hex(0x404040), 0);

    ESP_LOGI(TAG, "Multi-system status display UI created");
    return ESP_OK;
}

// Timer callback to restore System 1 MQTT LED brightness
static void system1_blink_timer_cb(lv_timer_t *timer)
{
    if (system1_mqtt_led) {
        lv_led_set_brightness(system1_mqtt_led, LED_BRIGHTNESS_NORMAL);
    }
    lv_timer_delete(timer);
    system1_blink_timer = NULL;
}

// Timer callback to restore System 2 MQTT LED brightness
static void system2_blink_timer_cb(lv_timer_t *timer)
{
    if (system2_mqtt_led) {
        lv_led_set_brightness(system2_mqtt_led, LED_BRIGHTNESS_NORMAL);
    }
    lv_timer_delete(timer);
    system2_blink_timer = NULL;
}

void ui_status_blink_mqtt_led(victron_system_id_t system_id)
{
    // Update UI in LVGL thread context
    if (lvgl_port_lock(0)) {
        if (system_id == VICTRON_SYSTEM_1) {
            if (system1_mqtt_led) {
                lv_led_set_brightness(system1_mqtt_led, LED_BRIGHTNESS_BLINK);
                // Cancel existing timer if any
                if (system1_blink_timer) {
                    lv_timer_delete(system1_blink_timer);
                }
                // Create timer to restore brightness
                system1_blink_timer = lv_timer_create(system1_blink_timer_cb, LED_BLINK_DURATION_MS, NULL);
                lv_timer_set_repeat_count(system1_blink_timer, 1);
            }
        } else if (system_id == VICTRON_SYSTEM_2) {
            if (system2_mqtt_led) {
                lv_led_set_brightness(system2_mqtt_led, LED_BRIGHTNESS_BLINK);
                // Cancel existing timer if any
                if (system2_blink_timer) {
                    lv_timer_delete(system2_blink_timer);
                }
                // Create timer to restore brightness
                system2_blink_timer = lv_timer_create(system2_blink_timer_cb, LED_BLINK_DURATION_MS, NULL);
                lv_timer_set_repeat_count(system2_blink_timer, 1);
            }
        }
        lvgl_port_unlock();
    }
}

// Track previous grid states for change detection
static bool s_prev_system1_connected = false;
static bool s_prev_system2_connected = false;
static bool s_grid_state_initialized = false;

void ui_status_update_grid(void)
{
    // Update both systems' grid status
    bool system1_connected = false;
    bool system2_connected = false;

    esp_err_t ret = victron_data_get_combined_status(&system1_connected, &system2_connected);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get combined grid status");
        return;
    }

    // Wake screen only on state transitions (not on first call)
    if (s_grid_state_initialized) {
        if (system1_connected != s_prev_system1_connected ||
            system2_connected != s_prev_system2_connected) {
            ESP_LOGI(TAG, "Grid status changed - waking screen");
            screen_timeout_wake();
        }
    }

    // Update tracked states
    s_prev_system1_connected = system1_connected;
    s_prev_system2_connected = system2_connected;
    s_grid_state_initialized = true;

    // Update UI in LVGL thread context
    if (lvgl_port_lock(0)) {
        // Update System 1 grid LED
        if (system1_grid_led) {
            lv_led_set_color(system1_grid_led, system1_connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
        }

        // Update System 2 grid LED
        if (system2_grid_led) {
            lv_led_set_color(system2_grid_led, system2_connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
        }

        lvgl_port_unlock();

        ESP_LOGI(TAG, "Updated grid status display - System1: %s, System2: %s",
                 system1_connected ? "Connected" : "Disconnected",
                 system2_connected ? "Connected" : "Disconnected");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock");
    }
}

void ui_status_update_wifi(bool connected)
{
    if (wifi_led == NULL) {
        ESP_LOGW(TAG, "WiFi LED not initialized");
        return;
    }

    // Update UI in LVGL thread context
    if (lvgl_port_lock(0)) {
        lv_led_set_color(wifi_led, connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
        lvgl_port_unlock();
        ESP_LOGI(TAG, "Updated WiFi status display: %s", connected ? "Connected" : "Disconnected");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock");
    }
}

void ui_status_update_mqtt(bool connected)
{
    // Update both systems' MQTT status
    bool system1_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_1);
    bool system2_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_2);

    // Update UI in LVGL thread context
    if (lvgl_port_lock(0)) {
        // Update System 1 MQTT LED
        if (system1_mqtt_led) {
            lv_led_set_color(system1_mqtt_led, system1_connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
        }

        // Update System 2 MQTT LED
        if (system2_mqtt_led) {
            lv_led_set_color(system2_mqtt_led, system2_connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);
        }

        lvgl_port_unlock();

        ESP_LOGI(TAG, "Updated MQTT status display - System1: %s, System2: %s",
                 system1_connected ? "Connected" : "Disconnected",
                 system2_connected ? "Connected" : "Disconnected");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock");
    }
}