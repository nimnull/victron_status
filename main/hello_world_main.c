/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esp_lcd_panel_ops.h"
#include <portmacro.h>

// touch contoller SDK support
#include "esp_lcd_touch.h"
#include "driver/i2c_master.h"

#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"

// Victron MQTT monitoring includes
#include "network_manager.h"
#include "victron_mqtt.h"
#include "victron_data.h"
#include "ui_status.h"

#define Rotate_90    0
#define Rotate_NONO  1

#define AMOLED_Rotate Rotate_90

#define LCD_HOST    SPI2_HOST
#define TOUCH_HOST  I2C_NUM_0

#define PIN_NUM_LCD_CS            (GPIO_NUM_9)
#define PIN_NUM_LCD_PCLK          (GPIO_NUM_10) 
#define PIN_NUM_LCD_DATA0         (GPIO_NUM_11)
#define PIN_NUM_LCD_DATA1         (GPIO_NUM_12)
#define PIN_NUM_LCD_DATA2         (GPIO_NUM_13)
#define PIN_NUM_LCD_DATA3         (GPIO_NUM_14)
#define PIN_NUM_LCD_RST           (GPIO_NUM_21)

#if (AMOLED_Rotate == Rotate_90)
#define LCD_H_RES              600
#define LCD_V_RES              450
#else
#define LCD_H_RES              450
#define LCD_V_RES              600
#endif

#define LVGL_BUF_HEIGHT        (LCD_V_RES/10)

#if CONFIG_LV_COLOR_DEPTH == 32
#define LCD_BIT_PER_PIXEL       (24)
#elif CONFIG_LV_COLOR_DEPTH == 24
#define LCD_BIT_PER_PIXEL       (24)
#elif CONFIG_LV_COLOR_DEPTH == 16
#define LCD_BIT_PER_PIXEL       (16)
#endif

#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2

#define EXAMPLE_USE_TOUCH              1 //Without tp ---- Touch off

#if EXAMPLE_USE_TOUCH
#define EXAMPLE_PIN_NUM_TOUCH_SCL         (GPIO_NUM_48)
#define EXAMPLE_PIN_NUM_TOUCH_SDA         (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_TOUCH_RST         (GPIO_NUM_3)
#define EXAMPLE_PIN_NUM_TOUCH_INT         (-1)

esp_lcd_touch_handle_t tp = NULL;
#endif


static const char *TAG = "example";
static SemaphoreHandle_t lvgl_mux = NULL;
static esp_timer_handle_t keepalive_timer = NULL;

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {

    {0xFE, (uint8_t []){0x20}, 1, 0},	
    {0x26, (uint8_t []){0x0A}, 1, 0}, 
    {0x24, (uint8_t []){0x80}, 1, 0}, 

    {0xFE, (uint8_t []){0x00}, 1, 0},  
#if LCD_BIT_PER_PIXEL == 24      
    {0x3A, (uint8_t []){0x77}, 1, 0}, //Interface Pixel Format	32bit/pixel         
#else
    {0x3A, (uint8_t []){0x55}, 1, 0}, //Interface Pixel Format	16bit/pixel     
#endif
    {0xC2, (uint8_t []){0x00}, 1, 10},  
    {0x35, (uint8_t []){0x00}, 0, 0}, //TE ON
    {0x51, (uint8_t []){0x00}, 1, 10}, //Write Display Brightness MAX_VAL=0XFF
    {0x11, (uint8_t []){0x00}, 0, 80},  
    {0x2A, (uint8_t []){0x00,0x10,0x01,0xD1}, 4, 0},
    {0x2B, (uint8_t []){0x00,0x00,0x02,0x57}, 4, 0},
    // {0x30, (uint8_t []){0x00, 0x01,0x02, 0x56}, 4, 0},   
    {0x29, (uint8_t []){0x00}, 0, 10},
#if (AMOLED_Rotate == Rotate_90)
    {0x36, (uint8_t []){0x30}, 1, 10},
#endif    
    {0x51, (uint8_t []){0x50}, 1, 0},//Write Display Brightness MAX_VAL=0XFF

};

static void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void keepalive_timer_callback(void *arg)
{
    ESP_LOGD(TAG, "Sending MQTT keepalive messages");
    esp_err_t ret = victron_mqtt_send_keepalive_all();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send some keepalive messages");
    }
}

// Made non-static to be accessible from ui_status.c
bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

// Made non-static to be accessible from ui_status.c
void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}


static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *display = (lv_display_t *)user_ctx;
    // lv_disp_flush_ready(display);
    return false;
}

void rounder_event_cb(lv_event_t * e)
{
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;
    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of area down to the nearest even number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;

    // round the end of area up to the nearest odd number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

static void lvgl_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t *px_map)
{
#if (AMOLED_Rotate == Rotate_90)
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1 + 16;
    const int offsety2 = area->y2 + 16;
#else
    const int offsetx1 = area->x1 + 16;
    const int offsetx2 = area->x2 + 16;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    
#endif
    

#if LCD_BIT_PER_PIXEL == 24
    lv_color_t * color_map = (lv_color_t *)px_map;
    uint8_t *to = (uint8_t *)px_map;
    // uint32_t pixel_num = (uint32_t)(area->x2 - area->x1 + 1) * (uint32_t)(area->y2 - area->y1 + 1);    
    uint16_t pixel_num = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);
    for (int i = 0; i < pixel_num; i++) {
        uint8_t red = color_map[i].red; 
        uint8_t blue = color_map[i].blue; 
        uint8_t green = color_map[i].green;
        *to++ = red;
        *to++ = green;
        *to++ = blue;
    }
#else
    lv_draw_sw_rgb565_swap(px_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1));
#endif
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(display);
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    lv_disp_flush_ready(display);
}

#if EXAMPLE_USE_TOUCH
static void example_lvgl_touch_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_driver_data(indev);
    assert(tp);

    uint16_t tp_x;
    uint16_t tp_y;
    uint8_t tp_cnt = 0;
    /* Read data from touch controller into memory */
    ESP_ERROR_CHECK(esp_lcd_touch_read_data(tp));
    /* Read data from touch controller */
    bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);
    if (tp_pressed && tp_cnt > 0) {
        data->point.x = tp_x ;
        data->point.y = tp_y ;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %d,%d", tp_x, tp_y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

void init_touch(void) {
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_config_t master_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT, // Use default clock source
        .i2c_port = I2C_NUM_0,             // Specify the I2C port
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,   // Select SCL GPIO
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,   // Select SDA GPIO
        .glitch_ignore_cnt = 7,            // Optional: configure glitch filter
        .flags.enable_internal_pullup = true, // Enable internal pullups if needed (external ones are recommended)
    };    
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&master_cfg, &bus_handle));

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_config.scl_speed_hz = 300 * 1000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &io_handle));

    // Configure touch panel
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_V_RES-1,
        .y_max = LCD_H_RES-1,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
#if (AMOLED_Rotate == Rotate_90)
            .swap_xy = 1,
            .mirror_x = 0,
            .mirror_y = 1,
#else
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
#endif
        },
    };

    ESP_LOGI(TAG, "Initialize touch controller");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(io_handle, &tp_cfg, &tp));

}

void app_main(void)
{
    lv_init();
    lv_display_t *display1 = lv_display_create(LCD_H_RES, LCD_V_RES);

    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(PIN_NUM_LCD_PCLK,
                                                                 PIN_NUM_LCD_DATA0,
                                                                 PIN_NUM_LCD_DATA1,
                                                                 PIN_NUM_LCD_DATA2,
                                                                 PIN_NUM_LCD_DATA3,
                                                                 LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
    
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    
    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(PIN_NUM_LCD_CS,
                                                                                notify_lvgl_flush_ready,
                                                                                &display1);
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Install SH8601 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
#if LCD_BIT_PER_PIXEL == 24
    lv_display_set_color_format(display1, LV_COLOR_FORMAT_RGB888);
#endif 
    lv_display_set_buffers(display1, buf1, buf2, LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Register display driver to LVGL");

    lv_display_set_user_data(display1, panel_handle);
    lv_display_set_flush_cb(display1, lvgl_flush_cb);
    lv_display_add_event_cb(display1, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

#if EXAMPLE_USE_TOUCH
    init_touch();

    lv_indev_t * indev = lv_indev_create(); 
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, example_lvgl_touch_cb);
    lv_indev_set_driver_data(indev, tp);
#endif


    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    // Initialize Victron data structures
    ESP_ERROR_CHECK(victron_data_init());

    // Create the Victron status UI instead of demo
    if (example_lvgl_lock(-1)) {
        // Clear the screen
        lv_obj_clean(lv_screen_active());

        // Create our status display
        ui_status_create(lv_screen_active());

        example_lvgl_unlock();
    }

    printf("Starting Victron MQTT Monitor...\n");

    // Initialize WiFi connection
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(network_init());

    ESP_LOGI(TAG, "WiFi connected successfully");
    ui_status_update_wifi(true);

    // Initialize and start MQTT clients for both systems
    ESP_LOGI(TAG, "Initializing MQTT clients for both systems...");
    ESP_ERROR_CHECK(victron_mqtt_init());

    ESP_ERROR_CHECK(victron_mqtt_start_all());
    ESP_LOGI(TAG, "MQTT clients started successfully for both systems");

    // Wait a bit for MQTT to connect
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Update MQTT status for both systems
    bool system1_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_1);
    bool system2_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_2);
    ui_status_update_mqtt(system1_connected || system2_connected);

    // Register callback to update UI when data is received
    victron_mqtt_register_data_callback((mqtt_data_callback_t)ui_status_update_grid);

    // Send initial keepalive immediately
    victron_mqtt_send_keepalive_all();

    // Create and start keepalive timer (15 seconds = 15000 ms)
    const esp_timer_create_args_t keepalive_timer_args = {
        .callback = &keepalive_timer_callback,
        .name = "mqtt_keepalive"
    };
    esp_err_t ret = esp_timer_create(&keepalive_timer_args, &keepalive_timer);
    if (ret == ESP_OK) {
        ret = esp_timer_start_periodic(keepalive_timer, 15000 * 1000); // 15 seconds in microseconds
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MQTT keepalive timer started (15s interval)");
        } else {
            ESP_LOGE(TAG, "Failed to start keepalive timer: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to create keepalive timer: %s", esp_err_to_name(ret));
    }


    printf("Victron MQTT Monitor initialized!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // Keep the system running instead of restarting
    while (1) {
        // Update connection statuses periodically
        ui_status_update_wifi(network_is_connected());

        // Update MQTT status for both systems
        bool system1_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_1);
        bool system2_connected = victron_mqtt_is_connected(VICTRON_SYSTEM_2);
        ui_status_update_mqtt(system1_connected || system2_connected);

        // Log heap size and connection status every 10 seconds for monitoring
        ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "System connections - Green: %s, Yellow: %s",
                 system1_connected ? "Connected" : "Disconnected",
                 system2_connected ? "Connected" : "Disconnected");

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

