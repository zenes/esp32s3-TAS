/**
 * @file      toe_iperf.h
 * @brief     W5500 TOE (Hardware Socket) throughput test definitions
 */

#pragma once

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// TOE iperf configuration
typedef struct {
    bool is_server;
    bool is_udp;
    uint16_t port;
    const char* target_ip; // Used when is_server == false
    
    // IP configuration for the HW stack
    uint8_t mac_addr[6];
    uint32_t local_ip;
    uint32_t gateway;
    uint32_t subnet;
} toe_iperf_cfg_t;

/**
 * @brief Initialize the SPI and W5500 for TOE operations
 * @return true if successful, false otherwise
 */
bool toe_iperf_init();

/**
 * @brief Start the TOE throughput test (Server or Client)
 * @param cfg Configuration containing mode, port and target IP
 * @return true if started successfully
 */
bool toe_iperf_start(const toe_iperf_cfg_t* cfg);

/**
 * @brief Stop the running TOE throughput test
 */
void toe_iperf_stop();

/**
 * @brief Check if TOE iperf is currently running
 * @return true if running
 */
bool toe_iperf_is_running();

#ifdef __cplusplus
}
#endif
