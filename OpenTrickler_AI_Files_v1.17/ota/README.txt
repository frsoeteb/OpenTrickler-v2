Simple OTA (Over-The-Air) Update for OpenTrickler
=================================================

WARNING: This is a simple OTA implementation without a bootloader.
If power is lost during the apply phase, the device will need USB recovery.
For production use, consider pico_fota_bootloader instead.

How It Works
------------
1. New firmware is uploaded to the upper half of flash (staging area)
2. SHA256 checksum is verified
3. On apply, firmware is copied to main flash area and device reboots

Flash Layout (4MB Pico 2W)
--------------------------
0x00000000 - 0x001FFFFF : Main application (2MB)
0x00200000 - 0x003FFFFF : OTA staging area (2MB)

Flash Layout (2MB Pico W)
-------------------------
0x00000000 - 0x000FFFFF : Main application (1MB)
0x00100000 - 0x001FFFFF : OTA staging area (1MB)

Integration
-----------
1. Add to main CMakeLists.txt (after other add_subdirectory calls):

   option(USE_OTA "Enable OTA firmware updates" OFF)
   if(USE_OTA)
       add_subdirectory(ota)
       target_link_libraries("${TARGET_NAME}" ota)
   endif()

2. In rest_endpoints.c, add:

   #ifdef USE_OTA
   #include "ota/rest_ota.h"
   #endif

   In rest_endpoints_init():
   #ifdef USE_OTA
       rest_ota_init();
   #endif

3. Build with OTA enabled:

   cmake -B build -G Ninja -DPICO_BOARD=pico2_w -DUSE_OTA=ON
   ninja -C build

REST API
--------
GET /rest/ota_status
    Returns: {"state":"idle|receiving|verifying|ready|applying|error",
              "progress":0-100, "received":bytes, "total":bytes}

GET /rest/ota_begin?size=BYTES&sha256=HEX64
    Start OTA. size is required, sha256 is optional but recommended.

GET /rest/ota_write?data=BASE64
    Write firmware chunk (max ~2KB decoded per call)

GET /rest/ota_end
    Finish upload and verify SHA256

GET /rest/ota_apply
    Apply update and reboot (WARNING: no response if successful)

GET /rest/ota_abort
    Cancel current OTA

Upload Script
-------------
Use the included Python script:

    pip install requests
    python ota_upload.py build/app.bin --host 192.168.4.1 --apply

Or manually with curl:

    # Start OTA
    curl "http://192.168.4.1/rest/ota_begin?size=727584&sha256=abc123..."

    # Upload chunks (base64 encoded)
    curl "http://192.168.4.1/rest/ota_write?data=BASE64DATA..."

    # Verify
    curl "http://192.168.4.1/rest/ota_end"

    # Apply
    curl "http://192.168.4.1/rest/ota_apply"

Troubleshooting
---------------
- "Invalid size": Firmware too large for staging area
- "Already in progress": Call /rest/ota_abort first
- "Checksum mismatch": Firmware corrupted during transfer
- Device doesn't respond after apply: USB recovery needed

For a safer OTA with rollback support, see:
https://github.com/JZimnol/pico_fota_bootloader
