/*
 * Screen Timeout Module
 * Turns off display after inactivity and wakes on touch
 */

#include "screen_timeout.h"
#include "esp_log.h"

static const char *TAG = "screen_timeout";

typedef struct {
    bool screen_on;
    bool wake_touch_consumed;
    lv_display_t *display;
    esp_lcd_panel_handle_t panel;
} screen_timeout_state_t;

static screen_timeout_state_t s_state = {
    .screen_on = true,
    .wake_touch_consumed = false,
    .display = NULL,
    .panel = NULL,
};

esp_err_t screen_timeout_init(lv_display_t *display, esp_lcd_panel_handle_t panel)
{
    if (!display || !panel) {
        return ESP_ERR_INVALID_ARG;
    }

    s_state.display = display;
    s_state.panel = panel;
    s_state.screen_on = true;
    s_state.wake_touch_consumed = false;

    ESP_LOGI(TAG, "Screen timeout initialized (timeout: %d ms)", SCREEN_TIMEOUT_MS);
    return ESP_OK;
}

void screen_timeout_check(void)
{
    if (!s_state.display || !s_state.panel) {
        return;
    }

    if (s_state.screen_on) {
        uint32_t inactive_time = lv_display_get_inactive_time(s_state.display);

        if (inactive_time >= SCREEN_TIMEOUT_MS) {
            ESP_LOGI(TAG, "Screen timeout - turning off (inactive: %lu ms)", (unsigned long)inactive_time);
            esp_lcd_panel_disp_on_off(s_state.panel, false);
            s_state.screen_on = false;
            s_state.wake_touch_consumed = false;
        }
    }
}

bool screen_timeout_process_touch(bool pressed)
{
    // If screen is on, pass all touches through
    if (s_state.screen_on) {
        return true;
    }

    // Screen is off - handle wake logic
    if (pressed) {
        if (!s_state.wake_touch_consumed) {
            ESP_LOGI(TAG, "Wake touch detected - turning screen on");
            esp_lcd_panel_disp_on_off(s_state.panel, true);
            s_state.screen_on = true;
            s_state.wake_touch_consumed = true;

            // Reset LVGL inactivity timer
            lv_display_trigger_activity(s_state.display);

            return false;  // Consume this touch - don't pass to UI
        }
    } else {
        // Touch released while screen is on - allow next press through
        if (s_state.screen_on) {
            s_state.wake_touch_consumed = false;
        }
    }

    return false;  // Screen still off or consuming wake touch
}

bool screen_timeout_is_on(void)
{
    return s_state.screen_on;
}

void screen_timeout_wake(void)
{
    if (!s_state.screen_on && s_state.panel) {
        ESP_LOGI(TAG, "Manual wake - turning screen on");
        esp_lcd_panel_disp_on_off(s_state.panel, true);
        s_state.screen_on = true;
        s_state.wake_touch_consumed = false;

        if (s_state.display) {
            lv_display_trigger_activity(s_state.display);
        }
    }
}
