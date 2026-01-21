/**
 * @file lv_conf.h
 * @brief LVGL Configuration for TFT35 on Raspberry Pi Pico W
 *
 * Configuration file for LVGL v9.x
 * Optimized for 480x320 TFT display with FreeRTOS
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color for SPI displays */
#define LV_COLOR_16_SWAP 1

/*====================
   MEMORY SETTINGS
 *====================*/

/* Size of the memory available for `lv_malloc()` in bytes (>= 2kB) */
#define LV_MEM_SIZE (48 * 1024U)

/* Set an address for the memory pool instead of allocating it as a normal array. */
#define LV_MEM_ADR 0

/* Use custom allocator (FreeRTOS pvPortMalloc/vPortFree) */
#define LV_MEM_CUSTOM 0

/* Memory monitor (shows used memory, helps tune LV_MEM_SIZE) */
#define LV_USE_MEM_MONITOR 0

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh, input device read and animation step period */
#define LV_DEF_REFR_PERIOD 20

/* Default Dot Per Inch (used for scaling) */
#define LV_DPI_DEF 130

/* Use a custom tick source with lv_tick_inc() */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "FreeRTOS.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount() * portTICK_PERIOD_MS)
#endif

/*====================
   FEATURE CONFIGURATION
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/* Default display render mode for new displays */
#define LV_DISPLAY_DEF_REFR_PERIOD 20

/* Enable drawing layers */
#define LV_USE_DRAW_SW 1

/*-------------
 * GPU
 *-----------*/

/* No GPU on Pico */
#define LV_USE_DRAW_ARM2D 0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

/*-------------
 * Logging
 *-----------*/

/* Enable the log module */
#define LV_USE_LOG 0

#if LV_USE_LOG
    /* Debug, Trace, Info, Warn, Error, User or None */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    /* Print via printf (disable for production) */
    #define LV_LOG_PRINTF 1
#endif

/*-------------
 * Asserts
 *-----------*/

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*-------------
 * Others
 *-----------*/

/* Enable animations */
#define LV_USE_ANIM 1

/* Enable shadow drawing */
#define LV_USE_SHADOW 1
#define LV_SHADOW_CACHE_SIZE 0

/* Enable blend modes */
#define LV_USE_BLEND_MODES 0

/* Garbage collector */
#define LV_ENABLE_GC 0

/*====================
 * COMPILER SETTINGS
 *====================*/

/* For big endian systems */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Attribute for large constant arrays */
#define LV_ATTRIBUTE_LARGE_CONST

/* Attribute for large RAM arrays */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Prefix for performance critical functions */
#define LV_ATTRIBUTE_FAST_MEM

/* Export integer constant to binding (no effect in C) */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Prefix all global extern data with this */
#define LV_ATTRIBUTE_EXTERN_DATA

/* Use `float` for coordinates */
#define LV_USE_FLOAT 0

/*====================
   FONT USAGE
 *====================*/

/* Montserrat fonts (recommended for embedded) */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0

/* Pixel perfect monospace fonts */
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable font subpixel rendering */
#define LV_USE_FONT_SUBPX 0

/* Enable drawing placeholders when glyph not found */
#define LV_USE_FONT_PLACEHOLDER 1

/*====================
   TEXT SETTINGS
 *====================*/

/* Select character encoding */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Can break (wrap) texts on these chars */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* If a word is at least this long, will break wherever "prettiest" */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum number of characters in a long word to put on a line before break */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum number of characters in a long word to put on a line after break */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* Support bidirectional texts */
#define LV_USE_BIDI 0

/* Enable Arabic/Persian processing */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGET USAGE
 *====================*/

/* Documentation of widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1

/*====================
   EXTRA COMPONENTS
 *====================*/

/* Widgets */
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   1
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_METER      0
#define LV_USE_MSGBOX     1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1
    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 0
    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

#define LV_USE_THEME_SIMPLE 1

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
   OS ABSTRACTION
 *====================*/

/* Enable FreeRTOS integration */
#define LV_USE_OS LV_OS_FREERTOS

/*====================
   OTHER
 *====================*/

/* Enable snapshot */
#define LV_USE_SNAPSHOT 0

/* Enable Monkey testing */
#define LV_USE_MONKEY 0

/* Enable profiler */
#define LV_USE_PROFILER 0

/* Enable file system */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0

/* PNG decoder */
#define LV_USE_PNG 0

/* BMP decoder */
#define LV_USE_BMP 0

/* JPG and SJPG decoder */
#define LV_USE_SJPG 0

/* GIF decoder */
#define LV_USE_GIF 0

/* QR code library */
#define LV_USE_QRCODE 0

/* FreeType library */
#define LV_USE_FREETYPE 0

/* Tiny TTF library */
#define LV_USE_TINY_TTF 0

/* Rlottie library */
#define LV_USE_RLOTTIE 0

/* FFmpeg library */
#define LV_USE_FFMPEG 0

#endif /* LV_CONF_H */
