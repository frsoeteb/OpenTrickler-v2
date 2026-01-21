/**
 * @file ui_common.cpp
 * @brief Common UI utilities implementation
 */

#include "ui_common.h"
#include <stdio.h>
#include <string.h>

// Static styles
static lv_style_t style_panel;
static lv_style_t style_titlebar;
static lv_style_t style_btn_large;
static lv_style_t style_btn_normal;
static bool styles_initialized = false;

// Initialize common styles
void ui_common_init(void) {
    if (styles_initialized) return;

    // Panel style
    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, UI_COLOR_PANEL);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_radius(&style_panel, 8);
    lv_style_set_pad_all(&style_panel, UI_PADDING);
    lv_style_set_border_width(&style_panel, 0);

    // Title bar style
    lv_style_init(&style_titlebar);
    lv_style_set_bg_color(&style_titlebar, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_titlebar, LV_OPA_COVER);
    lv_style_set_text_color(&style_titlebar, UI_COLOR_TEXT);
    lv_style_set_pad_all(&style_titlebar, UI_PADDING);

    // Large button style - pretty with shadow
    lv_style_init(&style_btn_large);
    lv_style_set_bg_color(&style_btn_large, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_btn_large, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_large, UI_COLOR_TEXT);
    lv_style_set_text_font(&style_btn_large, UI_FONT_LARGE);
    lv_style_set_radius(&style_btn_large, 16);
    lv_style_set_pad_all(&style_btn_large, 15);
    lv_style_set_shadow_width(&style_btn_large, 10);
    lv_style_set_shadow_color(&style_btn_large, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_btn_large, LV_OPA_40);
    lv_style_set_shadow_ofs_y(&style_btn_large, 4);

    // Normal button style - clean with subtle shadow
    lv_style_init(&style_btn_normal);
    lv_style_set_bg_color(&style_btn_normal, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_btn_normal, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_normal, UI_COLOR_TEXT);
    lv_style_set_radius(&style_btn_normal, 10);
    lv_style_set_pad_all(&style_btn_normal, 12);
    lv_style_set_shadow_width(&style_btn_normal, 6);
    lv_style_set_shadow_color(&style_btn_normal, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_btn_normal, LV_OPA_30);
    lv_style_set_shadow_ofs_y(&style_btn_normal, 3);

    styles_initialized = true;
}

// Create title bar
lv_obj_t* ui_create_titlebar(lv_obj_t *parent, const char *title) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &style_titlebar, 0);
    lv_obj_set_size(bar, UI_SCREEN_WIDTH, 44);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    return bar;
}

// Create panel
lv_obj_t* ui_create_panel(lv_obj_t *parent) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &style_panel, 0);
    return panel;
}

// Create large button with icon (icon can be NULL for text-only)
lv_obj_t* ui_create_large_button(lv_obj_t *parent, const char *icon, const char *text, lv_event_cb_t event_cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &style_btn_large, 0);
    lv_obj_set_size(btn, 140, 100);

    if (event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    // Use column layout for icon + text
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (icon) {
        lv_obj_t *icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_font(icon_label, UI_FONT_XLARGE, 0);  // Large icon
    }

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);

    return btn;
}

// Create normal button
lv_obj_t* ui_create_button(lv_obj_t *parent, const char *text, lv_event_cb_t event_cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &style_btn_normal, 0);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    if (event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

// Create navigation bar - clean bold text
lv_obj_t* ui_create_navbar(lv_obj_t *parent,
                           const char *back_text, lv_event_cb_t back_cb,
                           const char *next_text, lv_event_cb_t next_cb) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UI_SCREEN_WIDTH - 20, 50);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (back_text && back_cb) {
        lv_obj_t *btn_back = ui_create_button(bar, back_text, back_cb);
        lv_obj_set_size(btn_back, 100, 44);
    } else {
        lv_obj_t *spacer = lv_obj_create(bar);
        lv_obj_remove_style_all(spacer);
        lv_obj_set_size(spacer, 100, 1);
    }

    if (next_text && next_cb) {
        lv_obj_t *btn_next = ui_create_button(bar, next_text, next_cb);
        lv_obj_set_size(btn_next, 100, 44);
        lv_obj_set_style_bg_color(btn_next, UI_COLOR_SECONDARY, 0);
    }

    return bar;
}

// Create status bar
lv_obj_t* ui_create_statusbar(lv_obj_t *parent) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_size(bar, UI_SCREEN_WIDTH, 30);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_hor(bar, UI_PADDING, 0);

    return bar;
}

// Show toast message
static lv_obj_t *toast_obj = NULL;
static lv_timer_t *toast_timer = NULL;

static void toast_timer_cb(lv_timer_t *timer) {
    if (toast_obj) {
        lv_obj_del(toast_obj);
        toast_obj = NULL;
    }
    lv_timer_del(timer);
    toast_timer = NULL;
}

void ui_show_toast(const char *message, uint32_t duration_ms) {
    // Remove existing toast
    if (toast_obj) {
        lv_obj_del(toast_obj);
        toast_obj = NULL;
    }
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }

    // Create toast container
    toast_obj = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(toast_obj);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_90, 0);
    lv_obj_set_style_radius(toast_obj, 8, 0);
    lv_obj_set_style_pad_all(toast_obj, 15, 0);
    lv_obj_set_size(toast_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(toast_obj, LV_ALIGN_BOTTOM_MID, 0, -50);

    // Create label
    lv_obj_t *label = lv_label_create(toast_obj);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_center(label);

    // Auto-dismiss timer
    toast_timer = lv_timer_create(toast_timer_cb, duration_ms, NULL);
    lv_timer_set_repeat_count(toast_timer, 1);
}

// Format weight string
void ui_format_weight(char *buf, size_t buf_size, float weight, int decimals) {
    if (decimals == 2) {
        snprintf(buf, buf_size, "%.2f", weight);
    } else {
        snprintf(buf, buf_size, "%.3f", weight);
    }
}
