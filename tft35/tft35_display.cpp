/**
 * @file tft35_display.cpp
 * @brief TFT35 Display Driver Implementation
 *
 * ST7796/ILI9488 display driver with runtime auto-detection.
 */

#include "tft35_display.h"
#include "configuration.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "semphr.h"

// SPI and GPIO configuration (from raspberrypi_pico_w_config.h when USE_TFT35 defined)
#ifndef TFT35_SPI
#define TFT35_SPI           spi0
#define TFT35_SCK_PIN       18
#define TFT35_MOSI_PIN      19
#define TFT35_MISO_PIN      16
#define TFT35_CS_PIN        17
#define TFT35_DC_PIN        20
#define TFT35_RST_PIN       21
#endif

// SPI clock speed
#define TFT35_SPI_FREQ_HZ   (40 * 1000 * 1000)  // 40MHz for display

// Display commands (common to ST7796 and ILI9488)
#define CMD_NOP             0x00
#define CMD_SWRESET         0x01
#define CMD_RDDID           0x04
#define CMD_SLPIN           0x10
#define CMD_SLPOUT          0x11
#define CMD_INVOFF          0x20
#define CMD_INVON           0x21
#define CMD_DISPOFF         0x28
#define CMD_DISPON          0x29
#define CMD_CASET           0x2A
#define CMD_RASET           0x2B
#define CMD_RAMWR           0x2C
#define CMD_MADCTL          0x36
#define CMD_COLMOD          0x3A

// MADCTL bits
#define MADCTL_MY           0x80
#define MADCTL_MX           0x40
#define MADCTL_MV           0x20
#define MADCTL_ML           0x10
#define MADCTL_BGR          0x08
#define MADCTL_MH           0x04

// Static variables
static tft35_controller_t detected_controller = TFT35_CONTROLLER_UNKNOWN;
static tft35_rotation_t current_rotation = TFT35_ROTATION_0;
static lv_display_t *lvgl_display = NULL;
static int dma_channel = -1;
static SemaphoreHandle_t spi_mutex = NULL;

// Forward declarations
static void write_cmd(uint8_t cmd);
static void write_data(const uint8_t *data, size_t len);
static void write_data_byte(uint8_t data);
static void init_st7796(void);
static void init_ili9488(void);
static uint32_t read_display_id(void);

// CS control
static inline void cs_select(void) {
    gpio_put(TFT35_CS_PIN, 0);
}

static inline void cs_deselect(void) {
    gpio_put(TFT35_CS_PIN, 1);
}

// DC control (0 = command, 1 = data)
static inline void dc_command(void) {
    gpio_put(TFT35_DC_PIN, 0);
}

static inline void dc_data(void) {
    gpio_put(TFT35_DC_PIN, 1);
}

// Write a command byte
static void write_cmd(uint8_t cmd) {
    dc_command();
    cs_select();
    spi_write_blocking(TFT35_SPI, &cmd, 1);
    cs_deselect();
}

// Write data bytes
static void write_data(const uint8_t *data, size_t len) {
    dc_data();
    cs_select();
    spi_write_blocking(TFT35_SPI, data, len);
    cs_deselect();
}

// Write a single data byte
static void write_data_byte(uint8_t data) {
    write_data(&data, 1);
}

// Read display ID for controller detection
static uint32_t read_display_id(void) {
    uint8_t id[4] = {0};

    dc_command();
    cs_select();
    uint8_t cmd = CMD_RDDID;
    spi_write_blocking(TFT35_SPI, &cmd, 1);

    dc_data();
    // Dummy read + 3 ID bytes
    spi_read_blocking(TFT35_SPI, 0xFF, id, 4);
    cs_deselect();

    return (id[1] << 16) | (id[2] << 8) | id[3];
}

// ST7796 initialization sequence
static void init_st7796(void) {
    // Software reset
    write_cmd(CMD_SWRESET);
    sleep_ms(120);

    // Sleep out
    write_cmd(CMD_SLPOUT);
    sleep_ms(120);

    // Interface Pixel Format - 16bit/pixel
    write_cmd(CMD_COLMOD);
    write_data_byte(0x55);  // RGB565

    // Memory Access Control
    write_cmd(CMD_MADCTL);
    write_data_byte(MADCTL_MX | MADCTL_BGR);  // Landscape, BGR

    // Porch Control
    write_cmd(0xB2);
    uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    write_data(porch, sizeof(porch));

    // Gate Control
    write_cmd(0xB7);
    write_data_byte(0x35);

    // VCOM Setting
    write_cmd(0xBB);
    write_data_byte(0x19);

    // LCM Control
    write_cmd(0xC0);
    write_data_byte(0x2C);

    // VDV and VRH Command Enable
    write_cmd(0xC2);
    write_data_byte(0x01);

    // VRH Set
    write_cmd(0xC3);
    write_data_byte(0x12);

    // VDV Set
    write_cmd(0xC4);
    write_data_byte(0x20);

    // Frame Rate Control
    write_cmd(0xC6);
    write_data_byte(0x0F);  // 60Hz

    // Power Control 1
    write_cmd(0xD0);
    uint8_t pwr[] = {0xA4, 0xA1};
    write_data(pwr, sizeof(pwr));

    // Positive Gamma
    write_cmd(0xE0);
    uint8_t pgamma[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
                        0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    write_data(pgamma, sizeof(pgamma));

    // Negative Gamma
    write_cmd(0xE1);
    uint8_t ngamma[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
                        0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    write_data(ngamma, sizeof(ngamma));

    // Inversion off
    write_cmd(CMD_INVOFF);

    // Display on
    write_cmd(CMD_DISPON);
    sleep_ms(20);
}

// ILI9488 initialization sequence
static void init_ili9488(void) {
    // Software reset
    write_cmd(CMD_SWRESET);
    sleep_ms(120);

    // Sleep out
    write_cmd(CMD_SLPOUT);
    sleep_ms(120);

    // Interface Pixel Format - 16bit/pixel for SPI
    write_cmd(CMD_COLMOD);
    write_data_byte(0x55);  // RGB565

    // Memory Access Control
    write_cmd(CMD_MADCTL);
    write_data_byte(MADCTL_MX | MADCTL_BGR);

    // Power Control 1
    write_cmd(0xC0);
    uint8_t pwr1[] = {0x17, 0x15};
    write_data(pwr1, sizeof(pwr1));

    // Power Control 2
    write_cmd(0xC1);
    write_data_byte(0x41);

    // VCOM Control
    write_cmd(0xC5);
    uint8_t vcom[] = {0x00, 0x12, 0x80};
    write_data(vcom, sizeof(vcom));

    // Interface Control
    write_cmd(0xB0);
    write_data_byte(0x00);

    // Frame Rate
    write_cmd(0xB1);
    uint8_t frc[] = {0xA0, 0x11};
    write_data(frc, sizeof(frc));

    // Display Inversion Control
    write_cmd(0xB4);
    write_data_byte(0x02);

    // Display Function Control
    write_cmd(0xB6);
    uint8_t dfc[] = {0x02, 0x02, 0x3B};
    write_data(dfc, sizeof(dfc));

    // Entry Mode Set
    write_cmd(0xB7);
    write_data_byte(0xC6);

    // Adjust Control 3
    write_cmd(0xF7);
    uint8_t adj[] = {0xA9, 0x51, 0x2C, 0x82};
    write_data(adj, sizeof(adj));

    // Positive Gamma
    write_cmd(0xE0);
    uint8_t pgamma[] = {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
                        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F};
    write_data(pgamma, sizeof(pgamma));

    // Negative Gamma
    write_cmd(0xE1);
    uint8_t ngamma[] = {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
                        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F};
    write_data(ngamma, sizeof(ngamma));

    // Display on
    write_cmd(CMD_DISPON);
    sleep_ms(20);
}

// Initialize display
void tft35_display_init(void) {
    // Create mutex for SPI access
    spi_mutex = xSemaphoreCreateMutex();

    // Initialize SPI
    spi_init(TFT35_SPI, TFT35_SPI_FREQ_HZ);
    gpio_set_function(TFT35_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(TFT35_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(TFT35_MISO_PIN, GPIO_FUNC_SPI);

    // Initialize control pins
    gpio_init(TFT35_CS_PIN);
    gpio_set_dir(TFT35_CS_PIN, GPIO_OUT);
    gpio_put(TFT35_CS_PIN, 1);  // Deselect

    gpio_init(TFT35_DC_PIN);
    gpio_set_dir(TFT35_DC_PIN, GPIO_OUT);
    gpio_put(TFT35_DC_PIN, 1);

    gpio_init(TFT35_RST_PIN);
    gpio_set_dir(TFT35_RST_PIN, GPIO_OUT);

    // Hardware reset
    gpio_put(TFT35_RST_PIN, 0);
    sleep_ms(10);
    gpio_put(TFT35_RST_PIN, 1);
    sleep_ms(120);

    // Detect controller type
    uint32_t id = read_display_id();

    // ST7796 typically returns 0x7796XX, ILI9488 returns 0x9488XX
    if ((id >> 8) == 0x7796 || (id >> 16) == 0x77) {
        detected_controller = TFT35_CONTROLLER_ST7796;
        init_st7796();
    } else if ((id >> 8) == 0x9488 || (id >> 16) == 0x94) {
        detected_controller = TFT35_CONTROLLER_ILI9488;
        init_ili9488();
    } else {
        // Default to ST7796 if detection fails
        detected_controller = TFT35_CONTROLLER_ST7796;
        init_st7796();
    }

    // Initialize DMA channel for fast transfers
    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(TFT35_SPI, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(dma_channel, &c, &spi_get_hw(TFT35_SPI)->dr, NULL, 0, false);
}

// Get detected controller
tft35_controller_t tft35_display_get_controller(void) {
    return detected_controller;
}

// Set rotation
void tft35_display_set_rotation(tft35_rotation_t rotation) {
    uint8_t madctl = MADCTL_BGR;

    switch (rotation) {
        case TFT35_ROTATION_0:
            madctl |= MADCTL_MX;
            break;
        case TFT35_ROTATION_90:
            madctl |= MADCTL_MV;
            break;
        case TFT35_ROTATION_180:
            madctl |= MADCTL_MY;
            break;
        case TFT35_ROTATION_270:
            madctl |= MADCTL_MX | MADCTL_MY | MADCTL_MV;
            break;
    }

    write_cmd(CMD_MADCTL);
    write_data_byte(madctl);
    current_rotation = rotation;
}

// Set brightness (if backlight PWM available)
void tft35_display_set_brightness(uint8_t brightness) {
    // TFT35 may not have software brightness control
    // This could be implemented with a backlight PWM pin if available
    (void)brightness;
}

// Set drawing window
void tft35_display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column address
    write_cmd(CMD_CASET);
    uint8_t col_data[] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
    };
    write_data(col_data, 4);

    // Row address
    write_cmd(CMD_RASET);
    uint8_t row_data[] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
    };
    write_data(row_data, 4);

    // Write to RAM
    write_cmd(CMD_RAMWR);
}

// Write pixels using DMA
void tft35_display_write_pixels(const uint16_t *data, uint32_t len) {
    dc_data();
    cs_select();

    // Use DMA for large transfers
    if (len > 32 && dma_channel >= 0) {
        dma_channel_set_read_addr(dma_channel, data, false);
        dma_channel_set_trans_count(dma_channel, len * 2, true);  // 2 bytes per pixel
        dma_channel_wait_for_finish_blocking(dma_channel);
    } else {
        spi_write_blocking(TFT35_SPI, (const uint8_t *)data, len * 2);
    }

    cs_deselect();
}

// LVGL flush callback
void tft35_display_flush_cb(void *disp, const void *area, uint8_t *color_map) {
    lv_display_t *display = (lv_display_t *)disp;
    const lv_area_t *a = (const lv_area_t *)area;

    uint16_t x1 = a->x1;
    uint16_t y1 = a->y1;
    uint16_t x2 = a->x2;
    uint16_t y2 = a->y2;

    uint32_t size = (x2 - x1 + 1) * (y2 - y1 + 1);

    if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        tft35_display_set_window(x1, y1, x2, y2);
        tft35_display_write_pixels((const uint16_t *)color_map, size);
        xSemaphoreGive(spi_mutex);
    }

    lv_display_flush_ready(display);
}

// Signal flush complete
void tft35_display_flush_ready(void) {
    if (lvgl_display) {
        lv_display_flush_ready(lvgl_display);
    }
}
