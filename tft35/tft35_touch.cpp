/**
 * @file tft35_touch.cpp
 * @brief XPT2046 Touch Controller Driver Implementation
 */

#include "tft35_touch.h"
#include "configuration.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "semphr.h"

// Touch controller pins (from config)
#ifndef TFT35_TOUCH_CS_PIN
#define TFT35_TOUCH_CS_PIN      24
#define TFT35_TOUCH_IRQ_PIN     25
#endif

#ifndef TFT35_SPI
#define TFT35_SPI               spi0
#endif

// XPT2046 SPI speed (max 2.5MHz)
#define TOUCH_SPI_FREQ_HZ       (2 * 1000 * 1000)

// XPT2046 commands
#define XPT2046_CMD_X           0xD0    // X position
#define XPT2046_CMD_Y           0x90    // Y position
#define XPT2046_CMD_Z1          0xB0    // Z1 pressure
#define XPT2046_CMD_Z2          0xC0    // Z2 pressure

// Calibration defaults (raw ADC values)
#define CAL_X_MIN_DEFAULT       300
#define CAL_X_MAX_DEFAULT       3800
#define CAL_Y_MIN_DEFAULT       300
#define CAL_Y_MAX_DEFAULT       3800

// Pressure threshold
#define TOUCH_PRESSURE_MIN      50
#define TOUCH_PRESSURE_MAX      3000

// Display dimensions (from display driver - supports TFT35 and TFT43)
#include "tft35_display.h"
#define DISPLAY_WIDTH           TFT35_DISPLAY_WIDTH
#define DISPLAY_HEIGHT          TFT35_DISPLAY_HEIGHT

// Static variables
static int16_t cal_x_min = CAL_X_MIN_DEFAULT;
static int16_t cal_x_max = CAL_X_MAX_DEFAULT;
static int16_t cal_y_min = CAL_Y_MIN_DEFAULT;
static int16_t cal_y_max = CAL_Y_MAX_DEFAULT;
static bool cal_invert_x = false;
static bool cal_invert_y = false;
static SemaphoreHandle_t touch_mutex = NULL;
static uint32_t original_spi_freq = 0;

// Save and restore SPI frequency for touch (slower than display)
static void touch_spi_begin(void) {
    original_spi_freq = spi_get_baudrate(TFT35_SPI);
    spi_set_baudrate(TFT35_SPI, TOUCH_SPI_FREQ_HZ);
    gpio_put(TFT35_TOUCH_CS_PIN, 0);
}

static void touch_spi_end(void) {
    gpio_put(TFT35_TOUCH_CS_PIN, 1);
    spi_set_baudrate(TFT35_SPI, original_spi_freq);
}

// Read a channel from XPT2046
static uint16_t read_channel(uint8_t cmd) {
    uint8_t tx[3] = {cmd, 0, 0};
    uint8_t rx[3] = {0};

    spi_write_read_blocking(TFT35_SPI, tx, rx, 3);

    // XPT2046 returns 12-bit value in bits 11:0 of response
    return ((rx[1] << 8) | rx[2]) >> 3;
}

// Initialize touch controller
void tft35_touch_init(void) {
    touch_mutex = xSemaphoreCreateMutex();

    // Touch CS pin
    gpio_init(TFT35_TOUCH_CS_PIN);
    gpio_set_dir(TFT35_TOUCH_CS_PIN, GPIO_OUT);
    gpio_put(TFT35_TOUCH_CS_PIN, 1);  // Deselect

    // Touch IRQ pin (active low when touched)
    gpio_init(TFT35_TOUCH_IRQ_PIN);
    gpio_set_dir(TFT35_TOUCH_IRQ_PIN, GPIO_IN);
    gpio_pull_up(TFT35_TOUCH_IRQ_PIN);

    // Perform a dummy read to initialize
    touch_spi_begin();
    read_channel(XPT2046_CMD_X);
    touch_spi_end();
}

// Check if touch IRQ is active
bool tft35_touch_irq_active(void) {
    return !gpio_get(TFT35_TOUCH_IRQ_PIN);  // Active low
}

// Read raw touch values
bool tft35_touch_read_raw(int16_t *raw_x, int16_t *raw_y) {
    if (!tft35_touch_irq_active()) {
        return false;
    }

    if (xSemaphoreTake(touch_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }

    touch_spi_begin();

    // Read Z (pressure) first
    uint16_t z1 = read_channel(XPT2046_CMD_Z1);
    uint16_t z2 = read_channel(XPT2046_CMD_Z2);

    // Calculate pressure (simplified formula)
    int32_t pressure = z1;
    if (z2 > z1) {
        pressure = 4095 - (z2 - z1);
    }

    bool touched = (pressure > TOUCH_PRESSURE_MIN && pressure < TOUCH_PRESSURE_MAX);

    if (touched) {
        // Average multiple readings for stability
        int32_t sum_x = 0, sum_y = 0;
        const int samples = 4;

        for (int i = 0; i < samples; i++) {
            sum_x += read_channel(XPT2046_CMD_X);
            sum_y += read_channel(XPT2046_CMD_Y);
        }

        *raw_x = sum_x / samples;
        *raw_y = sum_y / samples;
    }

    touch_spi_end();
    xSemaphoreGive(touch_mutex);

    return touched;
}

// Set calibration values
void tft35_touch_set_calibration(int16_t x_min, int16_t x_max,
                                  int16_t y_min, int16_t y_max,
                                  bool invert_x, bool invert_y) {
    cal_x_min = x_min;
    cal_x_max = x_max;
    cal_y_min = y_min;
    cal_y_max = y_max;
    cal_invert_x = invert_x;
    cal_invert_y = invert_y;
}

// Get calibration values
void tft35_touch_get_calibration(int16_t *x_min, int16_t *x_max,
                                  int16_t *y_min, int16_t *y_max,
                                  bool *invert_x, bool *invert_y) {
    if (x_min) *x_min = cal_x_min;
    if (x_max) *x_max = cal_x_max;
    if (y_min) *y_min = cal_y_min;
    if (y_max) *y_max = cal_y_max;
    if (invert_x) *invert_x = cal_invert_x;
    if (invert_y) *invert_y = cal_invert_y;
}

// Read calibrated touch coordinates
bool tft35_touch_read(tft35_touch_point_t *point) {
    int16_t raw_x, raw_y;

    if (!tft35_touch_read_raw(&raw_x, &raw_y)) {
        point->pressed = false;
        return false;
    }

    // Apply calibration
    int32_t x = raw_x;
    int32_t y = raw_y;

    // Map raw to screen coordinates
    x = (x - cal_x_min) * DISPLAY_WIDTH / (cal_x_max - cal_x_min);
    y = (y - cal_y_min) * DISPLAY_HEIGHT / (cal_y_max - cal_y_min);

    // Apply inversion if needed
    if (cal_invert_x) {
        x = DISPLAY_WIDTH - 1 - x;
    }
    if (cal_invert_y) {
        y = DISPLAY_HEIGHT - 1 - y;
    }

    // Clamp to screen bounds
    if (x < 0) x = 0;
    if (x >= DISPLAY_WIDTH) x = DISPLAY_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= DISPLAY_HEIGHT) y = DISPLAY_HEIGHT - 1;

    point->x = x;
    point->y = y;
    point->pressed = true;

    return true;
}

// LVGL input device read callback
void tft35_touch_read_cb(void *indev, void *data) {
    lv_indev_t *indev_drv = (lv_indev_t *)indev;
    lv_indev_data_t *indev_data = (lv_indev_data_t *)data;

    tft35_touch_point_t point;

    if (tft35_touch_read(&point)) {
        indev_data->point.x = point.x;
        indev_data->point.y = point.y;
        indev_data->state = LV_INDEV_STATE_PRESSED;
    } else {
        indev_data->state = LV_INDEV_STATE_RELEASED;
    }
}
