/*
 * Display Driver - SH8601 AMOLED via QSPI
 * Handles display hardware initialization and LVGL integration
 */

#include "display_driver.h"
#include "sdkconfig.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "lvgl.h"

static const char *TAG = "display_driver";

/* Hardware configuration */
#define LCD_HOST            SPI2_HOST
#define PIN_NUM_LCD_CS      GPIO_NUM_9
#define PIN_NUM_LCD_PCLK    GPIO_NUM_10
#define PIN_NUM_LCD_DATA0   GPIO_NUM_11
#define PIN_NUM_LCD_DATA1   GPIO_NUM_12
#define PIN_NUM_LCD_DATA2   GPIO_NUM_13
#define PIN_NUM_LCD_DATA3   GPIO_NUM_14
#define PIN_NUM_LCD_RST     GPIO_NUM_21

/* Color depth configuration */
#if CONFIG_LV_COLOR_DEPTH == 32 || CONFIG_LV_COLOR_DEPTH == 24
#define LCD_BIT_PER_PIXEL   24
#else
#define LCD_BIT_PER_PIXEL   16
#endif

#define LVGL_BUF_HEIGHT     (DISPLAY_V_RES / 10)

/* Module state */
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static lv_display_t *s_display = NULL;

/* SH8601 initialization commands */
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x20}, 1, 0},
    {0x26, (uint8_t []){0x0A}, 1, 0},
    {0x24, (uint8_t []){0x80}, 1, 0},
    {0xFE, (uint8_t []){0x00}, 1, 0},
#if LCD_BIT_PER_PIXEL == 24
    {0x3A, (uint8_t []){0x77}, 1, 0},  /* 24-bit/pixel */
#else
    {0x3A, (uint8_t []){0x55}, 1, 0},  /* 16-bit/pixel */
#endif
    {0xC2, (uint8_t []){0x00}, 1, 10},
    {0x35, (uint8_t []){0x00}, 0, 0},  /* TE ON */
    {0x51, (uint8_t []){0x00}, 1, 10}, /* Brightness: 0 during init */
    {0x11, (uint8_t []){0x00}, 0, 80}, /* Sleep out */
    {0x2A, (uint8_t []){0x00, 0x10, 0x01, 0xD1}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x02, 0x57}, 4, 0},
    {0x29, (uint8_t []){0x00}, 0, 10}, /* Display on */
    {0x36, (uint8_t []){0x30}, 1, 10}, /* Rotation 90 degrees */
    {0x51, (uint8_t []){0x50}, 1, 0},  /* Brightness: 31% */
};

static void rounder_event_cb(lv_event_t *e)
{
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

    /* Round to even pixel boundaries for efficient DMA transfers */
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    /* Apply offset for 90-degree rotation */
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1 + 16;
    const int offsety2 = area->y2 + 16;

#if LCD_BIT_PER_PIXEL == 24
    /* Convert LVGL RGB888 to display format */
    lv_color_t *color_map = (lv_color_t *)px_map;
    uint8_t *to = (uint8_t *)px_map;
    uint16_t pixel_num = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);

    for (int i = 0; i < pixel_num; i++) {
        *to++ = color_map[i].red;
        *to++ = color_map[i].green;
        *to++ = color_map[i].blue;
    }
#else
    lv_draw_sw_rgb565_swap(px_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1));
#endif

    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(display);
    esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    lv_disp_flush_ready(display);
}

esp_err_t display_driver_init(void)
{
    esp_err_t ret;

    /* Initialize LVGL library */
    lv_init();

    /* Create LVGL display */
    s_display = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
    if (!s_display) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_ERR_NO_MEM;
    }

    /* Configure QSPI bus */
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        PIN_NUM_LCD_PCLK,
        PIN_NUM_LCD_DATA0,
        PIN_NUM_LCD_DATA1,
        PIN_NUM_LCD_DATA2,
        PIN_NUM_LCD_DATA3,
        DISPLAY_H_RES * DISPLAY_V_RES * LCD_BIT_PER_PIXEL / 8
    );

    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create panel IO */
    ESP_LOGI(TAG, "Installing panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;

    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        PIN_NUM_LCD_CS,
        NULL,  /* No flush ready callback */
        NULL
    );

    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create panel driver */
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Installing SH8601 panel driver");
    ret = esp_lcd_new_panel_sh8601(io_handle, &panel_config, &s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    /* Allocate LVGL draw buffers in PSRAM */
    lv_color_t *buf1 = heap_caps_malloc(DISPLAY_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = heap_caps_malloc(DISPLAY_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return ESP_ERR_NO_MEM;
    }

    /* Configure LVGL display */
#if LCD_BIT_PER_PIXEL == 24
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB888);
#endif
    lv_display_set_buffers(s_display, buf1, buf2,
                           DISPLAY_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(s_display, s_panel_handle);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);
    lv_display_add_event_cb(s_display, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    ESP_LOGI(TAG, "Display initialized (%dx%d, %d bpp)", DISPLAY_H_RES, DISPLAY_V_RES, LCD_BIT_PER_PIXEL);
    return ESP_OK;
}

esp_lcd_panel_handle_t display_driver_get_panel(void)
{
    return s_panel_handle;
}

lv_display_t *display_driver_get_lvgl_display(void)
{
    return s_display;
}
