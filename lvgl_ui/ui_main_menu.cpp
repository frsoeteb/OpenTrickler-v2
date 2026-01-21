/**
 * @file ui_main_menu.cpp
 * @brief Main Menu Screen
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include "version.h"

// Forward declarations for navigation
extern void ui_show_profile_select(void);
extern void ui_show_cleanup_warning(void);
extern void ui_show_wireless(void);
extern void ui_show_settings(void);

// Screen object
static lv_obj_t *main_menu_screen = NULL;
static lv_obj_t *status_label = NULL;

// Button callbacks
static void btn_start_cb(lv_event_t *e) {
    (void)e;
    ui_show_profile_select();
}

static void btn_cleanup_cb(lv_event_t *e) {
    (void)e;
    ui_show_cleanup_warning();
}

static void btn_wireless_cb(lv_event_t *e) {
    (void)e;
    ui_show_wireless();
}

static void btn_settings_cb(lv_event_t *e) {
    (void)e;
    ui_show_settings();
}

// Create main menu screen
void ui_main_menu_create(void) {
    if (main_menu_screen) {
        lv_obj_del(main_menu_screen);
    }

    main_menu_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_menu_screen, UI_COLOR_BG, 0);

    // Title bar
    ui_create_titlebar(main_menu_screen, "OpenTrickler");

    // Button container
    lv_obj_t *btn_container = lv_obj_create(main_menu_screen);
    lv_obj_remove_style_all(btn_container);
    lv_obj_set_size(btn_container, 320, 220);
    lv_obj_align(btn_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_obj_set_style_pad_all(btn_container, 10, 0);

    // Main menu buttons with icons
    lv_obj_t *btn_start = ui_create_large_button(btn_container, NULL, "START", btn_start_cb);
    lv_obj_set_style_bg_color(btn_start, UI_COLOR_SECONDARY, 0);  // Green for start

    lv_obj_t *btn_cleanup = ui_create_large_button(btn_container, LV_SYMBOL_REFRESH, "CLEANUP", btn_cleanup_cb);
    lv_obj_set_style_bg_color(btn_cleanup, UI_COLOR_WARNING, 0);  // Amber for cleanup

    lv_obj_t *btn_wireless = ui_create_large_button(btn_container, LV_SYMBOL_WIFI, "WIRELESS", btn_wireless_cb);

    lv_obj_t *btn_settings = ui_create_large_button(btn_container, LV_SYMBOL_SETTINGS, "SETTINGS", btn_settings_cb);

    // Status bar at bottom
    lv_obj_t *statusbar = ui_create_statusbar(main_menu_screen);

    status_label = lv_label_create(statusbar);
    lv_label_set_text(status_label, "Ready");
    lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(status_label, UI_FONT_SMALL, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 0, 0);

    // Version label on right side of status bar
    lv_obj_t *version_label = lv_label_create(statusbar);
    lv_label_set_text_fmt(version_label, "v%s", version_string);
    lv_obj_set_style_text_color(version_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(version_label, UI_FONT_SMALL, 0);
    lv_obj_align(version_label, LV_ALIGN_RIGHT_MID, 0, 0);
}

void ui_main_menu_show(void) {
    if (!main_menu_screen) {
        ui_main_menu_create();
    }
    lv_scr_load(main_menu_screen);
}

void ui_main_menu_update_status(const char *status) {
    if (status_label) {
        lv_label_set_text(status_label, status);
    }
}

// Public interface
void ui_show_main_menu(void) {
    ui_main_menu_show();
}
