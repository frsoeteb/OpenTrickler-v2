/**
 * Simple OTA (Over-The-Air) Firmware Update for Pico W / Pico 2W
 *
 * This module provides basic OTA functionality without requiring a bootloader
 * or partition table. Firmware is written to a staging area in flash, verified
 * with SHA256, then copied to the main application area.
 *
 * WARNING: If power is lost during the final copy, the device will need
 * USB recovery. For production use, consider pico_fota_bootloader instead.
 */

#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA status codes
typedef enum {
    OTA_OK = 0,
    OTA_ERROR_NOT_INITIALIZED,
    OTA_ERROR_ALREADY_IN_PROGRESS,
    OTA_ERROR_INVALID_SIZE,
    OTA_ERROR_FLASH_ERASE,
    OTA_ERROR_FLASH_WRITE,
    OTA_ERROR_CHECKSUM_MISMATCH,
    OTA_ERROR_NOT_READY,
    OTA_ERROR_APPLY_FAILED,
} ota_status_t;

// OTA state
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_RECEIVING,
    OTA_STATE_VERIFYING,
    OTA_STATE_READY_TO_APPLY,
    OTA_STATE_APPLYING,
    OTA_STATE_ERROR,
} ota_state_t;

// OTA progress info
typedef struct {
    ota_state_t state;
    uint32_t total_size;
    uint32_t received_size;
    uint8_t progress_percent;
    ota_status_t last_error;
} ota_progress_t;

/**
 * Initialize OTA subsystem
 * Call once at startup
 */
void ota_init(void);

/**
 * Begin OTA update
 * @param firmware_size Total size of firmware in bytes
 * @param expected_sha256 Expected SHA256 hash (32 bytes), or NULL to skip verification
 * @return OTA_OK on success
 */
ota_status_t ota_begin(uint32_t firmware_size, const uint8_t *expected_sha256);

/**
 * Write firmware chunk
 * @param data Pointer to data
 * @param len Length of data
 * @return OTA_OK on success
 */
ota_status_t ota_write(const uint8_t *data, size_t len);

/**
 * Finish receiving and verify checksum
 * @return OTA_OK if verification passed
 */
ota_status_t ota_end(void);

/**
 * Apply the update (copy to main flash and reboot)
 * WARNING: This is the dangerous part - do not interrupt!
 * @return Does not return on success, returns error code on failure
 */
ota_status_t ota_apply(void);

/**
 * Abort current OTA update
 */
void ota_abort(void);

/**
 * Get current OTA progress
 * @return Progress info struct
 */
ota_progress_t ota_get_progress(void);

/**
 * Get string description of status code
 */
const char* ota_status_str(ota_status_t status);

#ifdef __cplusplus
}
#endif

#endif // OTA_H
