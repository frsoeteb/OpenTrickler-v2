/**
 * @file tft35_touch.h
 * @brief XPT2046 Touch Controller Driver
 *
 * Driver for the XPT2046 resistive touch controller used on TFT35 displays.
 * Supports touch calibration and coordinate transformation.
 */

#ifndef TFT35_TOUCH_H
#define TFT35_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Touch state
typedef struct {
    int16_t x;
    int16_t y;
    bool pressed;
} tft35_touch_point_t;

// Initialize the touch controller
void tft35_touch_init(void);

// Read current touch state (returns calibrated coordinates)
bool tft35_touch_read(tft35_touch_point_t *point);

// Read raw touch values (for calibration)
bool tft35_touch_read_raw(int16_t *raw_x, int16_t *raw_y);

// Set calibration values
void tft35_touch_set_calibration(int16_t x_min, int16_t x_max,
                                  int16_t y_min, int16_t y_max,
                                  bool invert_x, bool invert_y);

// Get current calibration values
void tft35_touch_get_calibration(int16_t *x_min, int16_t *x_max,
                                  int16_t *y_min, int16_t *y_max,
                                  bool *invert_x, bool *invert_y);

// Check if touch IRQ pin is active (touch detected)
bool tft35_touch_irq_active(void);

// LVGL input device read callback
void tft35_touch_read_cb(void *indev, void *data);

#ifdef __cplusplus
}
#endif

#endif // TFT35_TOUCH_H
