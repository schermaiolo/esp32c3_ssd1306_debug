/*
 * main.c
 *
 * Main application entry point.
 *
 * Platform : ESP32-C3
 * Framework: ESP-IDF
 *
 * Author   : Daniel Fridman (schermaiolo)
 *
 * Description:
 * Application bootstrap file. It initializes NVS, UART,
 * and the display subsystem, then initializes 
 * the OLED debug display and periodically prints status messages to it.
 */

#include "display.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// ===== Pins & UART =====

#define UART1_RX          20
#define UART1_TX          21
#define UART1_BAUD        115200
#define UART1_PORT        UART_NUM_1

/**
 * @brief Initialize UART1 for application use
 *
 * Configures UART1 with the selected baud rate and installs
 * the ESP-IDF UART driver with RX/TX buffers.
 */
static void uart1_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = UART1_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    ESP_ERROR_CHECK(uart_param_config(UART1_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART1_PORT, UART1_TX, UART1_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // RX/TX buffers 4 KB each, no event queue
    ESP_ERROR_CHECK(uart_driver_install(UART1_PORT, 4096, 4096, 0, NULL, 0));
}

/**
 * @brief Application entry point
 *
 * Initializes NVS, display subsystem, and UART, then runs
 * a simple demonstration loop on the OLED display.
 */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    
    display_init();
    uart1_init();

    display_checkerboard();
    // Main loop
    int counter = 0;
    while (1) {
        display_clear();
        display_printf("Hello world");
        display_printf("Tick %d", counter);
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        ++counter;
    }
}
