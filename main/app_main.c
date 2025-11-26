/*
 * Victron MQTT Monitor - Main Entry Point
 * ESP32-S3 firmware for monitoring Victron installations via MQTT
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "display_driver.h"
#include "touch_driver.h"
#include "lvgl_port.h"
#include "screen_timeout.h"
#include "app_victron.h"

static const char *TAG = "main";

static void print_chip_info(void)
{
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

    printf("silicon revision v%d.%d, ", chip_info.revision / 100, chip_info.revision % 100);

    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        printf("%" PRIu32 "MB %s flash\n", flash_size / (1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

void app_main(void)
{
    /* Initialize display hardware and LVGL */
    ESP_ERROR_CHECK(display_driver_init());

    /* Initialize LVGL port (tick timer and mutex) */
    ESP_ERROR_CHECK(lvgl_port_init());

    /* Initialize touch controller */
    ESP_ERROR_CHECK(touch_driver_init());

    /* Register touch with LVGL */
    touch_driver_register_lvgl_indev();

    /* Initialize screen timeout */
    ESP_ERROR_CHECK(screen_timeout_init(
        display_driver_get_lvgl_display(),
        display_driver_get_panel()
    ));

    /* Start LVGL task */
    lvgl_port_start_task();

    /* Initialize and run Victron application */
    ESP_ERROR_CHECK(app_victron_init());

    /* Print chip diagnostics */
    print_chip_info();

    /* Run main application loop (does not return) */
    app_victron_run();
}
