/**
 * @file lvgl_port.cpp
 * @brief LVGL Port Layer - Main UI initialization
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"

// Forward declarations for all UI screens
extern void ui_main_menu_create(void);
extern void ui_main_menu_show(void);

// Main screen reference
static lv_obj_t *main_screen = NULL;

// Initialize all UI components
void ui_init(void) {
    // Initialize common styles
    ui_common_init();

    // Create and show main menu
    ui_main_menu_create();
    ui_main_menu_show();
}

// Get main screen
lv_obj_t* ui_get_main_screen(void) {
    return main_screen;
}
