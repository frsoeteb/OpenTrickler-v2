/**
 * @file ui_profile_select.cpp
 * @brief Profile Selection Screen
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"

// External dependencies
extern "C" {
    #include "profile.h"
}

// Forward declarations
extern void ui_show_charge_input(void);
extern void ui_show_main_menu(void);

// Screen object
static lv_obj_t *profile_screen = NULL;
static lv_obj_t *profile_list = NULL;
static int selected_profile_idx = 0;

// Back button callback
static void btn_back_cb(lv_event_t *e) {
    (void)e;
    ui_show_main_menu();
}

// Next button callback
static void btn_next_cb(lv_event_t *e) {
    (void)e;
    // Set selected profile before going to charge input
    select_profile(selected_profile_idx);
    ui_show_charge_input();
}

// List item callback
static void list_item_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    selected_profile_idx = (int)(intptr_t)lv_event_get_user_data(e);

    // Update visual selection
    uint32_t child_cnt = lv_obj_get_child_cnt(profile_list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(profile_list, i);
        if (child == btn) {
            lv_obj_set_style_bg_color(child, UI_COLOR_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(child, UI_COLOR_PANEL, 0);
        }
    }
}

// Create profile selection screen
void ui_profile_select_create(void) {
    if (profile_screen) {
        lv_obj_del(profile_screen);
    }

    profile_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(profile_screen, UI_COLOR_BG, 0);

    // Title bar
    ui_create_titlebar(profile_screen, "Select Profile");

    // Profile list
    profile_list = lv_list_create(profile_screen);
    lv_obj_set_size(profile_list, UI_SCREEN_WIDTH - 40, 180);
    lv_obj_align(profile_list, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_bg_color(profile_list, UI_COLOR_PANEL, 0);
    lv_obj_set_style_border_width(profile_list, 0, 0);
    lv_obj_set_style_radius(profile_list, 8, 0);

    // Populate profile list
    profile_data_t *profiles = get_profile_data();
    int profile_count = get_profile_count();

    for (int i = 0; i < profile_count && i < MAX_PROFILE_CNT; i++) {
        lv_obj_t *btn = lv_list_add_btn(profile_list, NULL, profiles[i].name);
        lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL, 0);
        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT, 0);

        // Highlight current selection
        if (i == selected_profile_idx) {
            lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
        }
    }

    // Navigation bar
    ui_create_navbar(profile_screen, "Back", btn_back_cb, "Next", btn_next_cb);
}

void ui_profile_select_show(void) {
    ui_profile_select_create();
    lv_scr_load(profile_screen);
}

// Public interface
void ui_show_profile_select(void) {
    ui_profile_select_show();
}
