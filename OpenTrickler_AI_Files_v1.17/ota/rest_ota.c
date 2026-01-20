/**
 * REST API implementation for OTA firmware update
 */

#include "rest_ota.h"
#include "ota.h"
#include "http_rest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Response buffer
static char ota_response_buf[512];

// Base64 decoding table
static const int8_t b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

// Decode base64, returns decoded length or -1 on error
static int base64_decode(const char *in, size_t in_len, uint8_t *out, size_t out_max) {
    size_t out_len = 0;
    uint32_t accum = 0;
    int bits = 0;

    for (size_t i = 0; i < in_len; i++) {
        char c = in[i];
        if (c == '=') break;  // Padding
        if (c == ' ' || c == '\n' || c == '\r') continue;  // Skip whitespace

        int8_t val = b64_decode_table[(uint8_t)c];
        if (val < 0) return -1;  // Invalid char

        accum = (accum << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (out_len >= out_max) return -1;  // Output overflow
            out[out_len++] = (accum >> bits) & 0xFF;
        }
    }

    return (int)out_len;
}

// Decode hex string to bytes
static int hex_decode(const char *hex, uint8_t *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) return -1;

    for (size_t i = 0; i < out_len; i++) {
        char buf[3] = {hex[i*2], hex[i*2+1], 0};
        char *end;
        long val = strtol(buf, &end, 16);
        if (*end != '\0') return -1;
        out[i] = (uint8_t)val;
    }
    return 0;
}

// Find parameter value by name
static const char* find_param(const char *name, int num_params, char *params[], char *values[]) {
    for (int i = 0; i < num_params; i++) {
        if (strcmp(params[i], name) == 0) {
            return values[i];
        }
    }
    return NULL;
}

void rest_ota_init(void) {
    ota_init();

    rest_register_handler("/rest/ota_status", http_rest_ota_status);
    rest_register_handler("/rest/ota_begin", http_rest_ota_begin);
    rest_register_handler("/rest/ota_write", http_rest_ota_write);
    rest_register_handler("/rest/ota_end", http_rest_ota_end);
    rest_register_handler("/rest/ota_apply", http_rest_ota_apply);
    rest_register_handler("/rest/ota_abort", http_rest_ota_abort);

    printf("[OTA] REST endpoints registered\n");
}

bool http_rest_ota_status(struct fs_file *file, int num_params, char *params[], char *values[]) {
    ota_progress_t progress = ota_get_progress();

    const char *state_str;
    switch (progress.state) {
        case OTA_STATE_IDLE: state_str = "idle"; break;
        case OTA_STATE_RECEIVING: state_str = "receiving"; break;
        case OTA_STATE_VERIFYING: state_str = "verifying"; break;
        case OTA_STATE_READY_TO_APPLY: state_str = "ready"; break;
        case OTA_STATE_APPLYING: state_str = "applying"; break;
        case OTA_STATE_ERROR: state_str = "error"; break;
        default: state_str = "unknown"; break;
    }

    int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n"
        "{\"state\":\"%s\",\"progress\":%u,\"received\":%u,\"total\":%u,\"error\":\"%s\"}",
        state_str, progress.progress_percent, progress.received_size,
        progress.total_size, ota_status_str(progress.last_error));

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ota_begin(struct fs_file *file, int num_params, char *params[], char *values[]) {
    const char *size_str = find_param("size", num_params, params, values);
    const char *sha256_str = find_param("sha256", num_params, params, values);

    if (!size_str) {
        int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"Missing size parameter\"}");
        file->data = ota_response_buf;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    uint32_t firmware_size = (uint32_t)strtoul(size_str, NULL, 10);

    uint8_t sha256[32];
    uint8_t *sha256_ptr = NULL;
    if (sha256_str && strlen(sha256_str) == 64) {
        if (hex_decode(sha256_str, sha256, 32) == 0) {
            sha256_ptr = sha256;
        }
    }

    ota_status_t status = ota_begin(firmware_size, sha256_ptr);

    int len;
    if (status == OTA_OK) {
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"status\":\"ok\",\"max_chunk\":2048}");
    } else {
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"%s\"}", ota_status_str(status));
    }

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ota_write(struct fs_file *file, int num_params, char *params[], char *values[]) {
    const char *data_str = find_param("data", num_params, params, values);

    if (!data_str) {
        int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"Missing data parameter\"}");
        file->data = ota_response_buf;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    // Decode base64 data
    static uint8_t decode_buf[3072];  // Max ~2KB decoded from ~2.7KB base64
    int decoded_len = base64_decode(data_str, strlen(data_str), decode_buf, sizeof(decode_buf));

    if (decoded_len < 0) {
        int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"Invalid base64 data\"}");
        file->data = ota_response_buf;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    ota_status_t status = ota_write(decode_buf, decoded_len);

    int len;
    if (status == OTA_OK) {
        ota_progress_t progress = ota_get_progress();
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"status\":\"ok\",\"received\":%u,\"progress\":%u}",
            progress.received_size, progress.progress_percent);
    } else {
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"%s\"}", ota_status_str(status));
    }

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ota_end(struct fs_file *file, int num_params, char *params[], char *values[]) {
    ota_status_t status = ota_end();

    int len;
    if (status == OTA_OK) {
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"status\":\"ok\",\"message\":\"Firmware verified. Call /rest/ota_apply to install.\"}");
    } else {
        len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"%s\"}", ota_status_str(status));
    }

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}

bool http_rest_ota_apply(struct fs_file *file, int num_params, char *params[], char *values[]) {
    ota_progress_t progress = ota_get_progress();

    if (progress.state != OTA_STATE_READY_TO_APPLY) {
        int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
            "{\"error\":\"Not ready to apply. Current state: %d\"}", progress.state);
        file->data = ota_response_buf;
        file->len = len;
        file->index = len;
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return true;
    }

    // Send response before applying (device will reboot)
    int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n"
        "{\"status\":\"applying\",\"message\":\"Device will reboot...\"}");

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    // Schedule apply after response is sent (use a small delay)
    // Note: This is a simplification - ideally use a FreeRTOS task/timer
    // For now, the apply happens immediately which may cut off the response
    ota_apply();  // This will reboot

    return true;
}

bool http_rest_ota_abort(struct fs_file *file, int num_params, char *params[], char *values[]) {
    ota_abort();

    int len = snprintf(ota_response_buf, sizeof(ota_response_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n"
        "{\"status\":\"ok\",\"message\":\"OTA aborted\"}");

    file->data = ota_response_buf;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}
