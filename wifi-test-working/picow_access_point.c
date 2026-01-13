/**
 * WiFi configuration page with SSID display and config form
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

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
#define LED_GPIO 0
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s" LED_TEST "\n\n"

// HTML template with SSID display and config form
#define PAGE_TEMPLATE "<html><head><style>body{font-family:Arial;margin:40px;}h1{color:#333;}.info{background:#f0f0f0;padding:15px;margin:10px 0;border-radius:5px;}.led{margin:20px 0;}form{background:#e8f4f8;padding:20px;border-radius:5px;margin:20px 0;}input{padding:8px;margin:5px 0;width:200px;}button{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;}button:hover{background:#45a049;}</style></head><body><h1>OpenTrickler WiFi Config</h1><div class='info'><strong>Current SSID:</strong> %s</div><div class='led'><p>LED is %s</p><p><a href=\"?led=%d\"><button>Turn LED %s</button></a></p></div><form action='/wificonfig' method='get'><h3>Change WiFi Settings</h3><label>New SSID:</label><br><input type='text' name='ssid' maxlength='32' required><br><label>New Password:</label><br><input type='password' name='pass' minlength='8' maxlength='63' required><br><br><button type='submit'>Update WiFi</button></form></body></html>"

#define WIFI_CONFIRM_PAGE "<html><head><meta http-equiv='refresh' content='5;url=/'></head><body><h1>WiFi Updated!</h1><p>New SSID: %s</p><p>Restarting WiFi... Device will reconnect in a few seconds.</p><p>You will be redirected automatically.</p></body></html>"

// Global WiFi config
static char current_ssid[33];
static char current_password[64];
static bool wifi_config_changed = false;

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[512];
    char result[1024];
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

    // Handle WiFi config page
    if (strncmp(request, WIFI_CONFIG, sizeof(WIFI_CONFIG) - 1) == 0) {
        if (params) {
            char new_ssid[33] = {0};
            char new_pass[64] = {0};
            char *ssid_param = strstr(params, "ssid=");
            char *pass_param = strstr(params, "pass=");

            if (ssid_param && pass_param) {
                // Extract SSID
                ssid_param += 5; // skip "ssid="
                char *ssid_end = strchr(ssid_param, '&');
                if (ssid_end) {
                    size_t ssid_len = ssid_end - ssid_param;
                    if (ssid_len < sizeof(new_ssid)) {
                        strncpy(new_ssid, ssid_param, ssid_len);
                        url_decode(new_ssid, new_ssid);
                    }
                }

                // Extract password
                pass_param += 5; // skip "pass="
                char *pass_end = strchr(pass_param, '&');
                if (!pass_end) pass_end = strchr(pass_param, ' ');
                if (pass_end) {
                    size_t pass_len = pass_end - pass_param;
                    if (pass_len < sizeof(new_pass)) {
                        strncpy(new_pass, pass_param, pass_len);
                        url_decode(new_pass, new_pass);
                    }
                } else {
                    strncpy(new_pass, pass_param, sizeof(new_pass) - 1);
                    url_decode(new_pass, new_pass);
                }

                if (strlen(new_ssid) > 0 && strlen(new_pass) >= 8) {
                    strncpy(current_ssid, new_ssid, sizeof(current_ssid) - 1);
                    strncpy(current_password, new_pass, sizeof(current_password) - 1);
                    wifi_config_changed = true;

                    len = snprintf(result, max_result_len, WIFI_CONFIRM_PAGE, current_ssid);
                    DEBUG_printf("WiFi config updated: SSID=%s\n", current_ssid);
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
            char led_param_str[32];
            if (sscanf(params, "led=%d", &led_state) == 1) {
                cyw43_gpio_set(&cyw43_state, LED_GPIO, led_state ? true : false);
            }
        }

        // Generate result with current SSID
        if (led_state) {
            len = snprintf(result, max_result_len, PAGE_TEMPLATE, current_ssid, "ON", 0, "OFF");
        } else {
            len = snprintf(result, max_result_len, PAGE_TEMPLATE, current_ssid, "OFF", 1, "ON");
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

    // Initialize WiFi config from defaults
    strncpy(current_ssid, WIFI_AP_SSID, sizeof(current_ssid) - 1);
    strncpy(current_password, WIFI_AP_PASSWORD, sizeof(current_password) - 1);

    // Get notified if the user presses a key
    stdio_set_chars_available_callback(key_pressed_func, state);

    cyw43_arch_enable_ap_mode(current_ssid, current_password, CYW43_AUTH_WPA3_WPA2_AES_PSK);

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
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state->gw, &mask);

    // Start the dns server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &state->gw);

    if (!tcp_server_open(state, current_ssid)) {
        DEBUG_printf("failed to open server\n");
        return 1;
    }

    state->complete = false;
    while(!state->complete) {
        // Check if WiFi config changed
        if (wifi_config_changed) {
            printf("WiFi config changed, restarting AP with new settings...\n");
            wifi_config_changed = false;

            // Give time for response to be sent
            sleep_ms(2000);

            // Restart WiFi with new settings
            tcp_server_close(state);
            cyw43_arch_lwip_begin();
            cyw43_arch_disable_ap_mode();
            sleep_ms(1000);
            cyw43_arch_enable_ap_mode(current_ssid, current_password, CYW43_AUTH_WPA3_WPA2_AES_PSK);
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
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_deinit();
    printf("Test complete\n");
    return 0;
}
