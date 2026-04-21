#include "w5500_base.h"
#include "utilities.h" 

static SPIClass* w5500_spi = nullptr;

bool w5500_hw_init() {
    if (!w5500_spi) {
        w5500_spi = new SPIClass(FSPI); 
        w5500_spi->begin(ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, -1);
        
        pinMode(ETH_CS_PIN, OUTPUT);
        digitalWrite(ETH_CS_PIN, HIGH);
        
        delay(10);
        uint8_t version = w5500_read(W5500_VERSIONR, 0x00);
        Serial.printf("[W5500-HAL] Version read: 0x%02X\n", version);
        
        if (version != 0x04) {
             Serial.println("[W5500-HAL] Error: SPI Check Failed!");
             return false;
        }
    }
    return true;
}

void w5500_hw_deinit() {
    if(w5500_spi) {
        w5500_spi->end();
        delete w5500_spi;
        w5500_spi = NULL;
    }
}

void w5500_write(uint16_t addr, uint8_t block, uint8_t data) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x04) };
    w5500_spi->transfer(header, 3);
    w5500_spi->transfer(data);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
}

uint8_t w5500_read(uint16_t addr, uint8_t block) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x00) };
    w5500_spi->transfer(header, 3);
    uint8_t res = w5500_spi->transfer(0x00);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
    return res;
}

void w5500_write16(uint16_t addr, uint8_t block, uint16_t data) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t cmd[5] = { 
        (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x04),
        (uint8_t)(data >> 8), (uint8_t)(data & 0xFF)
    };
    w5500_spi->transfer(cmd, 5);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
}

uint16_t w5500_read16(uint16_t addr, uint8_t block) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x00) };
    w5500_spi->transfer(header, 3);
    uint8_t hi = w5500_spi->transfer(0x00);
    uint8_t lo = w5500_spi->transfer(0x00);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
    return (hi << 8) | lo;
}

void w5500_write_data(uint8_t socket, const uint8_t *src, uint16_t len) {
    uint16_t ptr = w5500_read16(Sn_TX_WR, socket);
    uint8_t block = (socket * 4) + 2; 
    
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(ptr >> 8), (uint8_t)(ptr & 0xFF), (uint8_t)((block << 3) | 0x04) };
    w5500_spi->transfer(header, 3);
    w5500_spi->transfer((uint8_t*)src, len); 
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
    
    w5500_write16(Sn_TX_WR, socket, ptr + len);
    w5500_write(Sn_CR, socket, Sn_CR_SEND);
    wait_for_cmd(socket);
}

void w5500_read_data(uint8_t socket, uint8_t *dst, uint16_t len) {
    uint16_t ptr = w5500_read16(Sn_RX_RD, socket);
    uint8_t block = (socket * 4) + 3; 
    
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(ptr >> 8), (uint8_t)(ptr & 0xFF), (uint8_t)((block << 3) | 0x00) };
    w5500_spi->transfer(header, 3);
    w5500_spi->transfer(dst, len); 
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
    
    w5500_write16(Sn_RX_RD, socket, ptr + len);
    w5500_write(Sn_CR, socket, Sn_CR_RECV);
    wait_for_cmd(socket);
}

void w5500_write_buf(uint16_t addr, uint8_t block, const uint8_t *src, uint16_t len) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x04) };
    w5500_spi->transfer(header, 3);
    w5500_spi->transfer((uint8_t*)src, len);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
}

void w5500_read_buf(uint16_t addr, uint8_t block, uint8_t *dst, uint16_t len) {
    w5500_spi->beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(ETH_CS_PIN, LOW);
    uint8_t header[3] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), (uint8_t)((block << 3) | 0x00) };
    w5500_spi->transfer(header, 3);
    for(uint16_t i=0; i<len; i++) dst[i] = w5500_spi->transfer(0x00);
    digitalWrite(ETH_CS_PIN, HIGH);
    w5500_spi->endTransaction();
}

void wait_for_cmd(uint8_t socket) {
    while (w5500_read(Sn_CR, socket) != 0) {
        // Fast polling
    }
}
