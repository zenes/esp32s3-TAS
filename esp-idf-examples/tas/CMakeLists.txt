cmake_minimum_required(VERSION 3.16.0)

# --- Auto-Patch Arduino ETH.h for ESP-IDF v5.1 Compatibility ---
set(ETH_H_PATH "${CMAKE_CURRENT_LIST_DIR}/components/arduino/libraries/Ethernet/src/ETH.h")
if(EXISTS ${ETH_H_PATH})
    file(READ ${ETH_H_PATH} ETH_H_CONTENT)
    string(FIND "${ETH_H_CONTENT}" "#define ETH_SPI_SUPPORTS_CUSTOM 1" MATCH_FOUND)
    if(NOT MATCH_FOUND EQUAL -1)
        message(STATUS "Auto-Patching Arduino ETH.h for standard ESP-IDF v5.1 compatibility...")
        string(REPLACE "#define ETH_SPI_SUPPORTS_CUSTOM 1" "#define ETH_SPI_SUPPORTS_CUSTOM 0" ETH_H_CONTENT "${ETH_H_CONTENT}")
        string(REPLACE "#define ETH_SPI_SUPPORTS_NO_IRQ 1" "#define ETH_SPI_SUPPORTS_NO_IRQ 0" ETH_H_CONTENT "${ETH_H_CONTENT}")
        file(WRITE ${ETH_H_PATH} "${ETH_H_CONTENT}")
    endif()
endif()
# ---------------------------------------------------------------

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(LilyGO-T-ETH-Series)
