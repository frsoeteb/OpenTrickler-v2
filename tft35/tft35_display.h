/**
 * @file tft35_display.h
 * @brief TFT35 Display Driver for ST7796/ILI9488
 *
 * Supports both ST7796 and ILI9488 controllers with runtime auto-detection.
 * Uses SPI interface with DMA for efficient framebuffer transfers.
 */

#ifndef TFT35_DISPLAY_H
#define TFT35_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "tft35_module.h"

// Display dimensions (from config - supports TFT35 480x320 and TFT43 480x272)
#include "configuration.h"
#ifdef TFT_WIDTH
    #define TFT35_DISPLAY_WIDTH     TFT_WIDTH
    #define TFT35_DISPLAY_HEIGHT    TFT_HEIGHT
#else
    // Defaults for TFT35
    #define TFT35_DISPLAY_WIDTH     480
    #define TFT35_DISPLAY_HEIGHT    320
#endif

// Color format (RGB565)
#define TFT35_COLOR_DEPTH       16

// Initialize the display hardware and detect controller
void tft35_display_init(void);

// Get the detected controller type
tft35_controller_t tft35_display_get_controller(void);

// Set display rotation (0, 90, 180, 270 degrees)
void tft35_display_set_rotation(tft35_rotation_t rotation);

// Set display brightness (0-255, if hardware supports)
void tft35_display_set_brightness(uint8_t brightness);

// Set the drawing window
void tft35_display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// Write pixel data to the current window
void tft35_display_write_pixels(const uint16_t *data, uint32_t len);

// LVGL flush callback - registered with LVGL display driver
void tft35_display_flush_cb(void *disp, const void *area, uint8_t *color_map);

// Signal that flush is complete (called after DMA transfer)
void tft35_display_flush_ready(void);

#ifdef __cplusplus
}
#endif

#endif // TFT35_DISPLAY_H
