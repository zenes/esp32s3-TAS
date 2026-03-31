#include "socket_bridge.h"
#include "w5500_base.h"
#include <WiFi.h>

static bool is_running = false;
static TaskHandle_t bridge_task_handle = NULL;

static void socket_bridge_relay_task(void* pvParameters) {
    socket_bridge_cfg_t* cfg = (socket_bridge_cfg_t*)pvParameters;
    uint8_t eth_socket = W5500_S1;
    WiFiServer server(cfg->listen_port);
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_DMA);
    
    if (!w5500_hw_init()) {
        Serial.println("[Bridge] Failed to init W5500 HAL.");
        goto exit;
    }

    // W5500 Memory Allocation: 16KB for Socket 1, 0KB for others
    for (int i = 0; i < 8; i++) {
        uint8_t size = (i == eth_socket) ? 16 : 0;
        w5500_write(Sn_RXBUF_SIZE, i, size);
        w5500_write(Sn_TXBUF_SIZE, i, size);
    }
    
    // W5500 Base Config
    w5500_write(W5500_MR, 0x00, 0x00);
    w5500_write_buf(W5500_SHAR, 0x00, cfg->mac_addr, 6);
    w5500_write_buf(W5500_GAR, 0x00, (uint8_t*)&cfg->gateway, 4);
    w5500_write_buf(W5500_SUBR, 0x00, (uint8_t*)&cfg->subnet, 4);
    w5500_write_buf(W5500_SIPR, 0x00, (uint8_t*)&cfg->local_ip, 4);
    
    server.begin();
    server.setNoDelay(true); 
    Serial.printf("[Bridge] Server started on Port %d. Linking to %d.%d.%d.%d:%d\n", 
                  cfg->listen_port, cfg->target_ip[0], cfg->target_ip[1], cfg->target_ip[2], cfg->target_ip[3], cfg->target_port);
    
    while(is_running) {
        WiFiClient wifi_client = server.accept();
        if(wifi_client) {
            Serial.printf("[Bridge] New WiFi Connection from %s\n", wifi_client.remoteIP().toString().c_str());
            wifi_client.setNoDelay(true);
            
            w5500_write(Sn_MR, eth_socket, Sn_MR_TCP);
            w5500_write16(Sn_PORT, eth_socket, 5555); 
            w5500_write(Sn_CR, eth_socket, Sn_CR_OPEN);
            wait_for_cmd(eth_socket);
            
            w5500_write_buf(Sn_DIPR, eth_socket, cfg->target_ip, 4);
            w5500_write16(Sn_DPORT, eth_socket, cfg->target_port);
            w5500_write(Sn_CR, eth_socket, Sn_CR_CONNECT);
            wait_for_cmd(eth_socket);
            
            bool eth_connected = false;
            for(int i=0; i<100; i++) {
                if(w5500_read(Sn_SR, eth_socket) == SOCK_ESTABLISHED) {
                    eth_connected = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            if(!eth_connected) {
                uint8_t final_sr = w5500_read(Sn_SR, eth_socket);
                Serial.printf("[Bridge] Error: Target %d.%d.%d.%d unreachable (SR: 0x%02X)\n", 
                               cfg->target_ip[0], cfg->target_ip[1], cfg->target_ip[2], cfg->target_ip[3], final_sr);
                wifi_client.stop();
                w5500_write(Sn_CR, eth_socket, Sn_CR_CLOSE);
                wait_for_cmd(eth_socket);
                continue;
            }
            
            Serial.println("[Bridge] Bidirectional Relay Active.");
            
            while(is_running && wifi_client.connected()) {
                bool worked = false;
                
                // 1. WiFi -> Ethernet (TOE)
                while(true) {
                    size_t available = wifi_client.available();
                    if(available == 0) break;
                    
                    uint16_t tx_free = w5500_read16(Sn_TX_FSR, eth_socket);
                    if(tx_free == 0) break;

                    size_t read_len = (available > 8192) ? 8192 : available;
                    if(read_len > tx_free) read_len = tx_free;
                    
                    int bytes_read = wifi_client.read(buffer, read_len);
                    if (bytes_read > 0) {
                        w5500_write_data(eth_socket, buffer, bytes_read);
                        worked = true;
                    } else break;
                }
                
                // 2. Ethernet (TOE) -> WiFi (lwIP)
                while(true) {
                    uint16_t rx_size = w5500_read16(Sn_RX_RSR, eth_socket);
                    if(rx_size == 0) break;

                    size_t read_len = (rx_size > 8192) ? 8192 : rx_size;
                    w5500_read_data(eth_socket, buffer, read_len);
                    wifi_client.write(buffer, read_len);
                    worked = true;
                }
                
                if (w5500_read(Sn_SR, eth_socket) != SOCK_ESTABLISHED) break;
                
                if (!worked) {
                    vTaskDelay(1); 
                }
            }
            
            Serial.println("[Bridge] Client session ended.");
            wifi_client.stop();
            w5500_write(Sn_CR, eth_socket, Sn_CR_DISCON);
            wait_for_cmd(eth_socket);
            w5500_write(Sn_CR, eth_socket, Sn_CR_CLOSE);
            wait_for_cmd(eth_socket);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
exit:
    if(buffer) free(buffer);
    w5500_hw_deinit();
    is_running = false;
    if(cfg) free(cfg);
    bridge_task_handle = NULL;
    vTaskDelete(NULL);
}

bool socket_bridge_start(const socket_bridge_cfg_t* cfg) {
    if (is_running) return false;
    
    socket_bridge_cfg_t* cfg_copy = (socket_bridge_cfg_t*)malloc(sizeof(socket_bridge_cfg_t));
    if(!cfg_copy) return false;
    memcpy(cfg_copy, cfg, sizeof(socket_bridge_cfg_t));
    
    is_running = true;
    xTaskCreate(socket_bridge_relay_task, "socket_bridge", 8192, cfg_copy, 15, &bridge_task_handle);
    return true;
}

void socket_bridge_stop() {
    is_running = false;
}

bool socket_bridge_is_running() {
    return is_running;
}
