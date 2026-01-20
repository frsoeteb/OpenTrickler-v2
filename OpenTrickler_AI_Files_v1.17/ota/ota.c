/**
 * Simple OTA Implementation for Pico W / Pico 2W
 */

#include "ota.h"
#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

// RP2350 has hardware SHA256, RP2040 does not
#if defined(PICO_RP2350) || defined(RASPBERRYPI_PICO2_W)
#include "pico/sha256.h"
#define HAS_HW_SHA256 1
#else
#define HAS_HW_SHA256 0
#endif

// Flash layout for Pico W / Pico 2W
// Total flash: 2MB (Pico W) or 4MB (Pico 2W)
// We use the upper half for OTA staging
#define FLASH_SIZE_BYTES        (PICO_FLASH_SIZE_BYTES)
#define OTA_STAGING_OFFSET      (FLASH_SIZE_BYTES / 2)  // Start at 1MB or 2MB mark
#define OTA_MAX_FIRMWARE_SIZE   (FLASH_SIZE_BYTES / 2 - FLASH_SECTOR_SIZE)  // Leave one sector margin

// Flash addresses (XIP base + offset)
#define FLASH_XIP_BASE          0x10000000
#define OTA_STAGING_ADDR        (FLASH_XIP_BASE + OTA_STAGING_OFFSET)

// Write buffer (must be multiple of FLASH_PAGE_SIZE = 256)
#define OTA_WRITE_BUFFER_SIZE   4096
static uint8_t write_buffer[OTA_WRITE_BUFFER_SIZE] __attribute__((aligned(4)));
static size_t write_buffer_pos = 0;

// OTA state
static ota_state_t ota_state = OTA_STATE_IDLE;
static uint32_t ota_total_size = 0;
static uint32_t ota_received_size = 0;
static uint32_t ota_flash_offset = 0;
static uint8_t ota_expected_sha256[32];
static bool ota_verify_checksum = false;
static ota_status_t ota_last_error = OTA_OK;

// SHA256 context
#if HAS_HW_SHA256
static pico_sha256_state_t sha256_state;
#endif

// Forward declarations
static ota_status_t flush_write_buffer(void);
static void flash_write_callback(void *param);

void ota_init(void) {
    ota_state = OTA_STATE_IDLE;
    ota_total_size = 0;
    ota_received_size = 0;
    ota_flash_offset = 0;
    ota_verify_checksum = false;
    ota_last_error = OTA_OK;
    write_buffer_pos = 0;

    printf("[OTA] Initialized. Flash size: %u KB, staging at offset 0x%X\n",
           FLASH_SIZE_BYTES / 1024, OTA_STAGING_OFFSET);
}

ota_status_t ota_begin(uint32_t firmware_size, const uint8_t *expected_sha256) {
    if (ota_state != OTA_STATE_IDLE && ota_state != OTA_STATE_ERROR) {
        return OTA_ERROR_ALREADY_IN_PROGRESS;
    }

    if (firmware_size == 0 || firmware_size > OTA_MAX_FIRMWARE_SIZE) {
        printf("[OTA] Invalid firmware size: %u (max %u)\n", firmware_size, OTA_MAX_FIRMWARE_SIZE);
        return OTA_ERROR_INVALID_SIZE;
    }

    // Store expected checksum if provided
    if (expected_sha256) {
        memcpy(ota_expected_sha256, expected_sha256, 32);
        ota_verify_checksum = true;
    } else {
        ota_verify_checksum = false;
    }

    // Initialize state
    ota_total_size = firmware_size;
    ota_received_size = 0;
    ota_flash_offset = 0;
    write_buffer_pos = 0;
    ota_last_error = OTA_OK;

    // Initialize SHA256
#if HAS_HW_SHA256
    if (ota_verify_checksum) {
        pico_sha256_start_blocking(&sha256_state, SHA256_BIG_ENDIAN, true);
    }
#endif

    // Calculate sectors to erase (round up)
    uint32_t sectors_to_erase = (firmware_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;

    printf("[OTA] Starting update: %u bytes, erasing %u sectors\n",
           firmware_size, sectors_to_erase);

    // Erase staging area using flash_safe_execute
    uint32_t erase_size = sectors_to_erase * FLASH_SECTOR_SIZE;

    // Erase must be done with interrupts disabled and from RAM
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(OTA_STAGING_OFFSET, erase_size);
    restore_interrupts(ints);

    ota_state = OTA_STATE_RECEIVING;
    printf("[OTA] Ready to receive firmware\n");

    return OTA_OK;
}

ota_status_t ota_write(const uint8_t *data, size_t len) {
    if (ota_state != OTA_STATE_RECEIVING) {
        return OTA_ERROR_NOT_INITIALIZED;
    }

    if (ota_received_size + len > ota_total_size) {
        len = ota_total_size - ota_received_size;  // Truncate to expected size
    }

    if (len == 0) {
        return OTA_OK;
    }

    // Update SHA256
#if HAS_HW_SHA256
    if (ota_verify_checksum) {
        pico_sha256_update_blocking(&sha256_state, data, len);
    }
#endif

    // Copy to write buffer
    size_t remaining = len;
    const uint8_t *src = data;

    while (remaining > 0) {
        size_t to_copy = OTA_WRITE_BUFFER_SIZE - write_buffer_pos;
        if (to_copy > remaining) {
            to_copy = remaining;
        }

        memcpy(write_buffer + write_buffer_pos, src, to_copy);
        write_buffer_pos += to_copy;
        src += to_copy;
        remaining -= to_copy;
        ota_received_size += to_copy;

        // Flush buffer when full
        if (write_buffer_pos >= OTA_WRITE_BUFFER_SIZE) {
            ota_status_t status = flush_write_buffer();
            if (status != OTA_OK) {
                ota_state = OTA_STATE_ERROR;
                ota_last_error = status;
                return status;
            }
        }
    }

    return OTA_OK;
}

static ota_status_t flush_write_buffer(void) {
    if (write_buffer_pos == 0) {
        return OTA_OK;
    }

    // Pad to page boundary if needed
    size_t write_size = (write_buffer_pos + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
    if (write_size > write_buffer_pos) {
        memset(write_buffer + write_buffer_pos, 0xFF, write_size - write_buffer_pos);
    }

    // Write to flash
    uint32_t flash_addr = OTA_STAGING_OFFSET + ota_flash_offset;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(flash_addr, write_buffer, write_size);
    restore_interrupts(ints);

    // Verify write
    const uint8_t *verify_ptr = (const uint8_t *)(FLASH_XIP_BASE + flash_addr);
    if (memcmp(verify_ptr, write_buffer, write_buffer_pos) != 0) {
        printf("[OTA] Flash verify failed at offset 0x%X\n", flash_addr);
        return OTA_ERROR_FLASH_WRITE;
    }

    ota_flash_offset += write_size;
    write_buffer_pos = 0;

    // Progress update every 64KB
    if ((ota_flash_offset % 65536) == 0) {
        printf("[OTA] Progress: %u / %u bytes (%u%%)\n",
               ota_received_size, ota_total_size,
               (ota_received_size * 100) / ota_total_size);
    }

    return OTA_OK;
}

ota_status_t ota_end(void) {
    if (ota_state != OTA_STATE_RECEIVING) {
        return OTA_ERROR_NOT_INITIALIZED;
    }

    // Flush remaining data
    ota_status_t status = flush_write_buffer();
    if (status != OTA_OK) {
        ota_state = OTA_STATE_ERROR;
        ota_last_error = status;
        return status;
    }

    printf("[OTA] Received %u bytes, verifying...\n", ota_received_size);
    ota_state = OTA_STATE_VERIFYING;

    // Finalize SHA256
#if HAS_HW_SHA256
    if (ota_verify_checksum) {
        sha256_result_t result;
        pico_sha256_finish(&sha256_state, &result);

        if (memcmp(result.bytes, ota_expected_sha256, 32) != 0) {
            printf("[OTA] SHA256 mismatch!\n");
            printf("[OTA] Expected: ");
            for (int i = 0; i < 32; i++) printf("%02x", ota_expected_sha256[i]);
            printf("\n[OTA] Computed: ");
            for (int i = 0; i < 32; i++) printf("%02x", result.bytes[i]);
            printf("\n");

            ota_state = OTA_STATE_ERROR;
            ota_last_error = OTA_ERROR_CHECKSUM_MISMATCH;
            return OTA_ERROR_CHECKSUM_MISMATCH;
        }

        printf("[OTA] SHA256 verified OK\n");
    }
#else
    if (ota_verify_checksum) {
        printf("[OTA] Warning: SHA256 verification not available on RP2040\n");
    }
#endif

    ota_state = OTA_STATE_READY_TO_APPLY;
    printf("[OTA] Firmware ready to apply. Call ota_apply() to install.\n");

    return OTA_OK;
}

ota_status_t ota_apply(void) {
    if (ota_state != OTA_STATE_READY_TO_APPLY) {
        return OTA_ERROR_NOT_READY;
    }

    printf("[OTA] *** APPLYING UPDATE - DO NOT POWER OFF ***\n");
    ota_state = OTA_STATE_APPLYING;

    // Calculate size to copy (round up to sector)
    uint32_t copy_size = (ota_received_size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);

    printf("[OTA] Copying %u bytes from staging (0x%X) to main (0x0)...\n",
           copy_size, OTA_STAGING_OFFSET);

    // This is the dangerous part - we're overwriting our own code
    // Disable interrupts and copy sector by sector
    uint32_t ints = save_and_disable_interrupts();

    for (uint32_t offset = 0; offset < copy_size; offset += FLASH_SECTOR_SIZE) {
        // Erase destination sector
        flash_range_erase(offset, FLASH_SECTOR_SIZE);

        // Copy from staging
        const uint8_t *src = (const uint8_t *)(FLASH_XIP_BASE + OTA_STAGING_OFFSET + offset);
        flash_range_program(offset, src, FLASH_SECTOR_SIZE);
    }

    restore_interrupts(ints);

    printf("[OTA] Update applied. Rebooting...\n");

    // Small delay to let printf flush
    sleep_ms(100);

    // Reboot using watchdog
    watchdog_reboot(0, 0, 0);

    // Should not reach here
    while (1) {
        tight_loop_contents();
    }

    return OTA_OK;  // Never reached
}

void ota_abort(void) {
    printf("[OTA] Aborted\n");
    ota_state = OTA_STATE_IDLE;
    ota_total_size = 0;
    ota_received_size = 0;
    write_buffer_pos = 0;
}

ota_progress_t ota_get_progress(void) {
    ota_progress_t progress = {
        .state = ota_state,
        .total_size = ota_total_size,
        .received_size = ota_received_size,
        .progress_percent = (ota_total_size > 0) ?
            (uint8_t)((ota_received_size * 100) / ota_total_size) : 0,
        .last_error = ota_last_error,
    };
    return progress;
}

const char* ota_status_str(ota_status_t status) {
    switch (status) {
        case OTA_OK: return "OK";
        case OTA_ERROR_NOT_INITIALIZED: return "Not initialized";
        case OTA_ERROR_ALREADY_IN_PROGRESS: return "Already in progress";
        case OTA_ERROR_INVALID_SIZE: return "Invalid size";
        case OTA_ERROR_FLASH_ERASE: return "Flash erase failed";
        case OTA_ERROR_FLASH_WRITE: return "Flash write failed";
        case OTA_ERROR_CHECKSUM_MISMATCH: return "Checksum mismatch";
        case OTA_ERROR_NOT_READY: return "Not ready";
        case OTA_ERROR_APPLY_FAILED: return "Apply failed";
        default: return "Unknown";
    }
}
