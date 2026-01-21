/**
 * @file tft35_module.h
 * @brief BIGTREETECH TFT35 V3.0.1 Display Module
 *
 * This module provides support for the TFT35 color touch screen display
 * using LVGL graphics library. It includes:
 * - ST7796/ILI9488 display driver with auto-detection
 * - XPT2046 touch controller driver
 * - LVGL integration with FreeRTOS
 */

#ifndef TFT35_MODULE_H
#define TFT35_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Display controller types (auto-detected)
typedef enum {
    TFT35_CONTROLLER_UNKNOWN = 0,
    TFT35_CONTROLLER_ST7796,
    TFT35_CONTROLLER_ILI9488,
} tft35_controller_t;

// Display rotation options
typedef enum {
    TFT35_ROTATION_0 = 0,
    TFT35_ROTATION_90 = 1,
    TFT35_ROTATION_180 = 2,
    TFT35_ROTATION_270 = 3,
} tft35_rotation_t;

// TFT35 module configuration (stored in EEPROM)
typedef struct {
    uint32_t data_rev;
    tft35_rotation_t rotation;
    uint8_t brightness;
    // Touch calibration
    int16_t touch_cal_x_min;
    int16_t touch_cal_x_max;
    int16_t touch_cal_y_min;
    int16_t touch_cal_y_max;
    bool touch_inverted_x;
    bool touch_inverted_y;
} tft35_module_config_t;

// Module initialization
void tft35_module_init(void);

// Get detected controller type
tft35_controller_t tft35_get_controller_type(void);

// Display control
void tft35_set_rotation(tft35_rotation_t rotation);
void tft35_set_brightness(uint8_t brightness);

// Touch calibration
void tft35_touch_calibrate(void);
bool tft35_touch_is_calibrated(void);

// Configuration
tft35_module_config_t* tft35_get_config(void);
void tft35_save_config(void);

// LVGL task - call from main after init
void tft35_lvgl_task(void *p);

#ifdef __cplusplus
}
#endif

#endif // TFT35_MODULE_H
