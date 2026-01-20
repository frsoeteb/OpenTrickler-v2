/**
 * REST API endpoints for OTA firmware update
 */

#ifndef REST_OTA_H
#define REST_OTA_H

#include "http_rest.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize OTA REST endpoints
 * Call after rest_endpoints_init()
 */
void rest_ota_init(void);

/**
 * GET /rest/ota_status - Get OTA status and progress
 * Response: {"state": "idle|receiving|verifying|ready|applying|error",
 *            "progress": 0-100, "received": bytes, "total": bytes, "error": "..."}
 */
bool http_rest_ota_status(struct fs_file *file, int num_params, char *params[], char *values[]);

/**
 * GET /rest/ota_begin?size=BYTES[&sha256=HEX] - Start OTA update
 * Parameters:
 *   size: Firmware size in bytes (required)
 *   sha256: Expected SHA256 hash as 64 hex chars (optional)
 * Response: {"status": "ok"} or {"error": "..."}
 */
bool http_rest_ota_begin(struct fs_file *file, int num_params, char *params[], char *values[]);

/**
 * GET /rest/ota_write?data=BASE64 - Write firmware chunk
 * Parameters:
 *   data: Base64 encoded firmware chunk (max 2KB decoded per call)
 * Response: {"status": "ok", "received": total_bytes} or {"error": "..."}
 *
 * Note: For larger transfers, use multiple calls or implement POST handler
 */
bool http_rest_ota_write(struct fs_file *file, int num_params, char *params[], char *values[]);

/**
 * GET /rest/ota_end - Finish upload and verify
 * Response: {"status": "ok"} or {"error": "checksum mismatch", ...}
 */
bool http_rest_ota_end(struct fs_file *file, int num_params, char *params[], char *values[]);

/**
 * GET /rest/ota_apply - Apply update and reboot
 * WARNING: Device will reboot! No response if successful.
 * Response on error: {"error": "..."}
 */
bool http_rest_ota_apply(struct fs_file *file, int num_params, char *params[], char *values[]);

/**
 * GET /rest/ota_abort - Abort current OTA update
 * Response: {"status": "ok"}
 */
bool http_rest_ota_abort(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif

#endif // REST_OTA_H
