/**
 * @file tft35_module.cpp
 * @brief TFT35 Module - Main initialization and LVGL integration
 */

#include "tft35_module.h"
#include "tft35_display.h"
#include "tft35_touch.h"
#include "configuration.h"
#include "eeprom.h"
#include "display_config.h"
#include "mini_12864_module.h"  // For encoder_event_queue and ButtonEncoderEvent_t
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "lvgl.h"

// EEPROM address defined in eeprom.h (EEPROM_TFT35_CONFIG_BASE_ADDR = 11K)
#define TFT35_CONFIG_DATA_REV       1

// LVGL display buffer size (2 buffers for DMA double-buffering)
#define LVGL_BUF_LINES              40
#define LVGL_BUF_SIZE               (TFT35_DISPLAY_WIDTH * LVGL_BUF_LINES)

// Static variables
static tft35_module_config_t module_config;
static lv_display_t *display = NULL;
static lv_indev_t *touch_indev = NULL;
static lv_indev_t *encoder_indev = NULL;
static TimerHandle_t lvgl_tick_timer = NULL;

// External encoder queue (initialized by button_init in mini_12864_module.cpp)
extern QueueHandle_t encoder_event_queue;

// LVGL display buffers
static lv_color_t lvgl_buf1[LVGL_BUF_SIZE];
static lv_color_t lvgl_buf2[LVGL_BUF_SIZE];

// Forward declarations
static void load_config(void);
static void lvgl_tick_callback(TimerHandle_t timer);
static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

// Encoder read callback for LVGL
static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    data->enc_diff = 0;

    if (encoder_event_queue == NULL) {
        return;
    }

    ButtonEncoderEvent_t event;
    while (xQueueReceive(encoder_event_queue, &event, 0) == pdTRUE) {
        switch (event) {
            case BUTTON_ENCODER_ROTATE_CW:
                data->enc_diff = 1;
                break;
            case BUTTON_ENCODER_ROTATE_CCW:
                data->enc_diff = -1;
                break;
            case BUTTON_ENCODER_PRESSED:
                data->state = LV_INDEV_STATE_PRESSED;
                break;
            default:
                break;
        }
    }
}

// Load configuration from EEPROM
static void load_config(void) {
    eeprom_read(EEPROM_TFT35_CONFIG_BASE_ADDR, (uint8_t *)&module_config, sizeof(module_config));

    // Check if config is valid
    if (module_config.data_rev != TFT35_CONFIG_DATA_REV) {
        // Initialize with defaults
        module_config.data_rev = TFT35_CONFIG_DATA_REV;
        module_config.rotation = TFT35_ROTATION_0;
        module_config.brightness = 255;
        module_config.touch_cal_x_min = 300;
        module_config.touch_cal_x_max = 3800;
        module_config.touch_cal_y_min = 300;
        module_config.touch_cal_y_max = 3800;
        module_config.touch_inverted_x = false;
        module_config.touch_inverted_y = false;
    }
}

// LVGL tick timer callback (1ms)
static void lvgl_tick_callback(TimerHandle_t timer) {
    (void)timer;
    lv_tick_inc(1);
}

// Initialize the TFT35 module
void tft35_module_init(void) {
    // Load configuration (for touch calibration data)
    load_config();

    // Initialize display hardware
    // Rotation and brightness now come from central display_config
    display_config_t *disp_cfg = display_config_get();
    tft35_display_init();
    tft35_display_set_rotation((tft35_rotation_t)disp_cfg->rotation);
    tft35_display_set_brightness(disp_cfg->brightness);

    // Initialize touch hardware
    tft35_touch_init();
    tft35_touch_set_calibration(
        module_config.touch_cal_x_min,
        module_config.touch_cal_x_max,
        module_config.touch_cal_y_min,
        module_config.touch_cal_y_max,
        module_config.touch_inverted_x,
        module_config.touch_inverted_y
    );

    // Initialize LVGL
    lv_init();

    // Create LVGL display
    display = lv_display_create(TFT35_DISPLAY_WIDTH, TFT35_DISPLAY_HEIGHT);
    lv_display_set_flush_cb(display, (lv_display_flush_cb_t)tft35_display_flush_cb);
    lv_display_set_buffers(display, lvgl_buf1, lvgl_buf2, sizeof(lvgl_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create LVGL touch input device
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, (lv_indev_read_cb_t)tft35_touch_read_cb);

    // Create LVGL encoder input device (rotary knob)
    encoder_indev = lv_indev_create();
    lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(encoder_indev, encoder_read_cb);

    // Create LVGL tick timer (1ms period)
    lvgl_tick_timer = xTimerCreate(
        "LVGL Tick",
        pdMS_TO_TICKS(1),
        pdTRUE,     // Auto-reload
        NULL,
        lvgl_tick_callback
    );
    xTimerStart(lvgl_tick_timer, 0);
}

// Get detected controller type
tft35_controller_t tft35_get_controller_type(void) {
    return tft35_display_get_controller();
}

// Set display rotation
void tft35_set_rotation(tft35_rotation_t rotation) {
    display_config_get()->rotation = (display_rotation_t)rotation;
    tft35_display_set_rotation(rotation);
}

// Set display brightness
void tft35_set_brightness(uint8_t brightness) {
    display_config_get()->brightness = brightness;
    tft35_display_set_brightness(brightness);
}

// Check if touch is calibrated
bool tft35_touch_is_calibrated(void) {
    return module_config.data_rev == TFT35_CONFIG_DATA_REV;
}

// Get module configuration
tft35_module_config_t* tft35_get_config(void) {
    return &module_config;
}

// Save configuration to EEPROM
void tft35_save_config(void) {
    // Update calibration from touch driver
    tft35_touch_get_calibration(
        &module_config.touch_cal_x_min,
        &module_config.touch_cal_x_max,
        &module_config.touch_cal_y_min,
        &module_config.touch_cal_y_max,
        &module_config.touch_inverted_x,
        &module_config.touch_inverted_y
    );

    eeprom_write(EEPROM_TFT35_CONFIG_BASE_ADDR, (uint8_t *)&module_config, sizeof(module_config));
}

// Touch calibration (placeholder - UI will implement full calibration screen)
void tft35_touch_calibrate(void) {
    // This will be implemented in the UI layer with a calibration screen
    // For now, use defaults
}

// LVGL task - main rendering loop
void tft35_lvgl_task(void *p) {
    (void)p;

    // Import UI screens (declared in lvgl_ui headers)
    extern void ui_init(void);
    ui_init();

    while (true) {
        // Let LVGL handle timers and rendering
        uint32_t time_till_next = lv_timer_handler();

        // Limit minimum delay to prevent CPU hogging
        if (time_till_next < 5) {
            time_till_next = 5;
        }
        if (time_till_next > 50) {
            time_till_next = 50;
        }

        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}
