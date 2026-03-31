/**
 * @file      toe_iperf.cpp
 * @brief     W5500 TOE (Hardware Socket) throughput test implementation
 */

#include "toe_iperf.h"
#include "w5500_base.h"
#include "utilities.h" // For pin definitions

static bool is_running = false;
static TaskHandle_t iperf_task_handle = NULL;

#define W5500_S1      0x01

// -------------------------------------------------------------
// Core Task Definitions
// -------------------------------------------------------------

static void toe_iperf_server_task(void* pvParameters) {
    toe_iperf_cfg_t* cfg = (toe_iperf_cfg_t*)pvParameters;
    uint8_t socket = W5500_S1;
    uint64_t total_bytes = 0;
    uint32_t start_time = 0;
    bool connected = false;
    uint8_t* rx_buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_DMA);
    uint32_t end_time = 0;
    uint32_t duration_ms = 0;
    float mbps = 0;
    
    Serial.printf("[TOE-iPerf] Server starting on Socket %d, Port %d (%s)...\n", socket, cfg->port, cfg->is_udp ? "UDP" : "TCP");

    // Initialize Socket Mode
    w5500_write(Sn_MR, socket, cfg->is_udp ? Sn_MR_UDP : Sn_MR_TCP);
    w5500_write16(Sn_PORT, socket, cfg->port);
    w5500_write(Sn_CR, socket, Sn_CR_OPEN);
    wait_for_cmd(socket);

    if (cfg->is_udp) {
        if (w5500_read(Sn_SR, socket) != SOCK_UDP) {
            Serial.println("[TOE-iPerf] Failed to open UDP socket.");
            goto exit;
        }
        Serial.println("[TOE-iPerf] Server socket open (UDP). Waiting for packets...");
        connected = true; // For UDP, we just start measuring immediately upon rx
        start_time = millis();
    } else {
        if (w5500_read(Sn_SR, socket) != SOCK_INIT) {
            Serial.println("[TOE-iPerf] Failed to open TCP socket.");
            goto exit;
        }

        w5500_write(Sn_CR, socket, Sn_CR_LISTEN);
        wait_for_cmd(socket);

        if (w5500_read(Sn_SR, socket) != SOCK_LISTEN) {
            Serial.println("[TOE-iPerf] Failed to listen (TCP).");
            goto exit;
        }
        Serial.println("[TOE-iPerf] Server listening (TCP)...");
    }
    
    while (is_running) {
        uint8_t sr = w5500_read(Sn_SR, socket);
        
        if ((!cfg->is_udp && sr == SOCK_ESTABLISHED) || (cfg->is_udp && sr == SOCK_UDP)) {
            if (!cfg->is_udp && !connected) {
                connected = true;
                start_time = millis();
                total_bytes = 0;
                Serial.println("[TOE-iPerf] TCP Client connected, starting test...");
            }
            
            // In UDP mode, the first 8 bytes of each packet identify the remote IP/Port and length.
            // For raw speed testing, we just dequeue all bytes to empty the buffer.
            uint16_t rx_len = w5500_read16(Sn_RX_RSR, socket);
            if (rx_len > 0) {
                uint16_t read_len = (rx_len > 4096) ? 4096 : rx_len;
                w5500_read_data(socket, rx_buffer, read_len);
                total_bytes += read_len;
                
                // For UDP, we reset the timer continuously until traffic stops for a bit
                if(cfg->is_udp) {
                     if (total_bytes == read_len) {
                         start_time = millis(); // First packet sets start time
                     }
                     end_time = millis(); // constantly update end time on rx
                }
            } else {
                vTaskDelay(1); // Yield if no data
                
                // For UDP, detect end of test by silence (e.g., 2 seconds of no rx)
                if (cfg->is_udp && total_bytes > 0 && (millis() - end_time > 2000)) {
                    duration_ms = end_time - start_time;
                    mbps = 0;
                    if(duration_ms > 0) {
                        mbps = (total_bytes * 8.0) / (duration_ms * 1000.0);
                    }
                    Serial.println("\r\n--- TOE iPerf Server Rx Report (UDP) ---");
                    Serial.printf("Duration: %lu ms\n", duration_ms);
                    Serial.printf("Transferred: %llu Bytes (%.2f MB)\n", total_bytes, total_bytes/(1024.0*1024.0));
                    Serial.printf("Bandwidth: %.2f Mbps\n", mbps);
                    Serial.println("----------------------------------------");
                    total_bytes = 0; // reset for next burst
                }
            }
        } 
        else if (!cfg->is_udp && (sr == SOCK_CLOSE_WAIT || sr == SOCK_CLOSED)) {
            if (connected) {
                // Connection closed by TCP peer
                end_time = millis();
                duration_ms = end_time - start_time;
                mbps = 0;
                if(duration_ms > 0) {
                    mbps = (total_bytes * 8.0) / (duration_ms * 1000.0);
                }
                Serial.println("\r\n--- TOE iPerf Server Rx Report (TCP) ---");
                Serial.printf("Duration: %lu ms\n", duration_ms);
                Serial.printf("Transferred: %llu Bytes (%.2f MB)\n", total_bytes, total_bytes/(1024.0*1024.0));
                Serial.printf("Bandwidth: %.2f Mbps\n", mbps);
                Serial.println("----------------------------------------");
                connected = false;
                
                // Re-open server socket for next TCP connection
                w5500_write(Sn_CR, socket, Sn_CR_DISCON);
                wait_for_cmd(socket);
                w5500_write(Sn_CR, socket, Sn_CR_CLOSE);
                wait_for_cmd(socket);
                
                w5500_write(Sn_MR, socket, Sn_MR_TCP);
                w5500_write16(Sn_PORT, socket, cfg->port);
                w5500_write(Sn_CR, socket, Sn_CR_OPEN);
                wait_for_cmd(socket);
                w5500_write(Sn_CR, socket, Sn_CR_LISTEN);
                wait_for_cmd(socket);
            } else if (sr == SOCK_CLOSED) {
                // Should not happen, re-listen
                w5500_write(Sn_CR, socket, Sn_CR_OPEN);
                wait_for_cmd(socket);
                w5500_write(Sn_CR, socket, Sn_CR_LISTEN);
                wait_for_cmd(socket);
            }
        }
        else {
             vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    free(rx_buffer);
    
exit:
    w5500_write(Sn_CR, socket, Sn_CR_CLOSE);
    wait_for_cmd(socket);
    is_running = false;
    free(cfg);
    iperf_task_handle = NULL;
    vTaskDelete(NULL);
}

static void toe_iperf_client_task(void* pvParameters) {
    toe_iperf_cfg_t* cfg = (toe_iperf_cfg_t*)pvParameters;
    uint8_t socket = W5500_S1;
    int ip[4];
    uint64_t total_bytes = 0;
    uint32_t start_time = 0;
    uint8_t* tx_buffer = NULL;
    uint32_t duration_ms = 0;
    float mbps = 0;
    
    // Parse target IP
    sscanf(cfg->target_ip, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
    
    Serial.printf("[TOE-iPerf] Client starting to %s:%d (%s)...\n", cfg->target_ip, cfg->port, cfg->is_udp ? "UDP" : "TCP");

    w5500_write(Sn_MR, socket, cfg->is_udp ? Sn_MR_UDP : Sn_MR_TCP);
    
    // Set random source port, say 5555
    w5500_write16(Sn_PORT, socket, 5555);
    w5500_write(Sn_CR, socket, Sn_CR_OPEN);
    wait_for_cmd(socket);

    uint8_t sr_status = w5500_read(Sn_SR, socket);
    if (!cfg->is_udp && sr_status != SOCK_INIT) {
        Serial.printf("[TOE-iPerf] Failed to open TCP socket. Status: %02x\n", sr_status);
        goto exit;
    } else if (cfg->is_udp && sr_status != SOCK_UDP) {
        Serial.printf("[TOE-iPerf] Failed to open UDP socket. Status: %02x\n", sr_status);
        goto exit;
    }

    // Set destination IP/Port
    w5500_write(Sn_DIPR, socket, ip[0]);
    w5500_write(Sn_DIPR+1, socket, ip[1]);
    w5500_write(Sn_DIPR+2, socket, ip[2]);
    w5500_write(Sn_DIPR+3, socket, ip[3]);
    w5500_write16(Sn_DPORT, socket, cfg->port);

    if (!cfg->is_udp) {
        w5500_write(Sn_CR, socket, Sn_CR_CONNECT);
        wait_for_cmd(socket);

        // Wait for connection
        while (is_running) {
            uint8_t sr = w5500_read(Sn_SR, socket);
            if (sr == SOCK_ESTABLISHED) {
                break;
            }
            if (sr == SOCK_CLOSED) {
                 Serial.println("[TOE-iPerf] Connection failed/refused.");
                 goto exit;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        Serial.println("[TOE-iPerf] Client connected (TCP)! Sending data...");
    } else {
        Serial.println("[TOE-iPerf] Client ready (UDP)! Sending data...");
    }
    
    start_time = millis();
    tx_buffer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_DMA);
    if(tx_buffer) memset(tx_buffer, 0xAB, 4096); // Dummy data
    
    while (is_running && tx_buffer) {
        uint8_t sr = w5500_read(Sn_SR, socket);
        
        if (!cfg->is_udp && (sr == SOCK_CLOSED || sr == SOCK_CLOSE_WAIT)) {
             break;
        }
        
        // For UDP, we need to manually write the destination IP and PORT header
        // if using W5500 raw MACRAW, but since we are using UDP mode (Sn_MR_UDP),
        // we write to Sn_DPORT and Sn_DIPR and just send the payload using SEND command.
        uint16_t free_len = w5500_read16(Sn_TX_FSR, socket);
        if (free_len > 0) {
            uint16_t send_len = (free_len > 4096) ? 4096 : free_len;
            w5500_write_data(socket, tx_buffer, send_len);
            total_bytes += send_len;
        } else {
            vTaskDelay(1);
        }
            
        // Limit test time to 10 seconds
        if (millis() - start_time > 10000) {
             break;
        }
    }
    
    duration_ms = millis() - start_time;
    if(duration_ms > 0) {
        mbps = (total_bytes * 8.0) / (duration_ms * 1000.0);
    }
    Serial.printf("\r\n--- TOE iPerf Client Tx Report (%s) ---\n", cfg->is_udp ? "UDP" : "TCP");
    Serial.printf("Duration: %lu ms\n", duration_ms);
    Serial.printf("Transferred: %llu Bytes (%.2f MB)\n", total_bytes, total_bytes/(1024.0*1024.0));
    Serial.printf("Bandwidth: %.2f Mbps\n", mbps);
    Serial.println("----------------------------------------");
    
    if(tx_buffer) free(tx_buffer);
    
exit:
    w5500_write(Sn_CR, socket, Sn_CR_DISCON);
    wait_for_cmd(socket);
    w5500_write(Sn_CR, socket, Sn_CR_CLOSE);
    wait_for_cmd(socket);
    
    is_running = false;
    free(cfg);
    iperf_task_handle = NULL;
    vTaskDelete(NULL);
}


bool toe_iperf_init(const toe_iperf_cfg_t* cfg) {
    if (!w5500_hw_init()) return false;

    // RAM Reallocation: Socket 1 to 16KB TX/RX
    w5500_write(0x001E, 1, 16); // Sn_RXBUF_SIZE
    w5500_write(0x001F, 1, 16); // Sn_TXBUF_SIZE
    for(int i=0; i<8; i++) {
        if(i != 1) {
            w5500_write(0x001E, i, 0); 
            w5500_write(0x001F, i, 0);
        }
    }

    // Network Settings
    uint32_t ip = cfg->local_ip;
    uint32_t gw = cfg->gateway;
    uint32_t netmask = cfg->subnet;
    
    w5500_write(W5500_MR, 0x00, 0x00);
    w5500_write_buf(W5500_SHAR, 0x00, cfg->mac_addr, 6);
    w5500_write_buf(W5500_GAR, 0x00, (uint8_t*)&gw, 4);
    w5500_write_buf(W5500_SUBR, 0x00, (uint8_t*)&netmask, 4);
    w5500_write_buf(W5500_SIPR, 0x00, (uint8_t*)&ip, 4);

    return true;
}

bool toe_iperf_start(const toe_iperf_cfg_t* cfg) {
    if (is_running) return false;
    
    if(!toe_iperf_init(cfg)) return false;
    
    // Copy cfg since it may be stack allocated by the caller
    toe_iperf_cfg_t* cfg_copy = (toe_iperf_cfg_t*)malloc(sizeof(toe_iperf_cfg_t));
    if(!cfg_copy) return false;
    memcpy(cfg_copy, cfg, sizeof(toe_iperf_cfg_t));
    
    if (cfg->target_ip) {
       // Make a dynamic copy of the IP string
       char* ip_copy = (char*)malloc(strlen(cfg->target_ip) + 1);
       strcpy(ip_copy, cfg->target_ip);
       cfg_copy->target_ip = ip_copy;
    }
    
    is_running = true;
    
    if (cfg->is_server) {
        xTaskCreate(toe_iperf_server_task, "toe_server", 4096, cfg_copy, 5, &iperf_task_handle);
    } else {
        xTaskCreate(toe_iperf_client_task, "toe_client", 4096, cfg_copy, 5, &iperf_task_handle);
    }
    
    return true;
}

void toe_iperf_stop() {
    is_running = false;
    delay(100);
    w5500_hw_deinit();
}

bool toe_iperf_is_running() {
    return is_running;
}
