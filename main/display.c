/*
 * display.c
 *
 * High-level display module implementation.
 *
 * Platform : ESP32-C3
 * Framework: ESP-IDF
 *
 * Author   : Daniel Fridman (schermaiolo)
 *
 * Description:
 * LVGL integration layer for the SSD1306 OLED display.
 * This module initializes the display driver, configures
 * LVGL, provides the flush callback, and implements simple
 * demo rendering functions used by the application.
 */

#include "display.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "ssd1306.h"         // components/ssd1306

#define DISPLAY_MAX_LINES      4
#define DISPLAY_MAX_LINE_LEN   32



// GLOBALS
static ssd1306_t oled;
static esp_timer_handle_t lvgl_tick_timer;
static lv_display_t *disp;
static char display_lines[DISPLAY_MAX_LINES][DISPLAY_MAX_LINE_LEN];
static uint8_t display_line_count = 0;

/**
 * @brief LVGL tick callback
 *
 * Called periodically by esp_timer to increment the
 * internal LVGL tick counter.
 *
 * @param arg Unused callback argument
 */
static void lv_tick_cb(void *arg) 
{
// ---------- LVGL tick (1 ms) ----------
    (void)arg;
    lv_tick_inc(1);
}

/**
 * @brief Redraw the entire text log on the screen.
 *
 * This function clears the active LVGL screen and recreates one label
 * for each stored text line.
 */
static void display_redraw_lines(void)
{
    lv_obj_clean(lv_screen_active());

    for (uint8_t i = 0; i < display_line_count; i++) {
        lv_obj_t *line = lv_label_create(lv_screen_active());
        lv_label_set_text(line, display_lines[i]);
        lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, i * 14);
    }

    lv_timer_handler();
}

// LVGL v9 flush for SSD1306 using 8-bit L8 buffer (1 byte per pixel)
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) 
{
    // Clamp to screen bounds
    int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t x2 = area->x2 >= OLED_W  ? (OLED_W  - 1) : area->x2;
    int32_t y2 = area->y2 >= OLED_H ? (OLED_H - 1) : area->y2;

    const int w = (x2 - x1 + 1);
    const int h = (y2 - y1 + 1);

    // px_map is tightly packed L8 (one byte per pixel), row-major for the area
    const uint8_t *row = px_map;

    for (int y = 0; y < h; y++) {
        const uint8_t *p = row;
        for (int x = 0; x < w; x++) {
            bool on = !(*p++ > 127);                    // threshold to mono
            ssd1306_draw_pixel(&oled, x1 + x, y1 + y, on);
        }
        row += w;
    }

    ssd1306_update(&oled);
    lv_display_flush_ready(disp);
}

/**
 * @brief Initialize the display subsystem
 *
 * Initializes the SSD1306 driver, configures LVGL,
 * creates the display object, and prepares the basic UI.
 */
void display_init(void)
{
    // --- Init SSD1306 (I2C @ 400 kHz) ---
    ESP_ERROR_CHECK(ssd1306_init(&oled, I2C_PORT, I2C_SDA, I2C_SCL, -1, OLED_W, OLED_H, OLED_ADDR));
    ssd1306_clear(&oled);
    ssd1306_update(&oled);

    // --- Init LVGL ---
    lv_init();

    // Create display and hook flush/buffers (LVGL v9 API)
    disp = lv_display_create(OLED_W, OLED_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // LVGL draw buffer in L8 format (1 byte per pixel)
    static uint8_t buf1[OLED_W * 8];  // 128 * 8 = 1024 bytes
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1),
                        LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 1 ms LVGL tick
    const esp_timer_create_args_t lv_tick_args = {
        .callback = &lv_tick_cb,
        .name = "lvgl_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&lv_tick_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1000)); // 1000 us = 1 ms

}

/* checkerboard */
void display_checkerboard(void)
{
    ssd1306_clear(&oled);
    for (int y = 0; y < OLED_H; ++y) {
        for (int x = 0; x < OLED_W; ++x) {
            bool on = ((x / 8) ^ (y / 8)) & 1;  // checkerboard
            ssd1306_draw_pixel(&oled, x, y, on);
        }
    }
    ssd1306_update(&oled);
    vTaskDelay(pdMS_TO_TICKS(3000));  // wait 3 seconds to observe
}

/**
 * @brief Clear the display and reset the internal text buffer.
 */
void display_clear(void)
{
    memset(display_lines, 0, sizeof(display_lines));
    display_line_count = 0;

    lv_obj_clean(lv_screen_active());
    ssd1306_clear(&oled);
    ssd1306_update(&oled);
    lv_timer_handler();
}

/**
 * @brief Print a formatted line to the OLED debug display.
 *
 * The formatted string is appended to the internal line buffer.
 * If the buffer is already full, existing lines are shifted up
 * and the oldest line is discarded.
 *
 * @param fmt printf-style format string
 * @param ... printf-style arguments
 */
void display_printf(const char *fmt, ...)
{
    char buf[DISPLAY_MAX_LINE_LEN];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (display_line_count < DISPLAY_MAX_LINES) {
        strncpy(display_lines[display_line_count], buf, DISPLAY_MAX_LINE_LEN - 1);
        display_lines[display_line_count][DISPLAY_MAX_LINE_LEN - 1] = '\0';
        display_line_count++;
    } else {
        for (uint8_t i = 1; i < DISPLAY_MAX_LINES; i++) {
            strncpy(display_lines[i - 1], display_lines[i], DISPLAY_MAX_LINE_LEN);
        }
        strncpy(display_lines[DISPLAY_MAX_LINES - 1], buf, DISPLAY_MAX_LINE_LEN - 1);
        display_lines[DISPLAY_MAX_LINES - 1][DISPLAY_MAX_LINE_LEN - 1] = '\0';
    }

    display_redraw_lines();
}