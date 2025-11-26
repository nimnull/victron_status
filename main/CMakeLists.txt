idf_component_register(SRCS "hello_world_main.c"
                            "network_manager.c"
                            "victron_mqtt.c"
                            "victron_data.c"
                            "ui_status.c"
                       REQUIRES nvs_flash
                                esp_timer
                                esp_wifi
                                esp_netif
                                mqtt
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ".")
