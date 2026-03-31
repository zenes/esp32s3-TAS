/**
 * @file lv_conf.h
 * Configuration file for v8.3.x
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   GRAPHICAL SETTINGS
 *====================*/

/* Color depth: 1 (1 bit per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. Useful if the display has an 8-bit interface (e.g. SPI) */
#ifndef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 1
#endif

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* 1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()` */
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    /* Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB) */
    #define LV_MEM_SIZE (48U * 1024U)          /*[bytes]*/
#endif

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period. LVGL will redraw at most this many times in a second */
#ifndef LV_DISP_DEF_REFR_PERIOD
#define LV_DISP_DEF_REFR_PERIOD 16      /*[ms] 60 FPS 타겟 설정*/
#endif

/* 1: Show CPU usage and FPS count in the right bottom corner */
#define LV_USE_PERF_MONITOR 1
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
#endif

/* Default dot per inch. Used by the built-in themes */
#define LV_DPI_DEF 130                  /*[px/inch]*/

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Drawing
 *-----------*/

/* Max. msecs to be spent in free_line_cache_evict_cb. 0: disable */
#define LV_DRAW_COMPLEX 1

/*-------------
 * Helpers
 *-----------*/

/* 1: Enable API to take the screenshot */
#define LV_USE_SNAPSHOT 0

/*=====================
 *  Compiler settings
 *====================*/

/* Fill the buffers with something known to see the uninitialized areas */
#define LV_USE_ASSERT_NULL          1   /* Check if the parameter is NULL. (Quite fast) */
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /* DISABLED: causes INT_WDT on ESP32 (full mem scan every frame) */

/* Default value of the custom data in the widgets */
#define LV_USE_USER_DATA 1

/*================
 *  HAL settings
 *================*/

/* 1: use a custom tick source without `lv_tick_inc` */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"         /*Header for the system time function*/
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())    /*Expression evaluating to current system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

/*================
 * Log settings
 *================*/

/*1: Enable the log module*/
#define LV_USE_LOG 1
#if LV_USE_LOG
    /* How important log should be added:
     * LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
     * LV_LOG_LEVEL_INFO        Log important events
     * LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
     * LV_LOG_LEVEL_ERROR       Only critical issues, when the system may fail
     * LV_LOG_LEVEL_USER        Only logs added by the user
     * LV_LOG_LEVEL_NONE        Do not log anything */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /* 1: Print the log with 'printf';
     * 0: User need to register a callback with `lv_log_register_print_cb()`*/
    #define LV_LOG_PRINTF 1
#endif  /*LV_USE_LOG*/

/*=====================
 *  BUILD CONFIGURATION
 *=====================*/

/* 1: Add a symbol to all LVGL variables */
#define LV_EXPORT_CONST_INT(int_value) struct _dummy_for_int

/*==================
 *   FONT USAGE
 *==================*/

/* The built-in fonts */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* The default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*=================
 *  Text settings
 *=================*/

/* 1: Enable bidi (bidirectional) text support */
#define LV_USE_BIDI 0

/* 1: Enable Arabic/Persian shaping support */
#define LV_USE_ARABIC_PERSIAN_SHAPING 0

/*===================
 *  WIDGETS
 *==================*/

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1

/*==================
 *  THEMES
 *==================*/

/* 1: A more complex theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 1
    #define LV_THEME_DEFAULT_GROW 1
#endif /*LV_USE_THEME_DEFAULT*/

#endif /*LV_CONF_H*/
