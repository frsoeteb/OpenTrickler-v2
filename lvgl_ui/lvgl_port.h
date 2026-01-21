/**
 * @file lvgl_port.h
 * @brief LVGL Port Layer for FreeRTOS
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Initialize UI (called from tft35_lvgl_task)
void ui_init(void);

// Get the main screen object
lv_obj_t* ui_get_main_screen(void);

// Screen navigation
void ui_show_main_menu(void);
void ui_show_profile_select(void);
void ui_show_charge_input(void);
void ui_show_charge_mode(void);
void ui_show_cleanup_mode(void);
void ui_show_settings(void);
void ui_show_ai_tuning(void);
void ui_show_warning(const char *title, const char *message, void (*on_confirm)(void), void (*on_cancel)(void));

// Update functions for dynamic screens
void ui_charge_mode_update(float current_weight, float target_weight, float elapsed_time, const char *status);
void ui_cleanup_mode_update(float weight, float flow_rate, float speed, const char *servo_state);

#ifdef __cplusplus
}
#endif

#endif // LVGL_PORT_H
