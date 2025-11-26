idf_component_register(SRCS "app_main.c"
                            "display_driver.c"
                            "touch_driver.c"
                            "lvgl_port.c"
                            "app_victron.c"
                            "network_manager.c"
                            "victron_mqtt.c"
                            "victron_data.c"
                            "ui_status.c"
                            "screen_timeout.c"
                       REQUIRES nvs_flash
                                esp_timer
                                esp_wifi
                                esp_netif
                                mqtt
                                esp_lcd
                       PRIV_REQUIRES spi_flash
                                     esp_driver_gpio
                                     esp_driver_i2c
                                     esp_driver_spi
                       INCLUDE_DIRS ".")
