/*
 * ssd1306.c
 *
 * SSD1306 OLED driver implementation.
 *
 * Platform : ESP32-C3
 * Framework: ESP-IDF
 *
 * Author   : Daniel Fridman (schermaiolo)
 *
 * Description:
 * Minimal SSD1306 driver using the ESP-IDF I2C master API.
 * The driver allocates a framebuffer in RAM, sends the SSD1306
 * initialization sequence, and updates the display through I2C.
 *
 * Notes:
 * - Command values and initialization sequence are based on the
 *   SSD1306 controller documentation.
 * - The display is updated by transferring the local framebuffer
 *   to the panel.
 *
 * Reference:
 * SSD1306 datasheet (Solomon Systech)
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"

#define TAG "ssd1306"

// I2C
#define I2C_FREQ_HZ    100000
#define I2C_TIMEOUT_MS 50

// SSD1306 control bytes
#define SSD1306_CTRL_CMD  0x00
#define SSD1306_CTRL_DATA 0x40

/**
 * @brief Send a command or data block to the SSD1306 over I2C
 *
 * The transmitted frame is composed as:
 * [control byte][payload bytes...]
 *
 * @param dev   Pointer to driver instance
 * @param ctrl  SSD1306 control byte (command or data)
 * @param data  Pointer to payload buffer
 * @param len   Payload length in bytes
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error code
 */
static esp_err_t i2c_write(ssd1306_t *dev, uint8_t ctrl, const uint8_t *data, size_t len) 
{
    if (!dev || !dev->dev_handle) return ESP_ERR_INVALID_STATE;
    if (len > 0 && data == NULL) return ESP_ERR_INVALID_ARG;

    // New API:
    // build one TX buffer = [control byte][payload...]
    // then send it in a single i2c_master_transmit() call.
    uint8_t *tx = (uint8_t *)malloc(len + 1);
    if (!tx) return ESP_ERR_NO_MEM;

    tx[0] = ctrl;
    if (len > 0) {
        memcpy(&tx[1], data, len);
    }

    esp_err_t err = i2c_master_transmit(dev->dev_handle, tx, len + 1, I2C_TIMEOUT_MS);
    free(tx);
    return err;
}

/** @brief Send a single SSD1306 command byte. */
static inline esp_err_t cmd1(ssd1306_t *dev, uint8_t c) 
{
    return i2c_write(dev, SSD1306_CTRL_CMD, &c, 1);
}

/** @brief Send multiple SSD1306 command bytes. */
static inline esp_err_t cmdn(ssd1306_t *dev, const uint8_t *d, size_t n) 
{
    return i2c_write(dev, SSD1306_CTRL_CMD, d, n);
}

esp_err_t ssd1306_init(ssd1306_t *dev,
                       i2c_port_num_t port,
                       int sda_io, int scl_io,
                       int rst_io,
                       uint16_t width, uint16_t height,
                       uint8_t i2c_addr) 
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (width == 0 || height == 0) return ESP_ERR_INVALID_ARG;

    memset(dev, 0, sizeof(*dev));
    dev->i2c_port = port;
    dev->sda_io = sda_io;
    dev->scl_io = scl_io;
    dev->rst_io = rst_io;
    dev->i2c_addr = i2c_addr ? i2c_addr : SSD1306_I2C_ADDR_DEFAULT;
    dev->width = width;
    dev->height = height;

    size_t bufsize = (width * height) / 8;
    dev->buffer = (uint8_t *)calloc(1, bufsize);
    if (!dev->buffer) return ESP_ERR_NO_MEM;

    // I2C config + driver
    // New API uses a bus/device model:
    // 1) create the master bus
    // 2) attach the SSD1306 as a device on that bus
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &dev->bus_handle);
    if (err != ESP_OK) {
        free(dev->buffer);
        dev->buffer = NULL;
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev->i2c_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(dev->bus_handle, &dev_cfg, &dev->dev_handle);
    if (err != ESP_OK) 
    {
        i2c_del_master_bus(dev->bus_handle);
        dev->bus_handle = NULL;
        free(dev->buffer);
        dev->buffer = NULL;
        return err;
    }

    // Optional hardware reset
    if (rst_io >= 0) 
    {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << rst_io,
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io);
        gpio_set_level(rst_io, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(rst_io, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Init sequence (datasheet + common settings)
    const uint8_t init_seq[] = {
        0xAE,             // Display OFF
        0xD5, 0x80,       // Set display clock div/osc freq
        0xA8, 0x3F,       // Set multiplex ratio 0x3F = 63 (for 64 rows)
        0xD3, 0x00,       // Display offset = 0
        0x40,             // Start line = 0
        0x8D, 0x14,       // Charge pump ON (internal VCC)
        0x20, 0x00,       // Memory mode = Horizontal
        0xA1,             // Segment remap (mirror X) — try 0xA0 if mirrored
        0xC8,             // COM scan direction remap (Y flip) — try 0xC0 if upside-down
        0xDA, 0x12,       // COM pins config for 128×64
        0x81, 0xCF,       // Contrast
        0xD9, 0xF1,       // Precharge (internal VCC)
        0xDB, 0x40,       // VCOMH level
        0xA4,             // Display follows RAM (not all pixels on)
        0xA6,             // Normal (not inverted)
        0x21, 0x00, 0x7F, // Column address range: 0..127
        0x22, 0x00, 0x07, // Page address range:   0..7
        0xAF              // Display ON
    };

    err = cmdn(dev, init_seq, sizeof(init_seq));
    if (err != ESP_OK) 
    {
        ssd1306_deinit(dev);
        return err;
    }

    ESP_LOGI(TAG, "SSD1306 init OK: %ux%u @ 0x%02X", dev->width, dev->height, dev->i2c_addr);
    ssd1306_clear(dev);
    return ESP_OK;
}

void ssd1306_deinit(ssd1306_t *dev) 
{
    if (!dev) return;

    if (dev->buffer) {
        free(dev->buffer);
        dev->buffer = NULL;
    }

    // Remove device first, then delete the bus.
    if (dev->dev_handle) {
        i2c_master_bus_rm_device(dev->dev_handle);
        dev->dev_handle = NULL;
    }

    if (dev->bus_handle) {
        i2c_del_master_bus(dev->bus_handle);
        dev->bus_handle = NULL;
    }
}

void ssd1306_clear(ssd1306_t *dev) 
{
    if (!dev || !dev->buffer) return;
    memset(dev->buffer, 0x00, (dev->width * dev->height) / 8);
}

void ssd1306_draw_pixel(ssd1306_t *dev, int x, int y, bool on) 
{
    if (!dev || !dev->buffer) return;
    if (x < 0 || y < 0 || x >= dev->width || y >= dev->height) return;

    size_t index = x + (y / 8) * dev->width;
    uint8_t bit = 1U << (y & 7);

    if (on) dev->buffer[index] |= bit;
    else    dev->buffer[index] &= (uint8_t)~bit;
}

esp_err_t ssd1306_update(ssd1306_t *dev) 
{
    if (!dev || !dev->buffer) return ESP_ERR_INVALID_ARG;

    // Update full screen (simple, fine for 128x64)
    uint8_t col_pages[] = {
        0x21, 0x00, (uint8_t)(dev->width - 1),
        0x22, 0x00, (uint8_t)((dev->height / 8) - 1)
    };

    esp_err_t err = cmdn(dev, col_pages, sizeof(col_pages));
    if (err != ESP_OK) return err;

    size_t total = (dev->width * dev->height) / 8;
    size_t off = 0;
    while (off < total) 
    {
        size_t chunk = total - off;
        if (chunk > 128) chunk = 128; // conservative chunk size

        err = i2c_write(dev, SSD1306_CTRL_DATA, dev->buffer + off, chunk);
        if (err != ESP_OK) return err;

        off += chunk;
    }

    return ESP_OK;
}