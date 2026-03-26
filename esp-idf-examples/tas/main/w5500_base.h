/**
 * @file      w5500_base.h
 * @brief     Low-level W5500 hardware abstraction layer (HAL) for ESP32
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>

#ifdef __cplusplus
extern "C" {
#endif

// W5500 Common Registers
#define W5500_MR       0x0000 
#define W5500_GAR      0x0001
#define W5500_SUBR     0x0005 
#define W5500_SHAR     0x0009 
#define W5500_SIPR     0x000F 
#define W5500_VERSIONR 0x0039 

// W5500 Socket Constants
#define W5500_S0 0
#define W5500_S1 1
#define W5500_S2 2
#define W5500_S3 3
#define W5500_S4 4
#define W5500_S5 5
#define W5500_S6 6
#define W5500_S7 7

// W5500 Socket Registers
#define Sn_MR         0x0000 
#define Sn_CR         0x0001 
#define Sn_IR         0x0002 
#define Sn_SR         0x0003 
#define Sn_PORT       0x0004 
#define Sn_DIPR       0x000C 
#define Sn_DPORT      0x0010 
#define Sn_RXBUF_SIZE 0x001E 
#define Sn_TXBUF_SIZE 0x001F 
#define Sn_TX_FSR     0x0020 
#define Sn_TX_RD      0x0022 
#define Sn_TX_WR      0x0024 
#define Sn_RX_RSR     0x0026 
#define Sn_RX_RD      0x0028 
#define Sn_RX_WR      0x002A 

// Sn_MR Values
#define Sn_MR_TCP     0x01
#define Sn_MR_UDP     0x02

// Sn_CR Values
#define Sn_CR_OPEN    0x01
#define Sn_CR_LISTEN  0x02
#define Sn_CR_CONNECT 0x04
#define Sn_CR_DISCON  0x08
#define Sn_CR_CLOSE   0x10
#define Sn_CR_SEND    0x20
#define Sn_CR_RECV    0x40

// Sn_SR Values
#define SOCK_CLOSED      0x00
#define SOCK_INIT        0x13
#define SOCK_LISTEN      0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT  0x1C
#define SOCK_UDP         0x22

/**
 * @brief Initialize SPI for W5500 and verify communication
 * @return true if version check passes
 */
bool w5500_hw_init();

/**
 * @brief De-initialize SPI
 */
void w5500_hw_deinit();

// Register Access
void w5500_write(uint16_t addr, uint8_t block, uint8_t data);
uint8_t w5500_read(uint16_t addr, uint8_t block);
void w5500_write16(uint16_t addr, uint8_t block, uint16_t data);
uint16_t w5500_read16(uint16_t addr, uint8_t block);

// Data Transfer (Optimized with Batching/DMA readiness)
void w5500_write_data(uint8_t socket, const uint8_t *src, uint16_t len);
void w5500_read_data(uint8_t socket, uint8_t *dst, uint16_t len);

// Register Buffer Access (Common Registers)
void w5500_write_buf(uint16_t addr, uint8_t block, const uint8_t *src, uint16_t len);
void w5500_read_buf(uint16_t addr, uint8_t block, uint8_t *dst, uint16_t len);

// Command Control
void wait_for_cmd(uint8_t socket);

#ifdef __cplusplus
}
#endif
