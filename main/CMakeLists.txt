idf_component_register(SRCS "display.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES ssd1306 lvgl esp_timer nvs_flash esp_driver_uart)
