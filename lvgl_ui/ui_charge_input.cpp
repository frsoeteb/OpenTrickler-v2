/**
 * @file ui_charge_input.cpp
 * @brief Charge Weight Input Screen with numeric keypad
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// External dependencies
extern "C" {
    #include "charge_mode.h"
    #include "profile.h"
}

// Forward declarations
extern void ui_show_profile_select(void);
extern void ui_show_charge_warning(void);

// Screen objects
static lv_obj_t *charge_input_screen = NULL;
static lv_obj_t *weight_label = NULL;
static char weight_str[16] = "0.00";
static int decimal_places = 2;
static bool has_decimal = false;
static int digit_count = 0;

// Keypad map
static const char *keypad_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, ""
};

// Update weight display
static void update_weight_display(void) {
    if (weight_label) {
        char display_str[24];
        snprintf(display_str, sizeof(display_str), "%s gr", weight_str);
        lv_label_set_text(weight_label, display_str);
    }
}

// Reset input
static void reset_input(void) {
    if (decimal_places == 2) {
        strcpy(weight_str, "0.00");
    } else {
        strcpy(weight_str, "0.000");
    }
    has_decimal = false;
    digit_count = 0;
    update_weight_display();
}

// Keypad callback
static void keypad_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    const char *txt = lv_btnmatrix_get_btn_text(obj, id);

    if (!txt) return;

    size_t len = strlen(weight_str);

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        // Backspace - remove last character
        if (len > 1) {
            if (weight_str[len - 1] == '.') {
                has_decimal = false;
            }
            weight_str[len - 1] = '\0';
            digit_count--;
            if (digit_count < 0) digit_count = 0;
        } else {
            strcpy(weight_str, "0");
            has_decimal = false;
            digit_count = 0;
        }
    } else if (strcmp(txt, ".") == 0) {
        // Decimal point
        if (!has_decimal && len < 14) {
            strcat(weight_str, ".");
            has_decimal = true;
        }
    } else {
        // Digit
        // Limit digits after decimal
        if (has_decimal) {
            const char *dec_pos = strchr(weight_str, '.');
            if (dec_pos) {
                int digits_after = strlen(dec_pos + 1);
                if (digits_after >= decimal_places) return;
            }
        }

        // Limit total length
        if (len < 10) {
            // Replace leading zero
            if (strcmp(weight_str, "0") == 0 && strcmp(txt, "0") != 0) {
                strcpy(weight_str, txt);
            } else if (strcmp(weight_str, "0") == 0 && strcmp(txt, "0") == 0) {
                // Don't add more leading zeros
            } else {
                strcat(weight_str, txt);
            }
            digit_count++;
        }
    }

    update_weight_display();
}

// Back button callback
static void btn_back_cb(lv_event_t *e) {
    (void)e;
    ui_show_profile_select();
}

// Start button callback
static void btn_start_cb(lv_event_t *e) {
    (void)e;

    // Parse weight value and set it
    float weight = 0.0f;
    sscanf(weight_str, "%f", &weight);

    if (weight > 0.0f) {
        set_charge_weight(weight);
        ui_show_charge_warning();
    } else {
        ui_show_toast("Enter a valid weight", 2000);
    }
}

// Clear button callback
static void btn_clear_cb(lv_event_t *e) {
    (void)e;
    reset_input();
}

// Create charge input screen
void ui_charge_input_create(void) {
    if (charge_input_screen) {
        lv_obj_del(charge_input_screen);
    }

    // Get decimal places from config
    charge_mode_config_t *config = get_charge_mode_config();
    decimal_places = (config->eeprom_charge_mode_data.decimal_places == DP_2) ? 2 : 3;

    charge_input_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(charge_input_screen, UI_COLOR_BG, 0);

    // Title bar
    ui_create_titlebar(charge_input_screen, "Set Charge Weight");

    // Weight display panel
    lv_obj_t *weight_panel = ui_create_panel(charge_input_screen);
    lv_obj_set_size(weight_panel, UI_SCREEN_WIDTH - 40, 60);
    lv_obj_align(weight_panel, LV_ALIGN_TOP_MID, 0, 54);

    weight_label = lv_label_create(weight_panel);
    lv_obj_set_style_text_font(weight_label, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(weight_label, UI_COLOR_TEXT, 0);
    lv_obj_center(weight_label);
    reset_input();

    // Numeric keypad
    lv_obj_t *keypad = lv_btnmatrix_create(charge_input_screen);
    lv_btnmatrix_set_map(keypad, keypad_map);
    lv_obj_set_size(keypad, 240, 160);
    lv_obj_align(keypad, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(keypad, keypad_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(keypad, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_color(keypad, UI_COLOR_PRIMARY, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keypad, UI_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(keypad, UI_FONT_MEDIUM, LV_PART_ITEMS);

    // Clear button
    lv_obj_t *btn_clear = ui_create_button(charge_input_screen, "CLEAR", btn_clear_cb);
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_LEFT, 20, -60);

    // Navigation bar
    ui_create_navbar(charge_input_screen, "Back", btn_back_cb, "Start", btn_start_cb);
}

void ui_charge_input_show(void) {
    ui_charge_input_create();
    lv_scr_load(charge_input_screen);
}

// Public interface
void ui_show_charge_input(void) {
    ui_charge_input_show();
}
