#ifndef RASPBERRYPI_PICO_W_CONFIG_H_
#define RASPBERRYPI_PICO_W_CONFIG_H_

#include "pico/cyw43_arch.h"

/* Specify board PIN mapping
    Reference: https://github.com/eamars/RaspberryPi-Pico-Motor-Expansion-Board?tab=readme-ov-file#peripherals
*/

#define WATCHDOG_LED_PIN CYW43_WL_GPIO_LED_PIN

#define DISPLAY0_SPI spi0
#define DISPLAY0_RX_PIN 16
#define DISPLAY0_TX_PIN 19
#define DISPLAY0_CS_PIN 17
#define DISPLAY0_SCK_PIN 18
#define DISPLAY0_A0_PIN 20
#define DISPLAY0_RESET_PIN 21

#define BUTTON0_ENCODER_PIN1 15
#define BUTTON0_ENCODER_PIN2 14
#define BUTTON0_ENC_PIN 22
#define BUTTON0_RST_PIN 12
#define NEOPIXEL_PIN 13
#define NEOPIXEL_PWM3_PIN 28

#define MOTOR_UART uart1
#define MOTOR_UART_TX 4
#define MOTOR_UART_RX 5

#define COARSE_MOTOR_ADDR 0
#define COARSE_MOTOR_EN_PIN 6
#define COARSE_MOTOR_STEP_PIN 3 
#define COARSE_MOTOR_DIR_PIN 2

#define FINE_MOTOR_ADDR 1
#define FINE_MOTOR_EN_PIN 9
#define FINE_MOTOR_STEP_PIN 8
#define FINE_MOTOR_DIR_PIN 7

#define SCALE_UART uart0
#define SCALE_UART_BAUDRATE 19200
#define SCALE_UART_TX 0
#define SCALE_UART_RX 1

#define EEPROM_I2C i2c1
#define EEPROM_SDA_PIN 10
#define EEPROM_SCL_PIN 11
#define EEPROM_ADDR 0x50

#define SERVO0_PWM_PIN 26
#define SERVO1_PWM_PIN 27
#define SERVO_PWM_SLICE_NUM 5

// Color TFT Display Configuration (TFT35 or TFT43)
#if defined(USE_TFT35) || defined(USE_TFT43)
    // Both displays use the same SPI pins as the Mini 12864
    #define TFT_SPI             spi0
    #define TFT_SCK_PIN         DISPLAY0_SCK_PIN    // GPIO 18
    #define TFT_MOSI_PIN        DISPLAY0_TX_PIN     // GPIO 19
    #define TFT_MISO_PIN        DISPLAY0_RX_PIN     // GPIO 16
    #define TFT_CS_PIN          DISPLAY0_CS_PIN     // GPIO 17
    #define TFT_DC_PIN          DISPLAY0_A0_PIN     // GPIO 20
    #define TFT_RST_PIN         DISPLAY0_RESET_PIN  // GPIO 21

    // Additional pins for touch controller (XPT2046)
    #define TFT_TOUCH_CS_PIN    24  // Touch chip select
    #define TFT_TOUCH_IRQ_PIN   25  // Touch interrupt (active low)

    // SPI speeds
    #define TFT_SPI_FREQ_HZ     (40 * 1000 * 1000)  // 40MHz for display
    #define TFT_TOUCH_SPI_FREQ_HZ (2 * 1000 * 1000) // 2MHz for touch

    // Display-specific parameters
    #ifdef USE_TFT35
        // TFT35 V3.0.1: 3.5 inch, 480x320
        #define TFT_WIDTH       480
        #define TFT_HEIGHT      320
    #else
        // TFT43 V3.0: 4.3 inch, 480x272
        #define TFT_WIDTH       480
        #define TFT_HEIGHT      272
    #endif

    // Legacy aliases for compatibility
    #define TFT35_SPI           TFT_SPI
    #define TFT35_SCK_PIN       TFT_SCK_PIN
    #define TFT35_MOSI_PIN      TFT_MOSI_PIN
    #define TFT35_MISO_PIN      TFT_MISO_PIN
    #define TFT35_CS_PIN        TFT_CS_PIN
    #define TFT35_DC_PIN        TFT_DC_PIN
    #define TFT35_RST_PIN       TFT_RST_PIN
    #define TFT35_TOUCH_CS_PIN  TFT_TOUCH_CS_PIN
    #define TFT35_TOUCH_IRQ_PIN TFT_TOUCH_IRQ_PIN
    #define TFT35_WIDTH         TFT_WIDTH
    #define TFT35_HEIGHT        TFT_HEIGHT
    #define TFT35_SPI_FREQ_HZ   TFT_SPI_FREQ_HZ
    #define TFT35_TOUCH_SPI_FREQ_HZ TFT_TOUCH_SPI_FREQ_HZ
#endif

#endif  // RASPBERRYPI_PICO_W_CONFIG_H_