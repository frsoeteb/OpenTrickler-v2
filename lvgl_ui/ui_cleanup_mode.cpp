/**
 * @file ui_cleanup_mode.cpp
 * @brief Cleanup Mode Display Screen
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include <stdio.h>

// External dependencies
extern "C" {
    #include "cleanup_mode.h"
    #include "app.h"
}

// Forward declarations
extern void ui_show_main_menu(void);

// Screen objects
static lv_obj_t *cleanup_screen = NULL;
static lv_obj_t *weight_label = NULL;
static lv_obj_t *flow_label = NULL;
static lv_obj_t *speed_label = NULL;
static lv_obj_t *servo_label = NULL;
static lv_obj_t *speed_slider = NULL;

// Current speed value
static float current_speed = 0.0f;

// Speed slider callback
static void slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_speed = (float)lv_slider_get_value(slider) / 10.0f;

    // Update external speed
    extern cleanup_mode_config_t cleanup_mode_config;
    cleanup_mode_config.trickler_speed = current_speed;

    // Update label
    char buf[32];
    snprintf(buf, sizeof(buf), "Speed: %.1f rps", current_speed);
    lv_label_set_text(speed_label, buf);
}

// Speed +/- button callbacks
static void btn_speed_up_cb(lv_event_t *e) {
    (void)e;
    current_speed += 0.5f;
    if (current_speed > 20.0f) current_speed = 20.0f;

    lv_slider_set_value(speed_slider, (int)(current_speed * 10), LV_ANIM_ON);

    extern cleanup_mode_config_t cleanup_mode_config;
    cleanup_mode_config.trickler_speed = current_speed;

    char buf[32];
    snprintf(buf, sizeof(buf), "Speed: %.1f rps", current_speed);
    lv_label_set_text(speed_label, buf);
}

static void btn_speed_down_cb(lv_event_t *e) {
    (void)e;
    current_speed -= 0.5f;
    if (current_speed < 0.0f) current_speed = 0.0f;

    lv_slider_set_value(speed_slider, (int)(current_speed * 10), LV_ANIM_ON);

    extern cleanup_mode_config_t cleanup_mode_config;
    cleanup_mode_config.trickler_speed = current_speed;

    char buf[32];
    snprintf(buf, sizeof(buf), "Speed: %.1f rps", current_speed);
    lv_label_set_text(speed_label, buf);
}

// Stop motors button
static void btn_stop_motors_cb(lv_event_t *e) {
    (void)e;
    current_speed = 0.0f;
    lv_slider_set_value(speed_slider, 0, LV_ANIM_ON);

    extern cleanup_mode_config_t cleanup_mode_config;
    cleanup_mode_config.trickler_speed = 0.0f;

    lv_label_set_text(speed_label, "Speed: 0.0 rps");
}

// Exit button callback
static void btn_exit_cb(lv_event_t *e) {
    (void)e;
    // Stop motors before exit
    extern cleanup_mode_config_t cleanup_mode_config;
    cleanup_mode_config.trickler_speed = 0.0f;

    // Signal to exit cleanup mode
    extern AppState_t exit_state;
    exit_state = APP_STATE_DEFAULT;

    extern QueueHandle_t encoder_event_queue;
    ButtonEncoderEvent_t event = BUTTON_RST_PRESSED;
    xQueueSend(encoder_event_queue, &event, 0);

    ui_show_main_menu();
}

// Create cleanup mode screen
void ui_cleanup_mode_create(void) {
    if (cleanup_screen) {
        lv_obj_del(cleanup_screen);
    }

    cleanup_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(cleanup_screen, UI_COLOR_BG, 0);

    // Title bar
    ui_create_titlebar(cleanup_screen, "Cleanup Mode");

    // Info panel
    lv_obj_t *info_panel = ui_create_panel(cleanup_screen);
    lv_obj_set_size(info_panel, UI_SCREEN_WIDTH - 40, 100);
    lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, 54);

    // Weight
    weight_label = lv_label_create(info_panel);
    lv_label_set_text(weight_label, "Weight: 0.000 gr");
    lv_obj_set_style_text_font(weight_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(weight_label, UI_COLOR_TEXT, 0);
    lv_obj_align(weight_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Flow rate
    flow_label = lv_label_create(info_panel);
    lv_label_set_text(flow_label, "Flow: 0.000 gr/s");
    lv_obj_set_style_text_font(flow_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(flow_label, UI_COLOR_TEXT, 0);
    lv_obj_align(flow_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // Speed
    speed_label = lv_label_create(info_panel);
    lv_label_set_text(speed_label, "Speed: 0.0 rps");
    lv_obj_set_style_text_font(speed_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(speed_label, UI_COLOR_TEXT, 0);
    lv_obj_align(speed_label, LV_ALIGN_TOP_LEFT, 0, 50);

    // Servo state
    servo_label = lv_label_create(info_panel);
    lv_label_set_text(servo_label, "Servo: Disabled");
    lv_obj_set_style_text_font(servo_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(servo_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(servo_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Speed control panel
    lv_obj_t *speed_panel = ui_create_panel(cleanup_screen);
    lv_obj_set_size(speed_panel, UI_SCREEN_WIDTH - 40, 80);
    lv_obj_align(speed_panel, LV_ALIGN_TOP_MID, 0, 164);

    // Speed down button
    lv_obj_t *btn_down = ui_create_button(speed_panel, "-", btn_speed_down_cb);
    lv_obj_set_size(btn_down, 56, 56);
    lv_obj_align(btn_down, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(btn_down, UI_FONT_XLARGE, 0);
    lv_obj_set_style_radius(btn_down, 28, 0);  // Circular

    // Speed slider
    speed_slider = lv_slider_create(speed_panel);
    lv_obj_set_size(speed_slider, 250, 30);
    lv_obj_align(speed_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(speed_slider, 0, 200);  // 0.0 to 20.0 rps
    lv_slider_set_value(speed_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(speed_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Speed up button
    lv_obj_t *btn_up = ui_create_button(speed_panel, "+", btn_speed_up_cb);
    lv_obj_set_size(btn_up, 56, 56);
    lv_obj_align(btn_up, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(btn_up, UI_FONT_XLARGE, 0);
    lv_obj_set_style_radius(btn_up, 28, 0);  // Circular

    // Bottom buttons - clean and bold
    lv_obj_t *btn_stop = ui_create_button(cleanup_screen, "STOP", btn_stop_motors_cb);
    lv_obj_set_size(btn_stop, 130, 46);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_stop, UI_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(btn_stop, UI_FONT_MEDIUM, 0);

    lv_obj_t *btn_exit = ui_create_button(cleanup_screen, "EXIT", btn_exit_cb);
    lv_obj_set_size(btn_exit, 110, 46);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn_exit, UI_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(btn_exit, UI_FONT_MEDIUM, 0);
}

void ui_cleanup_mode_show(void) {
    ui_cleanup_mode_create();
    lv_scr_load(cleanup_screen);
}

// Update cleanup mode display
void ui_cleanup_mode_update(float weight, float flow_rate, float speed, const char *servo_state) {
    if (!cleanup_screen) return;

    char buf[32];

    if (weight_label) {
        snprintf(buf, sizeof(buf), "Weight: %.3f gr", weight);
        lv_label_set_text(weight_label, buf);
    }

    if (flow_label) {
        snprintf(buf, sizeof(buf), "Flow: %.3f gr/s", flow_rate);
        lv_label_set_text(flow_label, buf);
    }

    if (speed_label) {
        snprintf(buf, sizeof(buf), "Speed: %.1f rps", speed);
        lv_label_set_text(speed_label, buf);
    }

    if (servo_label && servo_state) {
        snprintf(buf, sizeof(buf), "Servo: %s", servo_state);
        lv_label_set_text(servo_label, buf);
    }
}

// Public interface
void ui_show_cleanup_mode(void) {
    ui_cleanup_mode_show();
}
