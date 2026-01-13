/**
 * WiFi configuration page with SSID display and config form
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcpserver.h"
#include "dnsserver.h"
#include "wifi_config.h"

#define TCP_PORT 80
#define DEBUG_printf printf
#define POLL_TIME_S 5
#define HTTP_GET "GET"
#define HTTP_RESPONSE_HEADERS "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"
#define LED_TEST "/ledtest"
#define WIFI_CONFIG "/wificonfig"
#define FORGET_WIFI "/forgetwifi"
#define LED_GPIO 0
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s" LED_TEST "\n\n"

// Flash storage configuration (using official Raspberry Pi approach)
// RP2350 has 4MB flash, use last sector for config storage
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_MAGIC 0x57494649  // "WIFI" in hex

// WiFi credentials structure for flash storage
typedef struct {
    uint32_t magic;           // Magic number to verify valid config
    char home_ssid[33];       // Home WiFi SSID
    char home_password[64];   // Home WiFi password
    uint32_t checksum;        // Simple checksum for validation
} wifi_credentials_t;

// HTML template with home WiFi config, AP config, and LED control
#define PAGE_TEMPLATE "<html><head><style>body{font-family:Arial;margin:40px;}h1{color:#333;}.info{background:#f0f0f0;padding:15px;margin:10px 0;border-radius:5px;}.led{margin:20px 0;}form{background:#e8f4f8;padding:20px;border-radius:5px;margin:20px 0;}input{padding:8px;margin:5px 0;width:200px;}button{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin-right:10px;}button:hover{background:#45a049;}button.danger{background:#f44336;}button.danger:hover{background:#da190b;}</style></head><body><h1>OpenTrickler WiFi Config</h1><div class='info'><strong>Current Mode:</strong> %s<br><strong>Current SSID:</strong> %s<br>%s</div><div class='led'><p>LED is %s</p><p><a href=\"?led=%d\"><button>Turn LED %s</button></a></p></div><form action='/wificonfig' method='get'><h3>Home WiFi Settings</h3><p>Configure your home WiFi network. Device will try to connect to this network on boot.</p><label>Home SSID:</label><br><input type='text' name='home_ssid' maxlength='32' value='%s' placeholder='Enter home WiFi SSID'><br><label>Home Password:</label><br><input type='password' name='home_pass' minlength='8' maxlength='63' placeholder='Enter home WiFi password'><br><br><button type='submit'>Save & Reboot</button></form><p><a href='/forgetwifi'><button class='danger'>Forget Home WiFi</button></a></p><form action='/wificonfig' method='get'><h3>Access Point Settings</h3><p>Change the fallback Access Point credentials.</p><label>AP SSID:</label><br><input type='text' name='ap_ssid' maxlength='32' value='%s' required><br><label>AP Password:</label><br><input type='password' name='ap_pass' minlength='8' maxlength='63' placeholder='Enter new AP password'><br><br><button type='submit'>Update AP</button></form></body></html>"

#define WIFI_CONFIRM_PAGE "<html><head><meta http-equiv='refresh' content='10;url=/'></head><body><h1>WiFi Settings Saved!</h1><p>%s</p><p>Device is rebooting... Please wait 10 seconds then reconnect.</p></body></html>"

#define FORGET_CONFIRM_PAGE "<html><head><meta http-equiv='refresh' content='5;url=/'></head><body><h1>Home WiFi Forgotten</h1><p>Device is rebooting in AP mode...</p><p>Connect to AP: %s</p></body></html>"

// Global WiFi config
static char current_ssid[33];        // Current AP SSID
static char current_password[64];    // Current AP password
static char home_ssid[33];           // Home WiFi SSID
static char home_password[64];       // Home WiFi password
static bool wifi_config_changed = false;
static bool need_reboot = false;
static bool connected_to_home = false;

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[512];
    char result[2048];
    int header_len;
    int result_len;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;

// URL decode helper
static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if (*src == '%' && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Flash storage functions (based on official Raspberry Pi pico-examples/flash/program)

// Calculate simple checksum
static uint32_t calculate_checksum(const wifi_credentials_t *creds) {
    uint32_t sum = creds->magic;
    for (int i = 0; i < sizeof(creds->home_ssid); i++) {
        sum += creds->home_ssid[i];
    }
    for (int i = 0; i < sizeof(creds->home_password); i++) {
        sum += creds->home_password[i];
    }
    return sum;
}

// Read WiFi credentials from flash
static bool read_wifi_credentials(wifi_credentials_t *creds) {
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(creds, flash_target_contents, sizeof(wifi_credentials_t));

    // Verify magic and checksum
    if (creds->magic != CONFIG_MAGIC) {
        DEBUG_printf("No valid WiFi config in flash (bad magic: 0x%08x)\n", creds->magic);
        return false;
    }

    uint32_t expected_checksum = calculate_checksum(creds);
    if (creds->checksum != expected_checksum) {
        DEBUG_printf("WiFi config checksum mismatch\n");
        return false;
    }

    DEBUG_printf("Valid WiFi config found in flash\n");
    return true;
}

// Flash erase callback (based on official Raspberry Pi example)
static void flash_erase_callback(void *param) {
    uint32_t offset = (uint32_t)(uintptr_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

// Flash program callback (based on official Raspberry Pi example)
static void flash_program_callback(void *param) {
    void **params = (void **)param;
    uint32_t offset = (uint32_t)(uintptr_t)params[0];
    const uint8_t *data = (const uint8_t *)params[1];
    flash_range_program(offset, data, FLASH_PAGE_SIZE);
}

// Write WiFi credentials to flash (using official Raspberry Pi approach)
static bool write_wifi_credentials(const wifi_credentials_t *creds) {
    wifi_credentials_t creds_to_write = *creds;
    creds_to_write.magic = CONFIG_MAGIC;
    creds_to_write.checksum = calculate_checksum(&creds_to_write);

    DEBUG_printf("Writing WiFi credentials to flash...\n");

    // Disable interrupts during flash operations (official approach)
    uint32_t ints = save_and_disable_interrupts();

    // Erase the flash sector
    flash_safe_execute(flash_erase_callback, (void *)(uintptr_t)FLASH_TARGET_OFFSET, UINT32_MAX);

    // Write data in FLASH_PAGE_SIZE chunks
    uint8_t buffer[FLASH_PAGE_SIZE] __attribute__((aligned(4))) = {0};
    size_t bytes_to_write = sizeof(wifi_credentials_t);
    size_t offset = 0;

    while (bytes_to_write > 0) {
        size_t chunk_size = (bytes_to_write < FLASH_PAGE_SIZE) ? bytes_to_write : FLASH_PAGE_SIZE;
        memset(buffer, 0, FLASH_PAGE_SIZE);
        memcpy(buffer, ((uint8_t *)&creds_to_write) + offset, chunk_size);

        void *params[2] = {
            (void *)(uintptr_t)(FLASH_TARGET_OFFSET + offset),
            (void *)buffer
        };
        flash_safe_execute(flash_program_callback, params, UINT32_MAX);

        offset += chunk_size;
        bytes_to_write -= chunk_size;
    }

    restore_interrupts(ints);

    DEBUG_printf("WiFi credentials written successfully\n");
    return true;
}

// Erase WiFi credentials from flash
static bool erase_wifi_credentials(void) {
    DEBUG_printf("Erasing WiFi credentials from flash...\n");

    uint32_t ints = save_and_disable_interrupts();
    flash_safe_execute(flash_erase_callback, (void *)(uintptr_t)FLASH_TARGET_OFFSET, UINT32_MAX);
    restore_interrupts(ints);

    DEBUG_printf("WiFi credentials erased\n");
    return true;
}

static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err) {
    if (client_pcb) {
        assert(con_state && con_state->pcb == client_pcb);
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
        if (con_state) {
            free(con_state);
        }
    }
    return close_err;
}

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    DEBUG_printf("tcp_server_sent %u\n", len);
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        DEBUG_printf("all done\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

static int test_server_content(const char *request, const char *params, char *result, size_t max_result_len) {
    int len = 0;

    // Handle "Forget WiFi" request
    if (strncmp(request, FORGET_WIFI, sizeof(FORGET_WIFI) - 1) == 0) {
        erase_wifi_credentials();
        len = snprintf(result, max_result_len, FORGET_CONFIRM_PAGE, current_ssid);
        need_reboot = true;
        return len;
    }

    // Handle WiFi config page
    if (strncmp(request, WIFI_CONFIG, sizeof(WIFI_CONFIG) - 1) == 0) {
        if (params) {
            char new_home_ssid[33] = {0};
            char new_home_pass[64] = {0};
            char new_ap_ssid[33] = {0};
            char new_ap_pass[64] = {0};

            char *home_ssid_param = strstr(params, "home_ssid=");
            char *home_pass_param = strstr(params, "home_pass=");
            char *ap_ssid_param = strstr(params, "ap_ssid=");
            char *ap_pass_param = strstr(params, "ap_pass=");

            // Handle home WiFi configuration
            if (home_ssid_param && home_pass_param) {
                // Extract home SSID
                home_ssid_param += 10; // skip "home_ssid="
                char *home_ssid_end = strchr(home_ssid_param, '&');
                if (!home_ssid_end) home_ssid_end = strchr(home_ssid_param, ' ');
                if (home_ssid_end) {
                    size_t ssid_len = home_ssid_end - home_ssid_param;
                    if (ssid_len < sizeof(new_home_ssid)) {
                        strncpy(new_home_ssid, home_ssid_param, ssid_len);
                    }
                } else {
                    strncpy(new_home_ssid, home_ssid_param, sizeof(new_home_ssid) - 1);
                }
                url_decode(new_home_ssid, new_home_ssid);

                // Extract home password
                home_pass_param += 10; // skip "home_pass="
                char *home_pass_end = strchr(home_pass_param, '&');
                if (!home_pass_end) home_pass_end = strchr(home_pass_param, ' ');
                if (home_pass_end) {
                    size_t pass_len = home_pass_end - home_pass_param;
                    if (pass_len < sizeof(new_home_pass)) {
                        strncpy(new_home_pass, home_pass_param, pass_len);
                    }
                } else {
                    strncpy(new_home_pass, home_pass_param, sizeof(new_home_pass) - 1);
                }
                url_decode(new_home_pass, new_home_pass);

                // Save to flash and reboot if valid
                if (strlen(new_home_ssid) > 0 && strlen(new_home_pass) >= 8) {
                    wifi_credentials_t creds = {0};
                    strncpy(creds.home_ssid, new_home_ssid, sizeof(creds.home_ssid) - 1);
                    strncpy(creds.home_password, new_home_pass, sizeof(creds.home_password) - 1);

                    if (write_wifi_credentials(&creds)) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Home WiFi saved: %s", new_home_ssid);
                        len = snprintf(result, max_result_len, WIFI_CONFIRM_PAGE, msg);
                        need_reboot = true;
                        DEBUG_printf("Home WiFi credentials saved to flash: %s\n", new_home_ssid);
                    }
                }
            }
            // Handle AP configuration
            else if (ap_ssid_param && ap_pass_param) {
                // Extract AP SSID
                ap_ssid_param += 8; // skip "ap_ssid="
                char *ap_ssid_end = strchr(ap_ssid_param, '&');
                if (!ap_ssid_end) ap_ssid_end = strchr(ap_ssid_param, ' ');
                if (ap_ssid_end) {
                    size_t ssid_len = ap_ssid_end - ap_ssid_param;
                    if (ssid_len < sizeof(new_ap_ssid)) {
                        strncpy(new_ap_ssid, ap_ssid_param, ssid_len);
                    }
                } else {
                    strncpy(new_ap_ssid, ap_ssid_param, sizeof(new_ap_ssid) - 1);
                }
                url_decode(new_ap_ssid, new_ap_ssid);

                // Extract AP password
                ap_pass_param += 8; // skip "ap_pass="
                char *ap_pass_end = strchr(ap_pass_param, '&');
                if (!ap_pass_end) ap_pass_end = strchr(ap_pass_param, ' ');
                if (ap_pass_end) {
                    size_t pass_len = ap_pass_end - ap_pass_param;
                    if (pass_len < sizeof(new_ap_pass)) {
                        strncpy(new_ap_pass, ap_pass_param, pass_len);
                    }
                } else {
                    strncpy(new_ap_pass, ap_pass_param, sizeof(new_ap_pass) - 1);
                }
                url_decode(new_ap_pass, new_ap_pass);

                // Update AP credentials if valid
                if (strlen(new_ap_ssid) > 0 && strlen(new_ap_pass) >= 8) {
                    strncpy(current_ssid, new_ap_ssid, sizeof(current_ssid) - 1);
                    strncpy(current_password, new_ap_pass, sizeof(current_password) - 1);
                    wifi_config_changed = true;

                    char msg[128];
                    snprintf(msg, sizeof(msg), "AP credentials updated: %s", current_ssid);
                    len = snprintf(result, max_result_len, WIFI_CONFIRM_PAGE, msg);
                    DEBUG_printf("AP config updated: SSID=%s\n", current_ssid);
                }
            }
        }
    }
    // Handle LED test page
    else if (strncmp(request, LED_TEST, sizeof(LED_TEST) - 1) == 0 || strcmp(request, "/") == 0) {
        // Get the state of the led
        bool value;
        cyw43_gpio_get(&cyw43_state, LED_GPIO, &value);
        int led_state = value;

        // See if the user changed it
        if (params) {
            if (sscanf(params, "led=%d", &led_state) == 1) {
                cyw43_gpio_set(&cyw43_state, LED_GPIO, led_state ? true : false);
            }
        }

        // Build the status info
        const char *mode_str = connected_to_home ? "Home WiFi Client" : "Access Point";
        const char *ssid_str = connected_to_home ? home_ssid : current_ssid;
        char ip_info[128] = {0};
        if (connected_to_home) {
            snprintf(ip_info, sizeof(ip_info), "<strong>IP Address:</strong> %s",
                     ip4addr_ntoa(netif_ip4_addr(netif_list)));
        } else {
            snprintf(ip_info, sizeof(ip_info), "<strong>AP IP:</strong> 192.168.4.1");
        }

        // Display home SSID if saved (or empty if not)
        const char *saved_home_ssid = (strlen(home_ssid) > 0) ? home_ssid : "";

        // Generate result
        if (led_state) {
            len = snprintf(result, max_result_len, PAGE_TEMPLATE,
                          mode_str, ssid_str, ip_info, "ON", 0, "OFF",
                          saved_home_ssid, current_ssid);
        } else {
            len = snprintf(result, max_result_len, PAGE_TEMPLATE,
                          mode_str, ssid_str, ip_info, "OFF", 1, "ON",
                          saved_home_ssid, current_ssid);
        }
    }
    return len;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (!p) {
        DEBUG_printf("connection closed\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    assert(con_state && con_state->pcb == pcb);
    if (p->tot_len > 0) {
        DEBUG_printf("tcp_server_recv %d err %d\n", p->tot_len, err);

        // Copy the request into the buffer
        pbuf_copy_partial(p, con_state->headers, p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len, 0);

        // Handle GET request
        if (strncmp(HTTP_GET, con_state->headers, sizeof(HTTP_GET) - 1) == 0) {
            char *request = con_state->headers + sizeof(HTTP_GET); // + space
            char *params = strchr(request, '?');
            if (params) {
                if (*params) {
                    char *space = strchr(request, ' ');
                    *params++ = 0;
                    if (space) {
                        *space = 0;
                    }
                } else {
                    params = NULL;
                }
            }

            // Generate content
            con_state->result_len = test_server_content(request, params, con_state->result, sizeof(con_state->result));
            DEBUG_printf("Request: %s?%s\n", request, params);
            DEBUG_printf("Result: %d\n", con_state->result_len);

            // Check we had enough buffer space
            if (con_state->result_len > sizeof(con_state->result) - 1) {
                DEBUG_printf("Too much result data %d\n", con_state->result_len);
                return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
            }

            // Generate web page
            if (con_state->result_len > 0) {
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS,
                    200, con_state->result_len);
                if (con_state->header_len > sizeof(con_state->headers) - 1) {
                    DEBUG_printf("Too much header data %d\n", con_state->header_len);
                    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
                }
            } else {
                // Send redirect
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_REDIRECT,
                    ipaddr_ntoa(con_state->gw));
                DEBUG_printf("Sending redirect %s", con_state->headers);
            }

            // Send the headers to the client
            con_state->sent_len = 0;
            err_t err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                DEBUG_printf("failed to write header data %d\n", err);
                return tcp_close_client_connection(con_state, pcb, err);
            }

            // Send the body to the client
            if (con_state->result_len) {
                err = tcp_write(pcb, con_state->result, con_state->result_len, 0);
                if (err != ERR_OK) {
                    DEBUG_printf("failed to write result data %d\n", err);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    DEBUG_printf("tcp_server_poll_fn\n");
    return tcp_close_client_connection(con_state, pcb, ERR_OK);
}

static void tcp_server_err(void *arg, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (err != ERR_ABRT) {
        DEBUG_printf("tcp_client_err_fn %d\n", err);
        tcp_close_client_connection(con_state, con_state->pcb, err);
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        DEBUG_printf("failure in accept\n");
        return ERR_VAL;
    }
    DEBUG_printf("client connected\n");

    // Create the state for the connection
    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state) {
        DEBUG_printf("failed to allocate connect state\n");
        return ERR_MEM;
    }
    con_state->pcb = client_pcb;
    con_state->gw = &state->gw;

    // setup connection to client
    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

static bool tcp_server_open(void *arg, const char *ap_name) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    DEBUG_printf("starting server on port %d\n", TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    if (err) {
        DEBUG_printf("failed to bind to port %d\n",TCP_PORT);
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        DEBUG_printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    printf("Try connecting to '%s' (press 'd' to disable access point)\n", ap_name);
    return true;
}

void key_pressed_func(void *param) {
    assert(param);
    TCP_SERVER_T *state = (TCP_SERVER_T*)param;
    int key = getchar_timeout_us(0);
    if (key == 'd' || key == 'D') {
        cyw43_arch_lwip_begin();
        cyw43_arch_disable_ap_mode();
        cyw43_arch_lwip_end();
        state->complete = true;
    }
}

int main() {
    stdio_init_all();

    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return 1;
    }

    if (cyw43_arch_init()) {
        DEBUG_printf("failed to initialise\n");
        return 1;
    }

    // Initialize AP config from defaults
    strncpy(current_ssid, WIFI_AP_SSID, sizeof(current_ssid) - 1);
    strncpy(current_password, WIFI_AP_PASSWORD, sizeof(current_password) - 1);

    // Try to read saved home WiFi credentials from flash
    wifi_credentials_t saved_creds = {0};
    bool has_saved_creds = read_wifi_credentials(&saved_creds);

    if (has_saved_creds) {
        strncpy(home_ssid, saved_creds.home_ssid, sizeof(home_ssid) - 1);
        strncpy(home_password, saved_creds.home_password, sizeof(home_password) - 1);
        printf("Found saved home WiFi credentials: %s\n", home_ssid);
    } else {
        home_ssid[0] = '\0';
        home_password[0] = '\0';
        printf("No saved home WiFi credentials found\n");
    }

    // Get notified if the user presses a key
    stdio_set_chars_available_callback(key_pressed_func, state);

    // Try to connect to home WiFi first (if saved credentials exist)
    connected_to_home = false;
    if (has_saved_creds && strlen(home_ssid) > 0) {
        printf("Attempting to connect to home WiFi: %s\n", home_ssid);
        cyw43_arch_enable_sta_mode();

        int connect_result = cyw43_arch_wifi_connect_timeout_ms(
            home_ssid, home_password, CYW43_AUTH_WPA2_AES_PSK, 30000);

        if (connect_result == 0) {
            printf("Connected to home WiFi successfully!\n");
            printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
            connected_to_home = true;
        } else {
            printf("Failed to connect to home WiFi (error %d), falling back to AP mode\n", connect_result);
            cyw43_arch_lwip_begin();
            cyw43_arch_disable_sta_mode();
            cyw43_arch_lwip_end();
        }
    }

    // If not connected to home WiFi, enable AP mode
    if (!connected_to_home) {
        printf("Starting Access Point mode: %s\n", current_ssid);
        cyw43_arch_enable_ap_mode(current_ssid, current_password, CYW43_AUTH_WPA2_AES_PSK);
    }

    // DHCP and DNS servers only needed in AP mode
    dhcp_server_t dhcp_server;
    dns_server_t dns_server;

    if (!connected_to_home) {
        #if LWIP_IPV6
        #define IP(x) ((x).u_addr.ip4)
        #else
        #define IP(x) (x)
        #endif

        ip4_addr_t mask;
        IP(state->gw).addr = PP_HTONL(CYW43_DEFAULT_IP_AP_ADDRESS);
        IP(mask).addr = PP_HTONL(CYW43_DEFAULT_IP_MASK);

        #undef IP

        // Start the dhcp server
        dhcp_server_init(&dhcp_server, &state->gw, &mask);

        // Start the dns server
        dns_server_init(&dns_server, &state->gw);
    } else {
        // In client mode, use the router's IP as gateway
        state->gw = *netif_ip4_gw(netif_list);
    }

    if (!tcp_server_open(state, current_ssid)) {
        DEBUG_printf("failed to open server\n");
        return 1;
    }

    state->complete = false;
    while(!state->complete) {
        // Check if reboot is needed (after saving/forgetting credentials)
        if (need_reboot) {
            printf("Rebooting to apply new WiFi settings...\n");
            sleep_ms(3000);  // Give time for HTTP response to be sent

            // Clean up
            tcp_server_close(state);
            if (!connected_to_home) {
                dns_server_deinit(&dns_server);
                dhcp_server_deinit(&dhcp_server);
            }
            cyw43_arch_deinit();

            // Trigger watchdog reset to reboot
            watchdog_enable(1, 1);
            while(1);
        }

        // Check if WiFi config changed (AP credentials only)
        if (wifi_config_changed) {
            printf("AP config changed, restarting AP with new settings...\n");
            wifi_config_changed = false;

            // Give time for response to be sent
            sleep_ms(2000);

            // Restart WiFi with new settings
            tcp_server_close(state);
            cyw43_arch_lwip_begin();
            cyw43_arch_disable_ap_mode();
            sleep_ms(1000);
            cyw43_arch_enable_ap_mode(current_ssid, current_password, CYW43_AUTH_WPA2_AES_PSK);
            cyw43_arch_lwip_end();
            sleep_ms(1000);

            // Restart server
            if (!tcp_server_open(state, current_ssid)) {
                DEBUG_printf("failed to reopen server\n");
                break;
            }
        }

#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
#else
        sleep_ms(1000);
#endif
    }
    tcp_server_close(state);
    if (!connected_to_home) {
        dns_server_deinit(&dns_server);
        dhcp_server_deinit(&dhcp_server);
    }
    cyw43_arch_deinit();
    printf("Test complete\n");
    return 0;
}
