#!/usr/bin/env python3
"""
OTA Firmware Upload Script for OpenTrickler

Usage:
    python ota_upload.py <firmware.bin> [--host 192.168.4.1] [--apply]

Example:
    python ota_upload.py build/app.bin --host 192.168.4.1 --apply
"""

import argparse
import base64
import hashlib
import requests
import sys
import time

CHUNK_SIZE = 1024  # 1KB chunks (will be ~1.4KB base64 encoded)


def calculate_sha256(data: bytes) -> str:
    """Calculate SHA256 hash of data and return as hex string."""
    return hashlib.sha256(data).hexdigest()


def upload_firmware(host: str, firmware_path: str, apply: bool = False) -> bool:
    """Upload firmware to device via OTA REST API."""

    base_url = f"http://{host}"

    # Read firmware file
    print(f"Reading firmware: {firmware_path}")
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()

    firmware_size = len(firmware_data)
    sha256_hash = calculate_sha256(firmware_data)

    print(f"Firmware size: {firmware_size} bytes")
    print(f"SHA256: {sha256_hash}")

    # Check current OTA status
    print("\nChecking device status...")
    try:
        resp = requests.get(f"{base_url}/rest/ota_status", timeout=5)
        status = resp.json()
        print(f"Current state: {status.get('state', 'unknown')}")

        if status.get('state') not in ['idle', 'error']:
            print("Device is busy with another OTA. Aborting first...")
            requests.get(f"{base_url}/rest/ota_abort", timeout=5)
            time.sleep(1)
    except Exception as e:
        print(f"Warning: Could not get status: {e}")

    # Begin OTA
    print(f"\nStarting OTA upload...")
    try:
        resp = requests.get(
            f"{base_url}/rest/ota_begin",
            params={"size": firmware_size, "sha256": sha256_hash},
            timeout=30
        )
        result = resp.json()
        if "error" in result:
            print(f"Error starting OTA: {result['error']}")
            return False
        print("OTA started successfully")
    except Exception as e:
        print(f"Error starting OTA: {e}")
        return False

    # Upload chunks
    print(f"\nUploading firmware in {CHUNK_SIZE} byte chunks...")
    offset = 0
    start_time = time.time()

    while offset < firmware_size:
        chunk = firmware_data[offset:offset + CHUNK_SIZE]
        chunk_b64 = base64.b64encode(chunk).decode('ascii')

        try:
            resp = requests.get(
                f"{base_url}/rest/ota_write",
                params={"data": chunk_b64},
                timeout=10
            )
            result = resp.json()

            if "error" in result:
                print(f"\nError writing chunk at {offset}: {result['error']}")
                return False

            progress = result.get('progress', 0)
            received = result.get('received', 0)

            # Progress bar
            bar_len = 40
            filled = int(bar_len * progress / 100)
            bar = '=' * filled + '-' * (bar_len - filled)
            elapsed = time.time() - start_time
            speed = received / elapsed / 1024 if elapsed > 0 else 0

            print(f"\r[{bar}] {progress}% ({received}/{firmware_size}) {speed:.1f} KB/s", end='')

        except Exception as e:
            print(f"\nError writing chunk at {offset}: {e}")
            return False

        offset += CHUNK_SIZE

    elapsed = time.time() - start_time
    print(f"\n\nUpload complete in {elapsed:.1f}s ({firmware_size/elapsed/1024:.1f} KB/s)")

    # Verify
    print("\nVerifying firmware...")
    try:
        resp = requests.get(f"{base_url}/rest/ota_end", timeout=30)
        result = resp.json()

        if "error" in result:
            print(f"Verification failed: {result['error']}")
            return False

        print("Verification successful!")
    except Exception as e:
        print(f"Error during verification: {e}")
        return False

    # Apply if requested
    if apply:
        print("\nApplying update (device will reboot)...")
        try:
            resp = requests.get(f"{base_url}/rest/ota_apply", timeout=5)
            print("Update applied! Device is rebooting...")
        except:
            # Connection will likely drop during reboot
            print("Device is rebooting...")

        # Wait for device to come back
        print("\nWaiting for device to come back online...")
        for i in range(30):
            time.sleep(2)
            try:
                resp = requests.get(f"{base_url}/rest/ota_status", timeout=3)
                print(f"Device is back online!")
                return True
            except:
                print(".", end='', flush=True)

        print("\nDevice did not respond after reboot. Check manually.")
    else:
        print("\nFirmware ready to apply. Run with --apply to install, or call:")
        print(f"  curl '{base_url}/rest/ota_apply'")

    return True


def main():
    parser = argparse.ArgumentParser(description="Upload firmware via OTA")
    parser.add_argument("firmware", help="Path to firmware .bin file")
    parser.add_argument("--host", default="192.168.4.1", help="Device IP address (default: 192.168.4.1)")
    parser.add_argument("--apply", action="store_true", help="Apply update after upload")

    args = parser.parse_args()

    success = upload_firmware(args.host, args.firmware, args.apply)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
