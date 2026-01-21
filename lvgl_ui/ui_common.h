/**
 * @file ui_common.h
 * @brief Common UI utilities and styles
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Color definitions
#define UI_COLOR_PRIMARY        lv_color_hex(0x2196F3)  // Blue
#define UI_COLOR_SECONDARY      lv_color_hex(0x4CAF50)  // Green
#define UI_COLOR_WARNING        lv_color_hex(0xFFC107)  // Amber
#define UI_COLOR_ERROR          lv_color_hex(0xF44336)  // Red
#define UI_COLOR_BG             lv_color_hex(0x1E1E1E)  // Dark gray
#define UI_COLOR_PANEL          lv_color_hex(0x2D2D2D)  // Lighter gray
#define UI_COLOR_TEXT           lv_color_hex(0xFFFFFF)  // White
#define UI_COLOR_TEXT_SECONDARY lv_color_hex(0xB0B0B0)  // Light gray

// Font shortcuts
#define UI_FONT_SMALL       &lv_font_montserrat_12
#define UI_FONT_NORMAL      &lv_font_montserrat_14
#define UI_FONT_MEDIUM      &lv_font_montserrat_18
#define UI_FONT_LARGE       &lv_font_montserrat_24
#define UI_FONT_XLARGE      &lv_font_montserrat_32

// Screen dimensions (from display driver - supports TFT35 and TFT43)
#include "tft35_display.h"
#define UI_SCREEN_WIDTH     TFT35_DISPLAY_WIDTH
#define UI_SCREEN_HEIGHT    TFT35_DISPLAY_HEIGHT

// Common margins/padding
#define UI_PADDING          10
#define UI_MARGIN           5

// Initialize common styles
void ui_common_init(void);

// Create a standard title bar
lv_obj_t* ui_create_titlebar(lv_obj_t *parent, const char *title);

// Create a standard panel
lv_obj_t* ui_create_panel(lv_obj_t *parent);

// Create a large button with icon (for main menu)
lv_obj_t* ui_create_large_button(lv_obj_t *parent, const char *icon, const char *text, lv_event_cb_t event_cb);

// Create a standard button
lv_obj_t* ui_create_button(lv_obj_t *parent, const char *text, lv_event_cb_t event_cb);

// Create a navigation bar with back/next buttons
lv_obj_t* ui_create_navbar(lv_obj_t *parent,
                           const char *back_text, lv_event_cb_t back_cb,
                           const char *next_text, lv_event_cb_t next_cb);

// Create a status bar (bottom of screen)
lv_obj_t* ui_create_statusbar(lv_obj_t *parent);

// Show a toast message
void ui_show_toast(const char *message, uint32_t duration_ms);

// Utility: Format weight string
void ui_format_weight(char *buf, size_t buf_size, float weight, int decimals);

#ifdef __cplusplus
}
#endif

#endif // UI_COMMON_H
