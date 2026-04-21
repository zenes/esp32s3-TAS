/**
 * @file      socket_bridge.h
 * @brief     High-performance Socket Bridge (WiFi lwIP <-> Ethernet TOE)
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t listen_port;    // Port to listen on WiFi (lwIP)
    uint8_t  target_ip[4];   // Destination IP on Ethernet (TOE)
    uint16_t target_port;    // Destination Port on Ethernet (TOE)
    
    // W5500 Network Config (Required for TOE initialization)
    uint8_t mac_addr[6];
    uint32_t local_ip;
    uint32_t gateway;
    uint32_t subnet;
    
    bool is_udp;             // true for UDP, false for TCP
} socket_bridge_cfg_t;

/**
 * @brief Start the socket bridge task
 * @return true if started successfully
 */
bool socket_bridge_start(const socket_bridge_cfg_t* cfg);

/**
 * @brief Stop the socket bridge task
 */
void socket_bridge_stop();

/**
 * @brief Check if the bridge is currently running
 */
bool socket_bridge_is_running();

#ifdef __cplusplus
}
#endif
