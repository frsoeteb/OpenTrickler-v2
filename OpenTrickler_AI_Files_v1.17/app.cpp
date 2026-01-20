/*
 * OpenTrickler v1.17 - Full firmware with WiFi
 */

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"
#include "lwip/apps/httpd.h"

#include "FreeRTOSConfig.h"
#include "configuration.h"

#include "rest_endpoints.h"
#include "wireless.h"
#include "error.h"
#include "eeprom.h"
#include "neopixel_led.h"
#include "scale.h"
#include "charge_mode.h"
#include "profile.h"
#include "servo_gate.h"
#include "mini_12864_module.h"
#include "display_config.h"
#ifdef USE_COLOR_TFT
#include "tft35/tft35_module.h"
#endif
#include "menu.h"
#include "motors.h"

extern "C" {
#include "dhcpserver.h"
#include "dnsserver.h"
}

// WiFi AP settings (fallback)
#define WIFI_AP_SSID "OpenTrickler"
#define WIFI_AP_PASSWORD "opentrickler"

// Global for DHCP/DNS servers
static dhcp_server_t dhcp_server;
static dns_server_t dns_server;
static bool connected_to_home = false;
static bool need_reboot = false;

// Simple task for WiFi maintenance and reboot handling
void simple_wifi_task(void *params) {
    (void)params;
    printf("Simple WiFi task started\n");
    while (1) {
        if (need_reboot) {
            printf("Rebooting...\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            watchdog_reboot(0, 0, 0);
            while(1);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static uint32_t get_cyw43_auth(uint32_t auth_method) {
    switch (auth_method) {
        case 0: return CYW43_AUTH_OPEN;
        case 1: return CYW43_AUTH_WPA_TKIP_PSK;
        case 2: return CYW43_AUTH_WPA2_AES_PSK;
        case 3: return CYW43_AUTH_WPA2_MIXED_PSK;
        default: return CYW43_AUTH_WPA2_AES_PSK;
    }
}

int main()
{
    stdio_init_all();
    error_system_init();

    printf("\n=== OpenTrickler v1.17 ===\n");

    bool has_saved_creds = wireless_config_init();

    if (cyw43_arch_init()) {
        printf("WiFi init FAILED!\n");
        report_error(ERR_WIFI_INIT_FAIL);
    } else {
        printf("WiFi init OK\n");

        connected_to_home = false;
        if (has_saved_creds && home_wifi_enabled && strlen(home_ssid) > 0) {
            printf("Connecting to home WiFi: %s\n", home_ssid);
            cyw43_arch_enable_sta_mode();
            int result = cyw43_arch_wifi_connect_timeout_ms(
                home_ssid, home_password,
                get_cyw43_auth(home_auth_method), home_timeout_ms);
            if (result == 0) {
                printf("Connected! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
                connected_to_home = true;
            } else {
                printf("Failed (err %d), starting AP\n", result);
                cyw43_arch_disable_sta_mode();
            }
        }

        if (!connected_to_home) {
            printf("Starting AP: %s\n", WIFI_AP_SSID);
            cyw43_arch_enable_ap_mode(WIFI_AP_SSID, WIFI_AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

            ip4_addr_t gw, mask;
            IP4_ADDR(&gw, 192, 168, 4, 1);
            IP4_ADDR(&mask, 255, 255, 255, 0);
            dhcp_server_init(&dhcp_server, &gw, &mask);
            dns_server_init(&dns_server, &gw);
        }

        rest_endpoints_init(connected_to_home);
        httpd_init();
        printf("HTTP server started\n");
    }

    // EEPROM init
    if (!eeprom_init()) {
        printf("EEPROM init FAILED!\n");
        report_error(ERR_EEPROM_I2C_INIT);
    } else {
        printf("EEPROM init OK\n");
        error_set_eeprom_ready(true);
    }

    // Neopixel init
    if (!neopixel_led_init()) {
        printf("Neopixel init FAILED!\n");
        report_error(ERR_NEOPIXEL_PIO_INIT);
    } else {
        printf("Neopixel init OK\n");
        error_set_neopixel_ready(true);
    }

    // Display config init (load display type, rotation, brightness from EEPROM)
    if (!display_config_init()) {
        printf("Display config init FAILED!\n");
    } else {
        printf("Display config init OK\n");
    }

    // Display init based on configured display type
    display_type_t display_type = display_config_get_type();
#ifdef USE_COLOR_TFT
    if (display_type == DISPLAY_TYPE_TFT35 || display_type == DISPLAY_TYPE_TFT43) {
        // TFT35/TFT43 display init
        tft35_module_init();
        // Also init rotary encoder (shared with Mini 12864)
        button_init();
        printf("TFT35/TFT43 init OK\n");
    } else
#endif
    {
        // Mini 12864 display init (includes button_init and display_init)
        if (!mini_12864_module_init()) {
            printf("Mini 12864 init FAILED!\n");
            report_error(ERR_DISPLAY_MUTEX_CREATE);
        } else {
            printf("Mini 12864 init OK\n");
        }
    }

    // Scale init
    if (!scale_init()) {
        printf("Scale init FAILED!\n");
        report_error(ERR_SCALE_UART_INIT);
    } else {
        printf("Scale init OK\n");
    }

    // Profile init
    if (!profile_data_init()) {
        printf("Profile init FAILED!\n");
        report_error(ERR_PROFILE_EEPROM_READ);
    } else {
        printf("Profile init OK\n");
    }

    // Charge mode init
    if (!charge_mode_config_init()) {
        printf("Charge mode init FAILED!\n");
        report_error(ERR_CHARGE_EEPROM_READ);
    } else {
        printf("Charge mode init OK\n");
    }

    // Servo init
    if (!servo_gate_init()) {
        printf("Servo init FAILED!\n");
        report_error(ERR_SERVO_TASK_CREATE);
    } else {
        printf("Servo init OK\n");
    }

    error_set_wifi_ready(true);

    // Create FreeRTOS tasks
    // WiFi task MUST be pinned to core 0 for stability with cyw43 driver
    TaskHandle_t wifi_task_handle;
    xTaskCreate(simple_wifi_task, "WiFi Task", 1024, NULL, 1, &wifi_task_handle);
    vTaskCoreAffinitySet(wifi_task_handle, (1 << 0));  // Pin to core 0

    // Create UI task based on display type
#ifdef USE_COLOR_TFT
    if (display_type == DISPLAY_TYPE_TFT35 || display_type == DISPLAY_TYPE_TFT43) {
        xTaskCreate(tft35_lvgl_task, "LVGL Task", 4096, NULL, 2, NULL);
    } else
#endif
    {
        xTaskCreate(menu_task, "Menu Task", 4096, NULL, 2, NULL);
    }
    xTaskCreate(motor_init_task, "Motor Init", 2048, NULL, 3, NULL);

    printf("Starting FreeRTOS scheduler\n");
    vTaskStartScheduler();

    while (true);
}
