/**
 * @file ui_charge_mode.cpp
 * @brief Charge Mode Display Screen
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include <stdio.h>

// External dependencies
extern "C" {
    #include "charge_mode.h"
    #include "profile.h"
    #include "app.h"
}

// Forward declarations
extern void ui_show_main_menu(void);

// Screen objects
static lv_obj_t *charge_mode_screen = NULL;
static lv_obj_t *target_label = NULL;
static lv_obj_t *weight_label = NULL;
static lv_obj_t *timer_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *profile_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *status_led = NULL;

// Status colors
static const lv_color_t color_waiting = {.blue = 0xF3, .green = 0x96, .red = 0x21};  // Blue
static const lv_color_t color_charging = {.blue = 0x07, .green = 0xC1, .red = 0xFF}; // Amber
static const lv_color_t color_complete = {.blue = 0x50, .green = 0xAF, .red = 0x4C}; // Green
static const lv_color_t color_error = {.blue = 0x36, .green = 0x43, .red = 0xF4};    // Red

// Stop button callback
static void btn_stop_cb(lv_event_t *e) {
    (void)e;
    // Signal to exit charge mode
    extern AppState_t exit_state;
    exit_state = APP_STATE_DEFAULT;

    extern QueueHandle_t encoder_event_queue;
    ButtonEncoderEvent_t event = BUTTON_RST_PRESSED;
    xQueueSend(encoder_event_queue, &event, 0);

    ui_show_main_menu();
}

// Create charge mode screen
void ui_charge_mode_create(void) {
    if (charge_mode_screen) {
        lv_obj_del(charge_mode_screen);
    }

    charge_mode_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(charge_mode_screen, UI_COLOR_BG, 0);

    // Title bar with target and timer
    lv_obj_t *title_bar = lv_obj_create(charge_mode_screen);
    lv_obj_remove_style_all(title_bar);
    lv_obj_set_style_bg_color(title_bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_size(title_bar, UI_SCREEN_WIDTH, 44);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);

    target_label = lv_label_create(title_bar);
    lv_label_set_text(target_label, "Target: ---.-- gr");
    lv_obj_set_style_text_font(target_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(target_label, UI_COLOR_TEXT, 0);
    lv_obj_align(target_label, LV_ALIGN_LEFT_MID, UI_PADDING, 0);

    timer_label = lv_label_create(title_bar);
    lv_label_set_text(timer_label, "0.00s");
    lv_obj_set_style_text_font(timer_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(timer_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(timer_label, LV_ALIGN_RIGHT_MID, -UI_PADDING, 0);

    // Large weight display
    lv_obj_t *weight_panel = ui_create_panel(charge_mode_screen);
    lv_obj_set_size(weight_panel, UI_SCREEN_WIDTH - 40, 100);
    lv_obj_align(weight_panel, LV_ALIGN_TOP_MID, 0, 54);

    weight_label = lv_label_create(weight_panel);
    lv_label_set_text(weight_label, "---.---");
    lv_obj_set_style_text_font(weight_label, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(weight_label, UI_COLOR_TEXT, 0);
    lv_obj_align(weight_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *unit_label = lv_label_create(weight_panel);
    lv_label_set_text(unit_label, "gr");
    lv_obj_set_style_text_font(unit_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(unit_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 25);

    // Progress bar
    progress_bar = lv_bar_create(charge_mode_screen);
    lv_obj_set_size(progress_bar, UI_SCREEN_WIDTH - 40, 20);
    lv_obj_align(progress_bar, LV_ALIGN_TOP_MID, 0, 164);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_color(progress_bar, UI_COLOR_PRIMARY, LV_PART_INDICATOR);

    // Status LED indicator
    status_led = lv_led_create(charge_mode_screen);
    lv_obj_set_size(status_led, 20, 20);
    lv_obj_align(status_led, LV_ALIGN_TOP_LEFT, 20, 194);
    lv_led_set_color(status_led, color_waiting);
    lv_led_on(status_led);

    // Status text
    status_label = lv_label_create(charge_mode_screen);
    lv_label_set_text(status_label, "Waiting for Zero");
    lv_obj_set_style_text_font(status_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 50, 196);

    // Profile name
    profile_label = lv_label_create(charge_mode_screen);
    profile_data_t *profile = get_selected_profile();
    if (profile) {
        lv_label_set_text(profile_label, profile->name);
    } else {
        lv_label_set_text(profile_label, "No Profile");
    }
    lv_obj_set_style_text_font(profile_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(profile_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(profile_label, LV_ALIGN_TOP_LEFT, 20, 220);

    // Stop button - bold red
    lv_obj_t *btn_stop = ui_create_button(charge_mode_screen, "STOP", btn_stop_cb);
    lv_obj_set_size(btn_stop, 140, 56);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_stop, UI_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(btn_stop, UI_FONT_LARGE, 0);
    lv_obj_set_style_shadow_width(btn_stop, 8, 0);
    lv_obj_set_style_shadow_color(btn_stop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(btn_stop, LV_OPA_30, 0);
    lv_obj_set_style_radius(btn_stop, 12, 0);
}

void ui_charge_mode_show(void) {
    ui_charge_mode_create();
    lv_scr_load(charge_mode_screen);
}

// Update charge mode display
void ui_charge_mode_update(float current_weight, float target_weight, float elapsed_time, const char *status) {
    if (!charge_mode_screen) return;

    char buf[32];

    // Update target
    if (target_label) {
        snprintf(buf, sizeof(buf), "Target: %.2f gr", target_weight);
        lv_label_set_text(target_label, buf);
    }

    // Update weight
    if (weight_label) {
        charge_mode_config_t *config = get_charge_mode_config();
        if (config->eeprom_charge_mode_data.decimal_places == DP_2) {
            snprintf(buf, sizeof(buf), "%.2f", current_weight);
        } else {
            snprintf(buf, sizeof(buf), "%.3f", current_weight);
        }
        lv_label_set_text(weight_label, buf);
    }

    // Update timer
    if (timer_label) {
        snprintf(buf, sizeof(buf), "%.2fs", elapsed_time);
        lv_label_set_text(timer_label, buf);
    }

    // Update status
    if (status_label && status) {
        lv_label_set_text(status_label, status);
    }

    // Update progress bar
    if (progress_bar && target_weight > 0) {
        int progress = (int)((current_weight / target_weight) * 100);
        if (progress < 0) progress = 0;
        if (progress > 100) progress = 100;
        lv_bar_set_value(progress_bar, progress, LV_ANIM_ON);
    }

    // Update LED color based on status
    if (status_led && status) {
        if (strstr(status, "Zero") || strstr(status, "Return")) {
            lv_led_set_color(status_led, color_waiting);
        } else if (strstr(status, "Complete") || strstr(status, "Remove")) {
            // Check if over/under
            float error = current_weight - target_weight;
            if (error < -0.02f) {
                lv_led_set_color(status_led, color_charging);  // Under
            } else if (error > 0.02f) {
                lv_led_set_color(status_led, color_error);  // Over
            } else {
                lv_led_set_color(status_led, color_complete);  // Good
            }
        } else {
            lv_led_set_color(status_led, color_charging);
        }
    }
}

// Public interface
void ui_show_charge_mode(void) {
    ui_charge_mode_show();
}
