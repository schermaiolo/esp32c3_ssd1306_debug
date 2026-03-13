/*
 * display.h
 *
 * High-level display module interface.
 *
 * Platform : ESP32-C3
 * Framework: ESP-IDF
 *
 * Author   : Daniel Fridman (schermaiolo)
 *
 * Description:
 * Public interface for the application display layer.
 * This module initializes LVGL, connects LVGL to the SSD1306
 * driver, and This module initializes LVGL, connects LVGL to the SSD1306 driver, 
 * and exposes helper functions for text output and simple test rendering
 *  on the OLED display.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @brief Initialize the display subsystem
 *
 * Initializes the SSD1306 driver, configures LVGL,
 * creates the display object, and prepares the basic UI.
 */
void display_init(void);

/**
 * @brief Render a checkerboard test pattern
 *
 * Draws a simple checkerboard directly into the OLED
 * framebuffer and updates the display.
 */
void display_checkerboard(void);

/**
 * @brief Clear the display and reset the internal text buffer.
 */
void display_clear(void);

/**
 * @brief Print a formatted line to the OLED debug display.
 *
 * The text is appended as a new line in the internal display log.
 * If the log is full, the oldest line is discarded.
 *
 * @param fmt printf-style format string
 * @param ... printf-style arguments
 */
void display_printf(const char *fmt, ...);

// ---------- Display config ----------
#define OLED_W     128
#define OLED_H      64
#define OLED_ADDR  0x3C

#define I2C_PORT   I2C_NUM_0
#define I2C_SDA    5
#define I2C_SCL    6

#endif //DISPLAY_H