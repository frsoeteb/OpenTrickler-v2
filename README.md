# OpenTrickler v2

Tested and working on Pico 2 W

## Version History

**v2.0** – Introduced AI tuning.

**v2.1** – Fixed dropdown lists to correctly display profile names.

**v2.2** – Updated Wi-Fi configuration to persist settings in flash memory.

**v2.3** – Eliminated silent failures by adding clear error messages.

**v2.4** – Updated and refined the web pages.

## Getting Started

Users can now flash the firmware and have the entire application load in AP mode. From there, they can configure their own Wi-Fi and immediately navigate the app.

As additional hardware is connected, error conditions will naturally reduce.

**Important:** Always add or remove hardware while the Pico is powered off / disconnected.

## Flashing the Firmware

1. Download `app_v2.4.0.uf2` from this repository
2. Hold the BOOTSEL button on your Pico 2 W
3. While holding BOOTSEL, connect the Pico to your computer via USB
4. Release BOOTSEL - the Pico will appear as a USB drive called "RPI-RP2"
5. Drag and drop `app_v2.4.0.uf2` onto the drive
6. The Pico will automatically reboot and start the application

## Download Latest Build from GitHub

No need to build yourself - GitHub automatically builds the firmware:

1. Go to the [Actions tab](https://github.com/CavemansToys/OpenTrickler-v2/actions)
2. Click on the latest successful build (green checkmark)
3. Scroll down to "Artifacts"
4. Download "firmware" - this contains `app.uf2`
5. Flash as described above

## Building Locally (Optional)

### Prerequisites
- CMake 3.25+
- Ninja
- ARM GCC toolchain (`gcc-arm-none-eabi`)

### Build Steps

```bash
git clone --recursive https://github.com/CavemansToys/OpenTrickler-v2.git
cd OpenTrickler-v2
mkdir build && cd build
cmake -G Ninja -DPICO_BOARD=pico2_w ..
ninja
```

The firmware will be at `build/app.uf2`
