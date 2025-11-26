/*
 * Touch Driver - FT6336U Capacitive Touch via I2C
 * Handles touch hardware initialization and LVGL input device
 */

#include "touch_driver.h"
#include "display_driver.h"
#include "screen_timeout.h"

#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_ft5x06.h"

static const char *TAG = "touch_driver";

/* Hardware configuration */
#define PIN_NUM_TOUCH_SCL   GPIO_NUM_48
#define PIN_NUM_TOUCH_SDA   GPIO_NUM_47
#define PIN_NUM_TOUCH_RST   GPIO_NUM_3
#define PIN_NUM_TOUCH_INT   (-1)

/* Module state */
static esp_lcd_touch_handle_t s_touch_handle = NULL;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_driver_data(indev);
    assert(tp);

    uint16_t tp_x;
    uint16_t tp_y;
    uint8_t tp_cnt = 0;

    /* Read data from touch controller */
    ESP_ERROR_CHECK(esp_lcd_touch_read_data(tp));
    bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);
    bool is_pressed = tp_pressed && tp_cnt > 0;

    /* Process through screen timeout - may consume touch for wake */
    bool pass_to_ui = screen_timeout_process_touch(is_pressed);

    if (is_pressed && pass_to_ui) {
        data->point.x = tp_x;
        data->point.y = tp_y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %d,%d", tp_x, tp_y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

esp_err_t touch_driver_init(void)
{
    esp_err_t ret;

    /* Initialize I2C bus */
    ESP_LOGI(TAG, "Initializing I2C bus");
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_NUM_TOUCH_SCL,
        .sda_io_num = PIN_NUM_TOUCH_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create touch panel IO */
    ESP_LOGI(TAG, "Initializing touch panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_config.scl_speed_hz = 300 * 1000;

    ret = esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create touch panel IO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure touch controller for 90-degree rotation */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_V_RES - 1,
        .y_max = DISPLAY_H_RES - 1,
        .rst_gpio_num = PIN_NUM_TOUCH_RST,
        .int_gpio_num = PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };

    /* Initialize FT5x06 touch controller */
    ESP_LOGI(TAG, "Initializing FT6336U touch controller");
    ret = esp_lcd_touch_new_i2c_ft5x06(io_handle, &tp_cfg, &s_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize touch controller: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Touch driver initialized");
    return ESP_OK;
}

void touch_driver_register_lvgl_indev(void)
{
    assert(s_touch_handle && "touch_driver_init must be called first");

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_driver_data(indev, s_touch_handle);

    ESP_LOGI(TAG, "LVGL input device registered");
}

esp_lcd_touch_handle_t touch_driver_get_handle(void)
{
    return s_touch_handle;
}
