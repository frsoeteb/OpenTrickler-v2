/**
 * @file ui_ai_tuning.cpp
 * @brief AI Auto-Tuning Screen
 *
 * Provides touch UI for the AI PID auto-tuning feature.
 * - Start tuning for selected profile
 * - Shows progress during tuning (drops completed, current phase)
 * - Displays recommended parameters when complete
 * - Apply or cancel results
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include <stdio.h>

// External dependencies
extern "C" {
    #include "ai_tuning.h"
    #include "profile.h"
    #include "app.h"
}

// Forward declarations
extern void ui_show_main_menu(void);
extern void ui_show_settings(void);

// Screen objects
static lv_obj_t *ai_tuning_screen = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *phase_label = NULL;
static lv_obj_t *drop_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *stats_panel = NULL;
static lv_obj_t *coarse_kp_label = NULL;
static lv_obj_t *coarse_kd_label = NULL;
static lv_obj_t *fine_kp_label = NULL;
static lv_obj_t *fine_kd_label = NULL;
static lv_obj_t *overthrow_label = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *btn_start = NULL;
static lv_obj_t *btn_apply = NULL;
static lv_obj_t *btn_cancel = NULL;

static lv_timer_t *update_timer = NULL;

// Start tuning callback
static void btn_start_cb(lv_event_t *e) {
    (void)e;

    profile_data_t *profile = get_selected_profile();
    if (!profile) {
        ui_show_toast("Select a profile first", 2000);
        return;
    }

    // Initialize AI tuning
    ai_tuning_init();

    // Start tuning session
    if (ai_tuning_start((profile_t*)profile)) {
        lv_obj_add_flag(btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(status_label, "Tuning in progress...");
        lv_label_set_text(phase_label, "Phase 1: Coarse Trickler");

        // Signal to enter charge mode for tuning
        extern AppState_t exit_state;
        exit_state = APP_STATE_ENTER_CHARGE_MODE;

        extern QueueHandle_t encoder_event_queue;
        ButtonEncoderEvent_t event = OVERRIDE_FROM_REST;
        xQueueSend(encoder_event_queue, &event, 0);
    } else {
        ui_show_toast("Failed to start tuning", 2000);
    }
}

// Apply parameters callback
static void btn_apply_cb(lv_event_t *e) {
    (void)e;

    if (ai_tuning_apply_params()) {
        ui_show_toast("Parameters applied!", 2000);

        // Save to EEPROM
        extern void eeprom_save_all(void);
        eeprom_save_all();

        ui_show_settings();
    } else {
        ui_show_toast("Failed to apply", 2000);
    }
}

// Cancel callback
static void btn_cancel_cb(lv_event_t *e) {
    (void)e;

    ai_tuning_cancel();
    ui_show_settings();
}

// Back callback
static void btn_back_cb(lv_event_t *e) {
    (void)e;

    if (ai_tuning_is_active()) {
        ui_show_warning("Cancel Tuning?",
                       "Tuning is in progress. Cancel and lose results?",
                       []() {
                           ai_tuning_cancel();
                           ui_show_settings();
                       },
                       NULL);
    } else {
        ui_show_settings();
    }
}

// Update timer callback - refresh display during tuning
static void update_timer_cb(lv_timer_t *timer) {
    (void)timer;

    if (!ai_tuning_screen) return;

    ai_tuning_session_t *session = ai_tuning_get_session();
    if (!session) return;

    char buf[64];

    // Update progress bar
    uint8_t progress = ai_tuning_get_progress_percent();
    lv_bar_set_value(progress_bar, progress, LV_ANIM_ON);

    // Update drop count
    snprintf(buf, sizeof(buf), "Drop %d / %d", session->drops_completed, session->total_drops_target);
    lv_label_set_text(drop_label, buf);

    // Update phase
    switch (session->state) {
        case AI_TUNING_PHASE_1_COARSE:
            lv_label_set_text(phase_label, "Phase 1: Tuning Coarse Trickler");
            lv_label_set_text(status_label, "Put pan on scale, charge will start...");
            break;

        case AI_TUNING_PHASE_2_FINE:
            lv_label_set_text(phase_label, "Phase 2: Tuning Fine Trickler");
            lv_label_set_text(status_label, "Continue charging...");
            break;

        case AI_TUNING_COMPLETE:
            lv_label_set_text(phase_label, "Tuning Complete!");
            lv_label_set_text(status_label, "Review results below");

            // Show results
            snprintf(buf, sizeof(buf), "Coarse Kp: %.2f", session->recommended_coarse_kp);
            lv_label_set_text(coarse_kp_label, buf);

            snprintf(buf, sizeof(buf), "Coarse Kd: %.2f", session->recommended_coarse_kd);
            lv_label_set_text(coarse_kd_label, buf);

            snprintf(buf, sizeof(buf), "Fine Kp: %.2f", session->recommended_fine_kp);
            lv_label_set_text(fine_kp_label, buf);

            snprintf(buf, sizeof(buf), "Fine Kd: %.2f", session->recommended_fine_kd);
            lv_label_set_text(fine_kd_label, buf);

            snprintf(buf, sizeof(buf), "Avg Overthrow: %.2f%%", session->avg_overthrow);
            lv_label_set_text(overthrow_label, buf);

            snprintf(buf, sizeof(buf), "Avg Time: %.1f ms", session->avg_total_time);
            lv_label_set_text(time_label, buf);

            // Show apply button, hide cancel
            lv_obj_clear_flag(btn_apply, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(stats_panel, LV_OBJ_FLAG_HIDDEN);
            break;

        case AI_TUNING_ERROR:
            lv_label_set_text(phase_label, "Error!");
            lv_label_set_text(status_label, session->error_message);
            lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_start, LV_OBJ_FLAG_HIDDEN);
            break;

        default:
            break;
    }

    // Update current parameters being tested
    if (ai_tuning_is_active() && session->drops_completed > 0) {
        ai_drop_telemetry_t *last_drop = &session->drops[session->drops_completed - 1];

        snprintf(buf, sizeof(buf), "Last: Kp=%.2f Kd=%.2f Score=%.0f",
                 last_drop->coarse_kp_used, last_drop->coarse_kd_used, last_drop->overall_score);
        lv_label_set_text(overthrow_label, buf);

        snprintf(buf, sizeof(buf), "Overthrow: %.2f%% Time: %.0fms",
                 last_drop->overthrow_percent, last_drop->total_time_ms);
        lv_label_set_text(time_label, buf);
    }
}

// Create AI tuning screen
void ui_ai_tuning_create(void) {
    if (ai_tuning_screen) {
        lv_obj_del(ai_tuning_screen);
    }

    ai_tuning_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ai_tuning_screen, UI_COLOR_BG, 0);

    // Title bar
    lv_obj_t *titlebar = ui_create_titlebar(ai_tuning_screen, "AI Auto-Tuning");

    // Back button in title bar
    lv_obj_t *btn_back = lv_btn_create(titlebar);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, "BACK");
    lv_obj_center(back_label);

    // Profile info
    profile_data_t *profile = get_selected_profile();
    lv_obj_t *profile_label = lv_label_create(ai_tuning_screen);
    if (profile) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Profile: %s", profile->name);
        lv_label_set_text(profile_label, buf);
    } else {
        lv_label_set_text(profile_label, "Profile: None selected");
    }
    lv_obj_set_style_text_color(profile_label, UI_COLOR_TEXT, 0);
    lv_obj_align(profile_label, LV_ALIGN_TOP_LEFT, 20, 54);

    // Status label
    status_label = lv_label_create(ai_tuning_screen);
    lv_label_set_text(status_label, "Press Start to begin auto-tuning");
    lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 20, 74);

    // Phase label
    phase_label = lv_label_create(ai_tuning_screen);
    lv_label_set_text(phase_label, "");
    lv_obj_set_style_text_color(phase_label, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(phase_label, UI_FONT_MEDIUM, 0);
    lv_obj_align(phase_label, LV_ALIGN_TOP_LEFT, 20, 100);

    // Drop count
    drop_label = lv_label_create(ai_tuning_screen);
    lv_label_set_text(drop_label, "Drop 0 / 4");
    lv_obj_set_style_text_color(drop_label, UI_COLOR_TEXT, 0);
    lv_obj_align(drop_label, LV_ALIGN_TOP_RIGHT, -20, 100);

    // Progress bar
    progress_bar = lv_bar_create(ai_tuning_screen);
    lv_obj_set_size(progress_bar, UI_SCREEN_WIDTH - 40, 20);
    lv_obj_align(progress_bar, LV_ALIGN_TOP_MID, 0, 130);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, UI_COLOR_PANEL, 0);
    lv_obj_set_style_bg_color(progress_bar, UI_COLOR_SECONDARY, LV_PART_INDICATOR);

    // Stats panel (hidden until complete)
    stats_panel = ui_create_panel(ai_tuning_screen);
    lv_obj_set_size(stats_panel, UI_SCREEN_WIDTH - 40, 100);
    lv_obj_align(stats_panel, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_add_flag(stats_panel, LV_OBJ_FLAG_HIDDEN);

    // Stats labels
    coarse_kp_label = lv_label_create(stats_panel);
    lv_label_set_text(coarse_kp_label, "Coarse Kp: --");
    lv_obj_set_style_text_color(coarse_kp_label, UI_COLOR_TEXT, 0);
    lv_obj_align(coarse_kp_label, LV_ALIGN_TOP_LEFT, 0, 0);

    coarse_kd_label = lv_label_create(stats_panel);
    lv_label_set_text(coarse_kd_label, "Coarse Kd: --");
    lv_obj_set_style_text_color(coarse_kd_label, UI_COLOR_TEXT, 0);
    lv_obj_align(coarse_kd_label, LV_ALIGN_TOP_LEFT, 0, 20);

    fine_kp_label = lv_label_create(stats_panel);
    lv_label_set_text(fine_kp_label, "Fine Kp: --");
    lv_obj_set_style_text_color(fine_kp_label, UI_COLOR_TEXT, 0);
    lv_obj_align(fine_kp_label, LV_ALIGN_TOP_LEFT, 200, 0);

    fine_kd_label = lv_label_create(stats_panel);
    lv_label_set_text(fine_kd_label, "Fine Kd: --");
    lv_obj_set_style_text_color(fine_kd_label, UI_COLOR_TEXT, 0);
    lv_obj_align(fine_kd_label, LV_ALIGN_TOP_LEFT, 200, 20);

    overthrow_label = lv_label_create(stats_panel);
    lv_label_set_text(overthrow_label, "");
    lv_obj_set_style_text_color(overthrow_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(overthrow_label, LV_ALIGN_TOP_LEFT, 0, 50);

    time_label = lv_label_create(stats_panel);
    lv_label_set_text(time_label, "");
    lv_obj_set_style_text_color(time_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 0, 70);

    // Buttons - clean and bold
    btn_start = ui_create_button(ai_tuning_screen, "START TUNING", btn_start_cb);
    lv_obj_set_size(btn_start, 180, 54);
    lv_obj_align(btn_start, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_start, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(btn_start, UI_FONT_MEDIUM, 0);

    btn_apply = ui_create_button(ai_tuning_screen, "APPLY", btn_apply_cb);
    lv_obj_set_size(btn_apply, 120, 50);
    lv_obj_align(btn_apply, LV_ALIGN_BOTTOM_LEFT, 60, -20);
    lv_obj_set_style_bg_color(btn_apply, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(btn_apply, UI_FONT_MEDIUM, 0);
    lv_obj_add_flag(btn_apply, LV_OBJ_FLAG_HIDDEN);

    btn_cancel = ui_create_button(ai_tuning_screen, "CANCEL", btn_cancel_cb);
    lv_obj_set_size(btn_cancel, 120, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -60, -20);
    lv_obj_set_style_bg_color(btn_cancel, UI_COLOR_ERROR, 0);
    lv_obj_set_style_text_font(btn_cancel, UI_FONT_MEDIUM, 0);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);

    // Start update timer
    if (update_timer) {
        lv_timer_del(update_timer);
    }
    update_timer = lv_timer_create(update_timer_cb, 500, NULL);
}

void ui_ai_tuning_show(void) {
    ui_ai_tuning_create();
    lv_scr_load(ai_tuning_screen);
}

// Public interface
void ui_show_ai_tuning(void) {
    ui_ai_tuning_show();
}
