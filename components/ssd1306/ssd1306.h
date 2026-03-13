/*
 * ssd1306.h
 *
 * SSD1306 OLED driver public interface.
 *
 * Platform : ESP32-C3
 * Framework: ESP-IDF
 *
 * Author   : Daniel Fridman (schermaiolo)
 *
 * Description:
 * Public API for a minimal SSD1306 driver based on the ESP-IDF
 * I2C master driver. The driver maintains a monochrome framebuffer
 * in RAM and provides basic drawing and display update functions.
 *
 * Reference:
 * SSD1306 datasheet (Solomon Systech)
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_I2C_ADDR_DEFAULT 0x3C

typedef struct {
    i2c_port_num_t i2c_port;
    int sda_io;
    int scl_io;
    int rst_io;       // -1 if not connected
    uint8_t i2c_addr; // usually 0x3C
    uint16_t width;   // 128
    uint16_t height;  // 64 or 32
    uint8_t *buffer;  // width*height/8

    // New ESP-IDF I2C master API handles:
    // - bus_handle owns the I2C controller/bus instance
    // - dev_handle represents the SSD1306 device on that bus
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
} ssd1306_t;

/**
 * @brief Initialize the SSD1306 display driver
 *
 * Configures the I2C bus/device, allocates the framebuffer,
 * optionally toggles the reset pin, and sends the SSD1306
 * initialization sequence to the display controller.
 *
 * @param dev       Pointer to driver instance
 * @param port      I2C port number
 * @param sda_io    SDA GPIO number
 * @param scl_io    SCL GPIO number
 * @param rst_io    Reset GPIO number, or -1 if not connected
 * @param width     Display width in pixels
 * @param height    Display height in pixels
 * @param i2c_addr  I2C address of the display, usually 0x3C
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error code
 */
esp_err_t ssd1306_init(ssd1306_t *dev,
                       i2c_port_num_t port,
                       int sda_io, int scl_io,
                       int rst_io,
                       uint16_t width, uint16_t height,
                       uint8_t i2c_addr);

/**
 * @brief Release SSD1306 driver resources
 *
 * Frees the local framebuffer and releases the I2C device/bus
 * handles associated with the display.
 *
 * @param dev Pointer to driver instance
 */
void ssd1306_deinit(ssd1306_t *dev);

/**
 * @brief Clear the local framebuffer
 *
 * This function only clears the RAM buffer. To update the
 * physical display, call ssd1306_update().
 *
 * @param dev Pointer to driver instance
 */
void ssd1306_clear(ssd1306_t *dev);

/**
 * @brief Set or clear a pixel in the local framebuffer
 *
 * If the coordinates are outside the display bounds,
 * the function returns without modifying the buffer.
 *
 * @param dev Pointer to driver instance
 * @param x   X coordinate
 * @param y   Y coordinate
 * @param on  true to set the pixel, false to clear it
 */
void ssd1306_draw_pixel(ssd1306_t *dev, int x, int y, bool on);

/**
 * @brief Transfer the framebuffer to the OLED display
 *
 * Sends the current framebuffer contents to the SSD1306
 * over I2C using page-based addressing.
 *
 * @param dev Pointer to driver instance
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error code
 */
esp_err_t ssd1306_update(ssd1306_t *dev);

#ifdef __cplusplus
}
#endif