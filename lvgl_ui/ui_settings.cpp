/**
 * @file ui_settings.cpp
 * @brief Settings Screens
 */

#include "lvgl_port.h"
#include "ui_common.h"
#include "lvgl.h"
#include <stdio.h>

// External dependencies
extern "C" {
    #include "app.h"
    #include "eeprom.h"
    #include "scale.h"
    #include "servo_gate.h"
    #include "version.h"
}

// Forward declarations
extern void ui_show_main_menu(void);
extern void ui_show_ai_tuning(void);

// Screen objects
static lv_obj_t *settings_screen = NULL;
static lv_obj_t *settings_list = NULL;

// Settings menu items
typedef enum {
    SETTINGS_AI_TUNING,
    SETTINGS_SCALE,
    SETTINGS_PROFILES,
    SETTINGS_EEPROM,
    SETTINGS_SERVO,
    SETTINGS_DISPLAY,
    SETTINGS_REBOOT,
    SETTINGS_VERSION,
    SETTINGS_BACK,
} settings_item_t;

// Back button callback
static void btn_back_cb(lv_event_t *e) {
    (void)e;
    ui_show_main_menu();
}

// List item callbacks
static void list_item_cb(lv_event_t *e) {
    settings_item_t item = (settings_item_t)(intptr_t)lv_event_get_user_data(e);

    switch (item) {
        case SETTINGS_AI_TUNING:
            // Show AI auto-tuning screen
            ui_show_ai_tuning();
            break;

        case SETTINGS_SCALE:
            // Show scale settings
            ui_show_toast("Scale Settings - Coming Soon", 2000);
            break;

        case SETTINGS_PROFILES:
            // Show profile manager
            ui_show_toast("Profile Manager - Coming Soon", 2000);
            break;

        case SETTINGS_EEPROM:
            // Show EEPROM options
            ui_show_warning("Save Settings",
                           "Save current settings to EEPROM?",
                           []() {
                               eeprom_save_all();
                               ui_show_toast("Settings saved!", 2000);
                           },
                           NULL);
            break;

        case SETTINGS_SERVO:
            // Toggle servo gate
            {
                servo_gate_state_t state = servo_gate_get_state();
                if (state == SERVO_GATE_STATE_DISABLE) {
                    servo_gate_set_state(SERVO_GATE_STATE_OPEN);
                    ui_show_toast("Servo Gate: Open", 1500);
                } else if (state == SERVO_GATE_STATE_OPEN) {
                    servo_gate_set_state(SERVO_GATE_STATE_CLOSE);
                    ui_show_toast("Servo Gate: Closed", 1500);
                } else {
                    servo_gate_set_state(SERVO_GATE_STATE_DISABLE);
                    ui_show_toast("Servo Gate: Disabled", 1500);
                }
            }
            break;

        case SETTINGS_DISPLAY:
            // Show display settings
            ui_show_toast("Display Settings - Coming Soon", 2000);
            break;

        case SETTINGS_REBOOT:
            // Confirm reboot
            ui_show_warning("Reboot",
                           "Reboot the device?",
                           []() {
                               extern AppState_t exit_state;
                               exit_state = APP_STATE_ENTER_REBOOT;
                               extern QueueHandle_t encoder_event_queue;
                               ButtonEncoderEvent_t event = BUTTON_RST_PRESSED;
                               xQueueSend(encoder_event_queue, &event, 0);
                           },
                           NULL);
            break;

        case SETTINGS_VERSION:
            // Show version info
            {
                char version_msg[128];
                snprintf(version_msg, sizeof(version_msg),
                         "Version: %s\nBuild: %s",
                         APP_VERSION_STRING, BUILD_TYPE);
                ui_show_warning("Version Info", version_msg, NULL, NULL);
            }
            break;

        case SETTINGS_BACK:
            ui_show_main_menu();
            break;
    }
}

// Create settings screen
void ui_settings_create(void) {
    if (settings_screen) {
        lv_obj_del(settings_screen);
    }

    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, UI_COLOR_BG, 0);

    // Title bar
    ui_create_titlebar(settings_screen, "Settings");

    // Settings list
    settings_list = lv_list_create(settings_screen);
    lv_obj_set_size(settings_list, UI_SCREEN_WIDTH - 40, 210);
    lv_obj_align(settings_list, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_bg_color(settings_list, UI_COLOR_PANEL, 0);
    lv_obj_set_style_border_width(settings_list, 0, 0);
    lv_obj_set_style_radius(settings_list, 8, 0);

    // Add menu items with icons
    struct {
        const char *icon;
        const char *text;
        settings_item_t item;
    } menu_items[] = {
        {LV_SYMBOL_CHARGE,    "AI Auto-Tuning",  SETTINGS_AI_TUNING},
        {LV_SYMBOL_DOWNLOAD,  "Scale",           SETTINGS_SCALE},
        {LV_SYMBOL_LIST,      "Profile Manager", SETTINGS_PROFILES},
        {LV_SYMBOL_SAVE,      "Save to EEPROM",  SETTINGS_EEPROM},
        {LV_SYMBOL_POWER,     "Servo Gate",      SETTINGS_SERVO},
        {LV_SYMBOL_IMAGE,     "Display",         SETTINGS_DISPLAY},
        {LV_SYMBOL_LOOP,      "Reboot",          SETTINGS_REBOOT},
        {LV_SYMBOL_HOME,      "Version Info",    SETTINGS_VERSION},
    };

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        lv_obj_t *btn = lv_list_add_btn(settings_list, menu_items[i].icon, menu_items[i].text);
        lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)menu_items[i].item);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PANEL, 0);
        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT, 0);
    }

    // Back button - clean
    lv_obj_t *btn_back = ui_create_button(settings_screen, "BACK", btn_back_cb);
    lv_obj_set_size(btn_back, 100, 44);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 20, -15);
}

void ui_settings_show(void) {
    ui_settings_create();
    lv_scr_load(settings_screen);
}

// Public interface
void ui_show_settings(void) {
    ui_settings_show();
}

// ============ Warning/Message Box Screen ============

static lv_obj_t *msgbox = NULL;
static void (*confirm_callback)(void) = NULL;
static void (*cancel_callback)(void) = NULL;

static void msgbox_event_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target(e);
    const char *btn_text = lv_msgbox_get_active_btn_text(obj);

    if (btn_text) {
        if (strcmp(btn_text, "OK") == 0 || strcmp(btn_text, "Yes") == 0) {
            if (confirm_callback) {
                confirm_callback();
            }
        } else if (strcmp(btn_text, "Cancel") == 0 || strcmp(btn_text, "No") == 0) {
            if (cancel_callback) {
                cancel_callback();
            }
        }
    }

    lv_msgbox_close(obj);
    msgbox = NULL;
    confirm_callback = NULL;
    cancel_callback = NULL;
}

void ui_show_warning(const char *title, const char *message, void (*on_confirm)(void), void (*on_cancel)(void)) {
    if (msgbox) {
        lv_msgbox_close(msgbox);
    }

    confirm_callback = on_confirm;
    cancel_callback = on_cancel;

    static const char *btns_confirm_cancel[] = {"Yes", "No", ""};
    static const char *btns_ok[] = {"OK", ""};

    const char **btns = (on_confirm && on_cancel) ? btns_confirm_cancel :
                        (on_confirm) ? btns_confirm_cancel : btns_ok;

    msgbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(msgbox, title);
    lv_msgbox_add_text(msgbox, message);
    lv_msgbox_add_close_btn(msgbox);

    if (on_confirm || on_cancel) {
        lv_obj_t *btn_yes = lv_msgbox_add_footer_btn(msgbox, "Yes");
        lv_obj_add_event_cb(btn_yes, msgbox_event_cb, LV_EVENT_CLICKED, NULL);

        if (on_cancel) {
            lv_obj_t *btn_no = lv_msgbox_add_footer_btn(msgbox, "No");
            lv_obj_add_event_cb(btn_no, msgbox_event_cb, LV_EVENT_CLICKED, NULL);
        }
    } else {
        lv_obj_t *btn_ok = lv_msgbox_add_footer_btn(msgbox, "OK");
        lv_obj_add_event_cb(btn_ok, msgbox_event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_center(msgbox);
}

// ============ Cleanup Warning Screen ============

void ui_show_cleanup_warning(void) {
    ui_show_warning("Warning",
                   "Put pan on the scale and press Yes to start cleanup mode.",
                   []() {
                       extern AppState_t exit_state;
                       exit_state = APP_STATE_ENTER_CLEANUP_MODE;
                       extern QueueHandle_t encoder_event_queue;
                       ButtonEncoderEvent_t event = OVERRIDE_FROM_REST;
                       xQueueSend(encoder_event_queue, &event, 0);
                   },
                   []() {
                       ui_show_main_menu();
                   });
}

// ============ Charge Warning Screen ============

void ui_show_charge_warning(void) {
    ui_show_warning("Warning",
                   "Put pan on the scale and press Yes to start charging.",
                   []() {
                       extern AppState_t exit_state;
                       exit_state = APP_STATE_ENTER_CHARGE_MODE;
                       extern QueueHandle_t encoder_event_queue;
                       ButtonEncoderEvent_t event = OVERRIDE_FROM_REST;
                       xQueueSend(encoder_event_queue, &event, 0);
                   },
                   []() {
                       ui_show_main_menu();
                   });
}

// ============ Wireless Screen ============

void ui_show_wireless(void) {
    extern char wifi_ip_address[];
    extern char wifi_ssid[];

    char info[128];
    snprintf(info, sizeof(info), "SSID: %s\nIP: %s", wifi_ssid, wifi_ip_address);

    ui_show_warning("WiFi Info", info, NULL, NULL);
}
