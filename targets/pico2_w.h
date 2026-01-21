/**
 * Pico 2W (RP2350) Board Configuration
 *
 * The Pico 2W has the same pinout as Pico W, but with:
 * - RP2350 chip (dual Cortex-M33 @ 150MHz vs RP2040's dual M0+ @ 133MHz)
 * - Hardware FPU (single precision)
 * - DSP instructions
 * - 520KB RAM (vs 264KB)
 * - Optional RISC-V cores
 */

#ifndef PICO2_W_H_
#define PICO2_W_H_

// Use same pin configuration as Pico W
#include "raspberrypi_pico_w_config.h"

// RP2350-specific optimizations
#ifdef PICO_RP2350

// Can use higher SPI frequencies on RP2350 due to faster CPU
#undef TFT_SPI_FREQ_HZ
#define TFT_SPI_FREQ_HZ     (62500000)  // 62.5MHz (max for RP2350 SPI)

#endif // PICO_RP2350

#endif // PICO2_W_H_
