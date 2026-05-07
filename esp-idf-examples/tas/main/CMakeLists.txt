# -----------------------------------------------------------------------------
# Build Profile Selection (-DENV=profile_name)
# -----------------------------------------------------------------------------
set(ENV "lilygo-lcd1" CACHE STRING "Select build environment (e.g., lilygo-lcd1, evb-lcd3, tas-lcd1)")
message(STATUS "Selected Build Environment: ${ENV}")

# Default definitions
set(EXTRA_DEFS "")
# Unconditionally include all libraries to prevent include path issues during ESP-IDF multi-pass configuration
set(COMP_REQUIRES "TFT_eSPI" "lovyan_gfx" "arduino" "lvgl" "esp_eth" "ETHClass2" "fatfs" "wear_levelling" "esp_tinyusb")

if("${ENV}" STREQUAL "lilygo-lcd1")
    list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ST7789=1 ENABLE_LCD=1)
    
elseif("${ENV}" STREQUAL "lilygo-lcd2")
    list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ILI9341=1 ENABLE_LCD=1)

elseif("${ENV}" STREQUAL "lilygo-lcd3")
    list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ILI9341=1 TFT_PARALLEL_16_BIT=1 ENABLE_LCD=1)
    list(APPEND EXTRA_DEFS TFT_WR=17 TFT_RD=4 TFT_DC=9 TFT_CS=46 TFT_RST=3)

elseif("${ENV}" STREQUAL "evb-lcd3")
    list(APPEND EXTRA_DEFS ESP32_S3_LCD_EV_BOARD_2=1 LCD_TYPE_ILI9341=1 TFT_PARALLEL_16_BIT=1 ENABLE_LCD=1 USE_LOVYANGFX=1)
    list(APPEND EXTRA_DEFS LGFX_WR=17 LGFX_RD=4 LGFX_DC=9 LGFX_CS=46 LGFX_RST=3)

elseif("${ENV}" STREQUAL "lilygo-lcd1-p8")
    list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ST7789=1 TFT_PARALLEL_8_BIT=1 ENABLE_LCD=1 USE_LOVYANGFX=1)
    # LilyGO 8-bit Parallel Pins (Safe Pinmap using LGFX_ prefix)
    list(APPEND EXTRA_DEFS LGFX_WR=40 LGFX_RD=41 LGFX_DC=38 LGFX_CS=21 LGFX_RST=39)
    list(APPEND EXTRA_DEFS LGFX_D0=1 LGFX_D1=2 LGFX_D2=4 LGFX_D3=8 LGFX_D4=15 LGFX_D5=16 LGFX_D6=17 LGFX_D7=18)
    # Color Consistency Flags from platformio.ini
    list(APPEND EXTRA_DEFS TFT_INVERSION_ON=1 TFT_RGB_ORDER=0 LV_COLOR_16_SWAP=1)

elseif("${ENV}" STREQUAL "evb-lcd1-p8")
    list(APPEND EXTRA_DEFS ESP32_S3_LCD_EV_BOARD_2=1 LCD_TYPE_ST7789=1 TFT_PARALLEL_8_BIT=1 ENABLE_LCD=1 USE_LOVYANGFX=1)
    list(APPEND EXTRA_DEFS LGFX_WR=17 LGFX_RD=4 LGFX_DC=9 LGFX_CS=46 LGFX_RST=3)
    list(APPEND EXTRA_DEFS LGFX_D0=45 LGFX_D1=38 LGFX_D2=39 LGFX_D3=40 LGFX_D4=41 LGFX_D5=42 LGFX_D6=2 LGFX_D7=1)

elseif("${ENV}" STREQUAL "tas-lcd1")
    list(APPEND EXTRA_DEFS TAS_CUSTOM_BOARD=1 LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ST7789=1 ENABLE_LCD=1)

else()
    message(WARNING "Unknown ENV: ${ENV}. Falling back to lilygo-lcd1.")
    list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ST7789=1 ENABLE_LCD=1)
endif()

# -----------------------------------------------------------------------------
# Component Registration
# -----------------------------------------------------------------------------
idf_component_register(SRCS "main.cpp"
                            "socket_bridge.cpp"
                            "toe_iperf.cpp"
                            "w5500_base.cpp"
                            "audio_output.cpp"
                            "file_manager.cpp"
                            "usb_msc.cpp"
                            "audio_player.cpp"
                            "lwiperf.c"
                    INCLUDE_DIRS "."
                    REQUIRES ${COMP_REQUIRES})

# Add definitions globally so they propagate to libraries (LVGL, LovyanGFX)
add_compile_definitions(
    ${EXTRA_DEFS}
    BOARD_HAS_PSRAM=1
    CORE_DEBUG_LEVEL=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-w")
