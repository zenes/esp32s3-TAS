/**
 * @file      eth2ap.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co.,
 * Ltd
 * @date      2024-02-03
 * @note      Converted from ESP-IDF eth2ap example
 *            This example bridges Ethernet to WiFi AP using NAPT
 */

#include <Arduino.h>
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <ETHClass2.h> //Is to use the modified ETHClass
#define ETH ETH2
#else
#include <ETH.h>
#endif
#include "utilities.h" //Board PinMap
#include <WiFi.h>
#include <ESP32Ping.h>
#include <SPI.h>
#include <esp_wifi.h> // For esp_wifi_set_bandwidth
#include <esp_system.h> // For esp_reset_reason
#include <rom/rtc.h>    // For rtc_get_reset_reason
#include <TFT_eSPI.h> // LCD Library
#include <lvgl.h>    // LVGL Graphic Library
#include <stdarg.h>  // For va_list
#include <Preferences.h> // For NVS storage
#include <esp_mac.h>     // For esp_read_mac
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <lwip/prot/ip.h>
#include <lwip/prot/ip4.h>
#include <lwip/prot/udp.h>
#include <lwip/prot/tcp.h>
#include <lwip/apps/lwiperf.h>
#include <AsyncUDP.h>
#include <inttypes.h>
#include "w5500_base.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"

// Global Traffic Stats
struct InterfaceStats {
    uint64_t rx_bytes = 0;
    uint64_t tx_bytes = 0;
    uint32_t rx_packets = 0;
    uint32_t tx_packets = 0;
};

// Log System Configuration
enum LogLevel { LOG_NONE = 0, LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };
static LogLevel current_log_level = LOG_INFO;

// Dmesg Ring Buffer
#define DMESG_MAX_LINES 50
#define DMESG_MAX_LEN 128

struct DmesgEntry {
    unsigned long timestamp;
    LogLevel level;
    char message[DMESG_MAX_LEN];
};

static DmesgEntry dmesg_buffer[DMESG_MAX_LINES];
static int dmesg_head = 0;
static int dmesg_tail = 0;
static int dmesg_count = 0;

void addDmesg(LogLevel level, const char* msg) {
    dmesg_buffer[dmesg_head].timestamp = millis();
    dmesg_buffer[dmesg_head].level = level;
    strncpy(dmesg_buffer[dmesg_head].message, msg, DMESG_MAX_LEN - 1);
    dmesg_buffer[dmesg_head].message[DMESG_MAX_LEN - 1] = '\0';
    
    dmesg_head = (dmesg_head + 1) % DMESG_MAX_LINES;
    if (dmesg_count < DMESG_MAX_LINES) {
        dmesg_count++;
    } else {
        dmesg_tail = (dmesg_tail + 1) % DMESG_MAX_LINES;
    }
}

void logMsg(LogLevel level, const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  
  // Always add to dmesg buffer
  addDmesg(level, buf);

  if (level <= current_log_level) {
    switch(level) {
      case LOG_ERROR: Serial.print("[ERROR] "); break;
      case LOG_WARN:  Serial.print("[WARN ] "); break;
      case LOG_INFO:  Serial.print("[INFO ] "); break;
      case LOG_DEBUG: Serial.print("[DEBUG] "); break;
      default: break;
    }
    Serial.print(buf);
    Serial.print("\r\n");
  }
}

// Interface instances


InterfaceStats eth_stats;
InterfaceStats ap_stats;
bool traffic_log_enabled = false;

// Hook Function types
typedef err_t (*netif_input_fn)(struct pbuf *p, struct netif *inp);
typedef err_t (*netif_linkoutput_fn)(struct netif *netif, struct pbuf *p);

netif_input_fn orig_eth_input = NULL;
netif_linkoutput_fn orig_eth_linkoutput = NULL;
netif_input_fn orig_ap_input = NULL;
netif_linkoutput_fn orig_ap_linkoutput = NULL;

void logPacket(const char* iface, bool rx, struct pbuf* p) {
    if (!traffic_log_enabled) return;
    
    uint8_t *payload = (uint8_t *)p->payload;
    struct ip_hdr *iphdr = NULL;

    // Detection logic:
    // 1. Direct IPv4 (ihl and version)
    if (p->len >= 20 && (payload[0] & 0xF0) == 0x40) {
        iphdr = (struct ip_hdr *)payload;
    }
    // 2. Ethernet Encapsulated IPv4 (EtherType 0x0800 at offset 12)
    else if (p->len >= 34 && payload[12] == 0x08 && payload[13] == 0x00 && (payload[14] & 0xF0) == 0x40) {
        iphdr = (struct ip_hdr *)(payload + 14);
    }

    if (iphdr) {
        char src_ip[16], dst_ip[16];
        ip4addr_ntoa_r((const ip4_addr_t*)&(iphdr->src), src_ip, sizeof(src_ip));
        ip4addr_ntoa_r((const ip4_addr_t*)&(iphdr->dest), dst_ip, sizeof(dst_ip));
        
        const char* proto = "OTHER";
        uint8_t p_type = IPH_PROTO(iphdr);
        if (p_type == IP_PROTO_ICMP) proto = "ICMP";
        else if (p_type == IP_PROTO_TCP) proto = "TCP";
        else if (p_type == IP_PROTO_UDP) proto = "UDP";
        
        Serial.printf("[%s %s] %s -> %s (%s, %d bytes)\r\n", 
                      iface, rx ? "RX" : "TX", src_ip, dst_ip, proto, p->tot_len);
    }
}

// Helper to print 64-bit values accurately
String formatBytes(uint64_t bytes) {
    if (bytes < 1024) return String((unsigned long)bytes) + " B";
    else if (bytes < 1024 * 1024) return String((unsigned long)(bytes / 1024)) + " KB";
    else return String((unsigned long)(bytes / (1024 * 1024))) + " MB";
}

err_t eth_input_hook(struct pbuf *p, struct netif *inp) {
    eth_stats.rx_bytes += p->tot_len;
    eth_stats.rx_packets++;
    logPacket("ETH", true, p);
    return orig_eth_input(p, inp);
}

err_t eth_linkoutput_hook(struct netif *netif, struct pbuf *p) {
    eth_stats.tx_bytes += p->tot_len;
    eth_stats.tx_packets++;
    logPacket("ETH", false, p);
    return orig_eth_linkoutput(netif, p);
}

err_t ap_input_hook(struct pbuf *p, struct netif *inp) {
    ap_stats.rx_bytes += p->tot_len;
    ap_stats.rx_packets++;
    logPacket("AP ", true, p);
    return orig_ap_input(p, inp);
}

err_t ap_linkoutput_hook(struct netif *netif, struct pbuf *p) {
    ap_stats.tx_bytes += p->tot_len;
    ap_stats.tx_packets++;
    logPacket("AP ", false, p);
    return orig_ap_linkoutput(netif, p);
}


// WiFi AP Configuration - Base values
#define AP_SSID_BASE "eth2ap"
#define AP_PASSWORD_BASE "12345678"
#define AP_CHANNEL 1
#define AP_MAX_CONN 4

// Log System moved up

// Shell Configuration
static String inputString = "";
static bool stringComplete = false;
static String ap_ssid_custom = AP_SSID_BASE;
static String ap_pw_custom = AP_PASSWORD_BASE;

Preferences preferences;

// Traffic Monitoring State
static uint32_t last_eth_rx = 0;
static uint32_t last_eth_tx = 0;
static uint32_t last_ap_rx = 0;
static uint32_t last_ap_tx = 0;
static unsigned long last_speed_check = 0;
// Shell & Monitoring Configuration
static bool monitor_traffic = false;
static bool monitor_enabled = false;
static int cursor_pos = 0;

// iPerf State
static void* lwiperf_session = NULL;
static void lwiperf_report(void* arg, enum lwiperf_report_type report_type,
                           const ip_addr_t* local_addr, u16_t local_port, const ip_addr_t* remote_addr, u16_t remote_port,
                           u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec) {
    
    float mbps = bandwidth_kbitpsec / 1000.0;
    float mb_s = mbps / 8.0;
    float total_mb = bytes_transferred / (1024.0 * 1024.0);

    Serial.printf("\r\n--- iPerf TCP Report ---\n");
    Serial.printf("Type: %d\n", (int)report_type);
    Serial.printf("Duration: %lu ms\n", ms_duration);
    Serial.printf("Transferred: %lu Bytes (%.2f MB)\n", bytes_transferred, total_mb);
    Serial.printf("Bandwidth: %.2f Mbps (%.2f MB/s)\n", mbps, mb_s);
    Serial.printf("------------------------\n> ");
    // Unset the session if it's done or aborted
    if (report_type == LWIPERF_TCP_DONE_SERVER || report_type == LWIPERF_TCP_ABORTED_LOCAL || 
        report_type == LWIPERF_TCP_ABORTED_LOCAL_DATAERROR || report_type == LWIPERF_TCP_ABORTED_LOCAL_TXERROR || 
        report_type == LWIPERF_TCP_ABORTED_REMOTE) {
         lwiperf_session = NULL;
    }
}

// UDP iPerf State
static AsyncUDP udp_iperf_server;
static bool udp_iperf_running = false;
static volatile uint32_t udp_iperf_bytes = 0;
static uint32_t udp_iperf_last_print = 0;

static void onUDPPacket(AsyncUDPPacket packet) {
  udp_iperf_bytes += packet.length();
}

// Ping State
static bool ping_running = false;
static String ping_target = "";
static unsigned long last_ping_time = 0;

void refreshLine() {
  // Move cursor to beginning of line (using \r)
  Serial.print("\r> ");
  // Print current inputString
  Serial.print(inputString);
  // Clear any leftover characters from previous longer string
  Serial.print("\033[K"); 
  // Move cursor back to current cursor_pos
  if (cursor_pos < inputString.length()) {
    Serial.printf("\033[%dD", (int)(inputString.length() - cursor_pos));
  }
}

void installHooks() {
  esp_netif_t* eth_netif = ETH.netif();
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
#else
  esp_netif_t* ap_netif = WiFi.AP.netif();
#endif

  if (eth_netif && orig_eth_input == NULL) {
    struct netif* lwip_eth = (struct netif*)esp_netif_get_netif_impl(eth_netif);
    if (lwip_eth) {
      orig_eth_input = lwip_eth->input;
      orig_eth_linkoutput = lwip_eth->linkoutput;
      lwip_eth->input = eth_input_hook;
      lwip_eth->linkoutput = eth_linkoutput_hook;
      logMsg(LOG_INFO, "ETH Hooks installed (Handle: %p, netif: %p)", eth_netif, lwip_eth);
    }
  }

  if (ap_netif && orig_ap_input == NULL) {
    struct netif* lwip_ap = (struct netif*)esp_netif_get_netif_impl(ap_netif);
    if (lwip_ap) {
      orig_ap_input = lwip_ap->input;
      orig_ap_linkoutput = lwip_ap->linkoutput;
      lwip_ap->input = ap_input_hook;
      lwip_ap->linkoutput = ap_linkoutput_hook;
      logMsg(LOG_INFO, "AP Hooks installed (Handle: %p, netif: %p)", ap_netif, lwip_ap);
    }
  }
}

// Command List for Autocomplete
const char* shell_commands[] = {
  "help", "status", "dmesg", "loglevel", "monitor", "restart", "set_ssid", "set_pw", "stats", "traffic", "ping", "ifconfig", "arp", "dhcp", "iperf", "udp_iperf", "toe_iperf"
};
const int shell_cmd_count = sizeof(shell_commands) / sizeof(shell_commands[0]);

// Shell History
#define MAX_HISTORY 10
static String shell_history[MAX_HISTORY];
static int history_count = 0;
static int history_index = -1;

// Autocomplete Cycling
static int tab_match_index = -1;
static String tab_prefix = "";

// ANSI Sequence Parser State
static enum { ANSI_NONE, ANSI_ESC, ANSI_BRACKET } ansi_state = ANSI_NONE;

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
#define ENABLE_LCD // Enable logic, but will be checked dynamically
static bool lcd_initialized = false;
static bool lcd_detected = false;

/* LVGL Double Buffers (Internal SRAM with DMA) */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL; 
static lv_color_t *buf2 = NULL; 

/* UI Objects */
static lv_obj_t *ui_label_eth;
static lv_obj_t *ui_label_ap;
static lv_obj_t *ui_label_clients;
static lv_obj_t *ui_label_fps;
static lv_obj_t *ui_test_rect;
static uint32_t frame_cnt = 0;
static uint32_t fps_val = 0;

/* Timer callback to force Full Screen UI changes */
void move_test_rect_cb(lv_timer_t * t) {
  static uint8_t c = 0;
  c += 10;
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(c, 100, 200), 0);
}

#define TE_PIN 21
static SemaphoreHandle_t te_semaphore = NULL;

void IRAM_ATTR te_isr_handler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (te_semaphore != NULL) {
        // 이미 차있더라도 덮어씀 (안전한 펄스 기록)
        xSemaphoreGiveFromISR(te_semaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/* Display flushing callback (Asynchronous DMA) */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  static bool transfer_open = false;

  // 1. Wait for PREVIOUS DMA transfer to finish, then close the SPI transaction
  if (transfer_open) {
    if (tft.dmaBusy()) tft.dmaWait();
    tft.endWrite(); // Resets CS pin HIGH (SPI Sync reset)
  }

  // 2. Start new transfer session (CS pin LOW)
  tft.startWrite(); 
  tft.setAddrWindow(area->x1, area->y1, w, h);

  // V-Sync(TE) 강제 대기 코드를 주석 처리 (원래 53 FPS의 논블로킹 속도 복구)
  // 부분 버퍼(64라인) 구조의 태생적 물리 한계상 이 대기 코드는 프레임레이트를 무조건 떨어뜨리면서도 티어링을 100% 잡지 못함.
  /*
  static bool wait_for_te = true;
  if (wait_for_te && te_semaphore != NULL) {
      xSemaphoreTake(te_semaphore, pdMS_TO_TICKS(100));
  }
  wait_for_te = lv_disp_flush_is_last(disp);
  */

  tft.pushPixelsDMA((uint16_t *)color_p, w * h);
  
  // CRITICAL: We DO NOT call tft.endWrite() here!
  // Instead, we mark it open so the NEXT flush closes it to prevent SPI lock-up.
  transfer_open = true;
  
  frame_cnt++;
  lv_disp_flush_ready(disp); // Tell LVGL we are ready for the NEXT frame buffer
}

volatile uint32_t last_render_time_ms = 0;

// LVGL 모니터링 콜백 (렌더링 소요 시간 직관적 측정용)
void my_monitor_cb(lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px) {
  // 화면 갱신 시 소요된 렌더링 시간을 저장
  if (time > 0) {
    last_render_time_ms = time;
  }
}

// Network state
// ============================================================================
// Network Configuration (Static IP)
// Set USE_STATIC_IP to true if you want to use a static IP address.
// This is useful if DHCP is not available or slow (Example: Corporate Network).
// ============================================================================
#define USE_STATIC_IP true // Set to true to enable Static IP

#if USE_STATIC_IP
// Please change these values to match your network environment
IPAddress local_ip(192, 168, 0, 50);
IPAddress gateway(192, 168, 0, 100); // Set gateway to Device A (Direct Connect)
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
#endif

// Network state
static bool eth_connected = false;
static bool ap_started = false;

/**
 * @brief Dynamic LCD hardware detection
 * @return true if LCD is detected, false otherwise
 */
bool detectLCD() {
  Serial.print("Probing LCD hardware... ");
  
  // Use SPI2 (FSPI) to avoid conflict with Ethernet (SPI3)
  static SPIClass* spi_p = nullptr;
  if (!spi_p) spi_p = new SPIClass(FSPI); 
  spi_p->begin(LCD_SCLK_PIN, LCD_MISO_PIN, LCD_MOSI_PIN, LCD_CS_PIN);
  
  pinMode(LCD_MISO_PIN, INPUT_PULLUP);
  pinMode(LCD_CS_PIN, OUTPUT);
  pinMode(LCD_DC_PIN, OUTPUT);
  pinMode(LCD_RST_PIN, OUTPUT);

  // Hardware Reset
  digitalWrite(LCD_RST_PIN, LOW);
  delay(100); 
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(200); 
  
  digitalWrite(LCD_CS_PIN, LOW);
  delay(5); 
  
  spi_p->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  
  // Multiple ID probe (ST7789 requires 1-byte dummy before parameter)
  auto readID = [&](uint8_t cmd) {
    digitalWrite(LCD_DC_PIN, LOW); spi_p->transfer(cmd);
    digitalWrite(LCD_DC_PIN, HIGH);
    spi_p->transfer(0x00); // Dummy Byte
    return spi_p->transfer(0x00); // Real ID
  };

  uint8_t id1 = readID(0xDA);
  uint8_t id2 = readID(0xDB);
  uint8_t id3 = readID(0xDC);
  
  spi_p->endTransaction();
  digitalWrite(LCD_CS_PIN, HIGH);
  
  bool detected = (id1 != 0x00 && id1 != 0xFF) || (id2 != 0x00 && id2 != 0xFF) || (id3 != 0x00 && id3 != 0xFF);

  if (detected) {
    Serial.printf("Done. LCD Detected (ID:%02X%02X%02X)\n", id1, id2, id3);
  } else {
    Serial.println("Failed. LCD not found.");
  }
  
  return detected;
}

/**
 * @brief Initialize LVGL UI widgets
 */
void initLVGLUI() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  
  // 화면 잘림 현상을 확인하기 위한 전체 테두리 (디버깅용)
  lv_obj_set_style_border_color(scr, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_border_width(scr, 2, 0);
  lv_obj_set_style_pad_all(scr, 0, 0); // 패딩 제거로 모서리까지 꽉 차게 변경

  /* Title */
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "ETH2AP Bridge");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_YELLOW), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  /* FPS Label */
  ui_label_fps = lv_label_create(scr);
  lv_obj_set_style_text_font(ui_label_fps, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui_label_fps, lv_palette_main(LV_PALETTE_CYAN), 0);
  lv_obj_align(ui_label_fps, LV_ALIGN_TOP_RIGHT, -10, 10);
  lv_label_set_text(ui_label_fps, "FPS: 0");

  /* Color Test Boxes (Bottom) */
  static lv_color_t test_colors[] = {LV_COLOR_MAKE(255,0,0), LV_COLOR_MAKE(0,255,0), LV_COLOR_MAKE(0,0,255), LV_COLOR_MAKE(255,255,255), LV_COLOR_MAKE(0,0,0)};
  const char* color_names[] = {"R", "G", "B", "W", "K"};
  
  for(int i=0; i<5; i++) {
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 30, 20);
    lv_obj_set_style_bg_color(box, test_colors[i], 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(box, LV_ALIGN_BOTTOM_LEFT, 10 + (i * 35), -10);
    
    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, color_names[i]);
    lv_obj_center(lbl);
    if(i == 3) lv_obj_set_style_text_color(lbl, lv_color_black(), 0); // Black text for White box
    else lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
  }

  /* Separator */
  static lv_point_t line_points[] = {{0, 0}, {LCD_HEIGHT, 0}}; // Landscape orientation
  lv_obj_t *line = lv_line_create(scr);
  lv_line_set_points(line, line_points, 2);
  lv_obj_set_style_line_color(line, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 35);

  /* Ethernet Label */
  ui_label_eth = lv_label_create(scr);
  lv_label_set_recolor(ui_label_eth, true);
  lv_obj_set_style_text_color(ui_label_eth, lv_color_white(), 0);
  lv_obj_align(ui_label_eth, LV_ALIGN_TOP_LEFT, 10, 50);

  /* Access Point Label */
  ui_label_ap = lv_label_create(scr);
  lv_label_set_recolor(ui_label_ap, true);
  lv_obj_set_style_text_color(ui_label_ap, lv_color_white(), 0);
  lv_obj_align(ui_label_ap, LV_ALIGN_TOP_LEFT, 10, 80);

  /* Clients Info */
  ui_label_clients = lv_label_create(scr);
  lv_obj_set_style_text_color(ui_label_clients, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_obj_align(ui_label_clients, LV_ALIGN_TOP_LEFT, 10, 110);

  /* Animation Bar for FPS Verification */
  lv_obj_t *bar = lv_bar_create(scr);
  lv_obj_set_size(bar, 100, 5);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_bar_set_range(bar, 0, 100);
  
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, bar);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_set_time(&a, 1000);
  lv_anim_set_playback_time(&a, 1000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
  lv_anim_start(&a);

  /* 30FPS Test Moving Rect */
  ui_test_rect = lv_obj_create(scr);
  lv_obj_set_size(ui_test_rect, 15, 15);
  lv_obj_set_style_bg_color(ui_test_rect, lv_palette_main(LV_PALETTE_YELLOW), 0);
  lv_obj_set_style_border_width(ui_test_rect, 0, 0);
  lv_obj_align(ui_test_rect, LV_ALIGN_CENTER, 0, 50);
  
  // Create 30Hz timer (1000/30 = 33.3ms)
  lv_timer_create(move_test_rect_cb, 33, NULL);
}

/**
 * @brief Update LCD with current network status (LVGL version)
 */
void updateLCD() {
#ifdef ENABLE_LCD
  if (!lcd_initialized) {
    return;
  }

  // Update Ethernet Status Label
  if (eth_connected) {
    char buf_eth[64];
    snprintf(buf_eth, sizeof(buf_eth), "ETH: #00FF00 Connected#\n IP: %s", ETH.localIP().toString().c_str());
    lv_label_set_text(ui_label_eth, buf_eth);
  } else {
    lv_label_set_text(ui_label_eth, "ETH: #FF0000 Disconnected#");
  }

  // Update AP Status Label
  if (ap_started) {
    char buf_ap[64];
    snprintf(buf_ap, sizeof(buf_ap), "AP: #00FF00 Running#\n SSID: %s", ap_ssid_custom.c_str());
    lv_label_set_text(ui_label_ap, buf_ap);

    char buf_clients[32];
    snprintf(buf_clients, sizeof(buf_clients), "Clients: %d / %d", WiFi.softAPgetStationNum(), AP_MAX_CONN);
    lv_label_set_text(ui_label_clients, buf_clients);
  } else {
    lv_label_set_text(ui_label_ap, "AP: #FF0000 Stopped#");
    lv_label_set_text(ui_label_clients, "");
  }
#endif
}

/**
 * @brief Handle Serial Shell Commands
 */
void handleShell() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();

    // ANSI Escape Sequence Parser
    if (ansi_state == ANSI_NONE) {
      if (inChar == 0x1B) { // ESC
        ansi_state = ANSI_ESC;
        continue;
      }
    } else if (ansi_state == ANSI_ESC) {
      if (inChar == '[') {
        ansi_state = ANSI_BRACKET;
      } else {
        ansi_state = ANSI_NONE;
      }
      continue;
    } else if (ansi_state == ANSI_BRACKET) {
      ansi_state = ANSI_NONE;
      if (inChar == 'A') { // UP Arrow
        if (history_count > 0) {
          if (history_index == -1) history_index = history_count - 1;
          else if (history_index > 0) history_index--;
          
          inputString = shell_history[history_index];
          cursor_pos = inputString.length();
          refreshLine();
        }
        continue;
      }
      else if (inChar == 'B') { // DOWN Arrow
        if (history_count > 0 && history_index != -1) {
          history_index++;
          if (history_index >= history_count) {
            history_index = -1;
            inputString = "";
          } else {
            inputString = shell_history[history_index];
          }
          cursor_pos = inputString.length();
          refreshLine();
        }
        continue;
      }
      else if (inChar == 'C') { // RIGHT Arrow
        if (cursor_pos < inputString.length()) {
          cursor_pos++;
          Serial.print("\033[C"); // ANSI Right
        }
        continue;
      }
      else if (inChar == 'D') { // LEFT Arrow
        if (cursor_pos > 0) {
          cursor_pos--;
          Serial.print("\033[D"); // ANSI Left
        }
        continue;
      }
      continue;
    }
    
    // Ctrl+C (Cancel)
    if (inChar == 0x03) {
      if (ping_running) {
        ping_running = false;
        Serial.print("^C\r\nStopping ping.\r\n> ");
      } else {
        Serial.print("^C\r\n> ");
      }
      inputString = "";
      cursor_pos = 0;
      history_index = -1;
      tab_match_index = -1;
      tab_prefix = "";
      continue;
    }

    // Reset Tab Cycling if anything else is pressed
    if (inChar != 0x09) {
      tab_match_index = -1;
      tab_prefix = "";
    }

    // Echo back the character
    if (inChar == '\n' || inChar == '\r') {
      Serial.print("\r\n"); // New line on enter
      if (inputString.length() > 0) {
        // Save to history
        if (history_count == 0 || shell_history[history_count-1] != inputString) {
          if (history_count < MAX_HISTORY) {
            shell_history[history_count++] = inputString;
          } else {
            for (int i = 0; i < MAX_HISTORY - 1; i++) shell_history[i] = shell_history[i+1];
            shell_history[MAX_HISTORY-1] = inputString;
          }
        }
        history_index = -1;
        cursor_pos = 0; // Reset for next command
        stringComplete = true;
      } else {
        Serial.print("> "); // Empty line, just show prompt
        cursor_pos = 0;
      }
    } 
    else if (inChar == 0x09) { // Tab key
      if (inputString.length() > 0) {
        // First tab or cycling?
        if (tab_match_index == -1) {
          tab_prefix = inputString;
          tab_match_index = 0;
        } else {
          tab_match_index++;
        }

        String matches[shell_cmd_count];
        int matchCount = 0;
        for (int i = 0; i < shell_cmd_count; i++) {
          if (String(shell_commands[i]).startsWith(tab_prefix)) {
            matches[matchCount++] = shell_commands[i];
          }
        }

        if (matchCount > 0) {
          if (tab_match_index >= matchCount) tab_match_index = 0;
          
          inputString = matches[tab_match_index];
          cursor_pos = inputString.length();
          refreshLine();
        }
      }
    }
    else if (inChar == 0x08 || inChar == 0x7F) { // Backspace or Delete
      if (cursor_pos > 0) {
        inputString.remove(cursor_pos - 1, 1);
        cursor_pos--;
        refreshLine();
      }
    }
    else {
      // Insert character at cursor_pos
      if (cursor_pos == inputString.length()) {
        inputString += inChar;
        cursor_pos++;
        Serial.print(inChar); // Normal echo
      } else {
        String start = inputString.substring(0, cursor_pos);
        String end = inputString.substring(cursor_pos);
        inputString = start + inChar + end;
        cursor_pos++;
        refreshLine();
      }
    }
  }

  if (stringComplete) {
    inputString.trim();
    // Handled in handleShell loop already

    String cmd = inputString;
    String arg = "";
    int spaceIndex = inputString.indexOf(' ');
    if (spaceIndex != -1) {
      cmd = inputString.substring(0, spaceIndex);
      arg = inputString.substring(spaceIndex + 1);
      arg.trim();
    }

    if (cmd.equalsIgnoreCase("help")) {
      Serial.print("\n\rAvailable Commands:\r\n");
      Serial.print("  help            - Show this help\r\n");
      Serial.print("  status          - Show current system status\r\n");
      Serial.print("  dmesg [lines]   - Show system kernel logs (Ring Buffer)\r\n");
      Serial.print("  loglevel <0-4>  - Set log level (0:NONE, 1:ERR, 2:WARN, 3:INFO, 4:DEBUG)\r\n");
      Serial.print("  monitor <on/off>- Toggle periodic status printing\r\n");
      Serial.print("  restart         - Reboot system\r\n");
      Serial.print("  set_ssid <ssid> - Change WiFi AP SSID\r\n");
      Serial.print("  set_pw <pass>   - Change WiFi AP Password\r\n");
#ifdef ENABLE_ETHERNET
      Serial.print("  stats           - Show interface statistics (Bytes/Packets)\r\n");
      Serial.print("  traffic [on/off]- Show NAT sessions (if on, periodically shows speed)\r\n");
#endif
      Serial.print("  ping <host>     - Ping a host (domain or IP)\r\n");
      Serial.print("  ifconfig        - Show network interface configurations\r\n");
      Serial.print("  arp             - Show connected AP clients (MAC/RSSI)\r\n");
#ifdef ENABLE_ETHERNET
      Serial.print("  dhcp            - Show DHCP server leases (IP/MAC mappings)\r\n");
      Serial.print("  iperf <start|stop>- Start/stop TCP iPerf Server (Port 5001)\r\n");
      Serial.print("  udp_iperf <start|stop>- Start/stop UDP Speed Test (Port 5002)\r\n");
      Serial.print("  toe_iperf <start|stop> [-s | -c ip] - Start/stop W5500 TOE Speed Test\r\n");
#endif
      Serial.print("\r\n");
    } 
    else if (cmd.equalsIgnoreCase("status")) {
      Serial.print("\n\r--- System Status ---\r\n");
      Serial.printf("Ethernet: %s (IP: %s)\r\n", eth_connected ? "Connected" : "Disconnected", ETH.localIP().toString().c_str());
      Serial.printf("WiFi AP : %s (SSID: %s)\r\n", ap_started ? "Running" : "Stopped", ap_ssid_custom.c_str());
      Serial.printf("Clients : %d\r\n", WiFi.softAPgetStationNum());
      Serial.printf("Log Lev : %d\r\n", (int)current_log_level);
      Serial.printf("Monitor : %s\r\n", monitor_enabled ? "ON" : "OFF");
      Serial.printf("Uptime  : %lu sec\r\n", millis() / 1000);
      Serial.printf("CPU Freq: %u MHz\r\n", getCpuFrequencyMhz());
      Serial.printf("Flash   : %u MB\r\n", ESP.getFlashChipSize() / (1024 * 1024));
      
      // CPU Temperature (Best effort)
      float temp = temperatureRead();
      if (!isnan(temp)) {
        Serial.printf("CPU Temp: %.1f °C\r\n", temp);
      }
      
      Serial.printf("Free RAM: %u KB (Internal)\r\n", ESP.getFreeHeap() / 1024);
      Serial.printf("Min Free: %u KB (Internal Min)\r\n", ESP.getMinFreeHeap() / 1024);
      if (psramFound()) {
        Serial.printf("PSRAM   : %u / %u KB (Free/Total)\r\n", ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
      } else {
        Serial.print("PSRAM   : Not Found\r\n");
      }
      Serial.print("-----------------------------\r\n\r\n");
    }
    else if (cmd.equalsIgnoreCase("dmesg")) {
      Serial.print("\n\r--- System Logs (dmesg) ---\r\n");
      
      int max_lines = dmesg_count;
      if (arg.length() > 0) {
        int requested = arg.toInt();
        if (requested > 0 && requested < dmesg_count) {
          max_lines = requested;
        }
      }
      
      int count = 0;
      int idx = (dmesg_head - max_lines + DMESG_MAX_LINES) % DMESG_MAX_LINES;
      
      while (count < max_lines) {
        unsigned long ts = dmesg_buffer[idx].timestamp;
        LogLevel lvl = dmesg_buffer[idx].level;
        const char* msg = dmesg_buffer[idx].message;
        
        const char* lvl_str = "";
        switch(lvl) {
          case LOG_ERROR: lvl_str = "[ERROR]"; break;
          case LOG_WARN:  lvl_str = "[WARN ]"; break;
          case LOG_INFO:  lvl_str = "[INFO ]"; break;
          case LOG_DEBUG: lvl_str = "[DEBUG]"; break;
          default: break;
        }
        
        Serial.printf("[%6lu.%03lu] %s %s\r\n", ts / 1000, ts % 1000, lvl_str, msg);
        
        idx = (idx + 1) % DMESG_MAX_LINES;
        count++;
      }
      Serial.print("---------------------------\r\n\r\n");
    }
    else if (cmd.equalsIgnoreCase("monitor")) {
      if (arg.equalsIgnoreCase("on")) {
        monitor_enabled = true;
        Serial.print("Periodic monitoring: ON\r\n");
      } else if (arg.equalsIgnoreCase("off")) {
        monitor_enabled = false;
        Serial.print("Periodic monitoring: OFF\r\n");
      } else if (arg.equalsIgnoreCase("traffic")) {
        monitor_traffic = !monitor_traffic;
        Serial.printf("Traffic monitoring: %s\r\n", monitor_traffic ? "ON" : "OFF");
      } else {
        Serial.print("Usage: monitor <on/off/traffic>\r\n");
      }
    }
    else if (cmd.equalsIgnoreCase("loglevel")) {
      if (arg.length() > 0) {
        int val = arg.toInt();
        if (val >= 0 && val <= 4) {
          current_log_level = (LogLevel)val;
          Serial.printf("Log level changed to %d\r\n", val);
        } else {
          Serial.print("Error: Log level must be between 0 and 4.\r\n");
        }
      } else {
        Serial.printf("Current Log Level: %d\r\n", (int)current_log_level);
      }
    }
    else if (cmd.equalsIgnoreCase("restart")) {
      Serial.print("Restarting system...\r\n");
      delay(500);
      ESP.restart();
    } 
    else if (cmd.equalsIgnoreCase("set_ssid")) {
      if (arg.length() > 0) {
        ap_ssid_custom = arg;
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", ap_ssid_custom);
        preferences.putString("password", ap_pw_custom); // Save both for consistency
        preferences.end();
        Serial.printf("Changing SSID to '%s'. Restarting AP...\r\n", ap_ssid_custom.c_str());
        
        WiFi.softAPdisconnect(false); // Don't turn off WiFi radio, just stop AP
        delay(100);
        WiFi.softAP(ap_ssid_custom.c_str(), ap_pw_custom.c_str(), AP_CHANNEL, 0, AP_MAX_CONN);
        updateLCD();
      } else {
        Serial.print("Error: No SSID provided. Usage: set_ssid <name>\r\n");
      }
    } 
    else if (cmd.equalsIgnoreCase("set_pw")) {
      if (arg.length() >= 8) {
        ap_pw_custom = arg;
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", ap_ssid_custom);   // Save both for consistency
        preferences.putString("password", ap_pw_custom);
        preferences.end();
        Serial.printf("Changing Password to '%s'. Restarting AP...\r\n", ap_pw_custom.c_str());
        
        WiFi.softAPdisconnect(false); // Don't turn off WiFi radio, just stop AP
        delay(100);
        WiFi.softAP(ap_ssid_custom.c_str(), ap_pw_custom.c_str(), AP_CHANNEL, 0, AP_MAX_CONN);
      } else {
        Serial.print("Error: Password must be at least 8 characters. Usage: set_pw <pass>\r\n");
      }
    } 
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("stats")) {
      Serial.println("\r\n--- Interface Statistics (Live) ---");
      Serial.printf("Free RAM: %u KB, Min: %u KB\r\n", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
      
      Serial.println("[Ethernet]");
      Serial.printf("  RX: %s (%lu pkts)\r\n", formatBytes(eth_stats.rx_bytes).c_str(), eth_stats.rx_packets);
      Serial.printf("  TX: %s (%lu pkts)\r\n", formatBytes(eth_stats.tx_bytes).c_str(), eth_stats.tx_packets);
      
      Serial.println("[WiFi AP]");
      Serial.printf("  RX: %s (%lu pkts)\r\n", formatBytes(ap_stats.rx_bytes).c_str(), ap_stats.rx_packets);
      Serial.printf("  TX: %s (%lu pkts)\r\n", formatBytes(ap_stats.tx_bytes).c_str(), ap_stats.tx_packets);
      
      Serial.println("-----------------------------------\r\n");
    }
#endif
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("traffic")) {
      if (arg.equalsIgnoreCase("on")) {
        traffic_log_enabled = true;
        installHooks(); // Ensure hooks are on
        Serial.println("Real-time traffic logging: ON");
      } else if (arg.equalsIgnoreCase("off")) {
        traffic_log_enabled = false;
        Serial.println("Real-time traffic logging: OFF");
      } else {
        Serial.println("\r\n--- Connected Stations ---");
        wifi_sta_list_t stationList;
        esp_wifi_ap_get_sta_list(&stationList);
        for (int i = 0; i < stationList.num; i++) {
          char macStr[18];
          snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                   stationList.sta[i].mac[0], stationList.sta[i].mac[1], stationList.sta[i].mac[2],
                   stationList.sta[i].mac[3], stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
          Serial.printf("  [%d] MAC: %s, RSSI: %d\r\n", i+1, macStr, stationList.sta[i].rssi);
        }
        Serial.println("--------------------------\r\n");
      }
    }
#endif
    else if (cmd.equalsIgnoreCase("ping")) {
      if (arg.length() > 0) {
        ping_target = arg;
        ping_running = true;
        last_ping_time = 0; // Start immediately
        Serial.printf("Pinging %s (Infinite, press Ctrl+C to stop)...\r\n", ping_target.c_str());
      } else {
        Serial.print("Usage: ping <host>\r\n");
      }
    }
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("ifconfig") || cmd.equalsIgnoreCase("ip")) {
      Serial.print("\r\n--- Interface Configuration ---\r\n");
      Serial.println("[ETH] Ethernet Interface:");
      Serial.printf("  Status: %s\r\n", eth_connected ? "UP" : "DOWN");
      if (eth_connected) {
        Serial.printf("  MAC   : %s\r\n", ETH.macAddress().c_str());
        Serial.printf("  IPv4  : %s\r\n", ETH.localIP().toString().c_str());
        Serial.printf("  Subnet: %s\r\n", ETH.subnetMask().toString().c_str());
        Serial.printf("  GW    : %s\r\n", ETH.gatewayIP().toString().c_str());
        Serial.printf("  DNS   : %s\r\n", ETH.dnsIP().toString().c_str());
      }
      
      Serial.println("\r\n[AP] WiFi Access Point:");
      Serial.printf("  Status: %s\r\n", ap_started ? "UP" : "DOWN");
      if (ap_started) {
        Serial.printf("  MAC   : %s\r\n", WiFi.softAPmacAddress().c_str());
        Serial.printf("  IPv4  : %s\r\n", WiFi.softAPIP().toString().c_str());
      }
      Serial.print("-------------------------------\r\n");
    }
#endif
    else if (cmd.equalsIgnoreCase("arp")) {
      Serial.print("\r\n--- AP ARP / Station List ---\r\n");
      wifi_sta_list_t stationList;
      esp_wifi_ap_get_sta_list(&stationList);
      if (stationList.num == 0) {
        Serial.println("  No clients connected.");
      } else {
        for (int i = 0; i < stationList.num; i++) {
          char macStr[18];
          snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                   stationList.sta[i].mac[0], stationList.sta[i].mac[1], stationList.sta[i].mac[2],
                   stationList.sta[i].mac[3], stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
          Serial.printf("  [%d] MAC: %s, RSSI: %d\r\n", i+1, macStr, stationList.sta[i].rssi);
        }
      }
      Serial.print("-----------------------------\r\n");
    }
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("dhcp")) {
      Serial.print("\r\n--- AP DHCP Leases ---\r\n");
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
#else
      esp_netif_t* ap_netif = WiFi.AP.netif();
#endif
      wifi_sta_list_t stationList;
      esp_wifi_ap_get_sta_list(&stationList);

      if (ap_netif && stationList.num > 0) {
        esp_netif_pair_mac_ip_t mac_ip_pair[10]; 
        int num_clients = stationList.num > 10 ? 10 : stationList.num;
        
        // Prepare MAC addresses as input
        for (int i = 0; i < num_clients; i++) {
            memcpy(mac_ip_pair[i].mac, stationList.sta[i].mac, 6);
            mac_ip_pair[i].ip.addr = 0; // Clear IP initially
        }

        // ESP_OK (0) means success
        if (esp_netif_dhcps_get_clients_by_mac(ap_netif, num_clients, mac_ip_pair) == ESP_OK) {
          int count = 0;
          for (int i = 0; i < num_clients; i++) {
            if (mac_ip_pair[i].ip.addr != 0) {
              char macStr[18];
              snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                       mac_ip_pair[i].mac[0], mac_ip_pair[i].mac[1], mac_ip_pair[i].mac[2],
                       mac_ip_pair[i].mac[3], mac_ip_pair[i].mac[4], mac_ip_pair[i].mac[5]);
                       
              char ipStr[16];
              ip4addr_ntoa_r((const ip4_addr_t*)&mac_ip_pair[i].ip, ipStr, sizeof(ipStr));

              Serial.printf("  [%d] MAC: %s  ->  IP: %s\r\n", count+1, macStr, ipStr);
              count++;
            }
          }
          if (count == 0) {
             Serial.println("  No active DHCP leases with IP assigned.");
          }
        } else {
          Serial.println("  Failed to query DHCP server.");
        }
      } else {
        Serial.println("  No active DHCP leases (or AP down).");
      }
      Serial.print("----------------------\r\n");
    }
#endif
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("iperf")) {
      if (arg.equalsIgnoreCase("start")) {
        if (lwiperf_session != NULL) {
           Serial.println("iPerf server is already running on port 5001.");
        } else {
           lwiperf_session = lwiperf_start_tcp_server_default(lwiperf_report, NULL);
           if (lwiperf_session != NULL) {
               Serial.println("iPerf TCP server started (Port 5001). Connect with: iperf -c <IP> -i 1 -t 10");
           } else {
               Serial.println("Failed to start iPerf TCP server.");
           }
        }
      } else if (arg.equalsIgnoreCase("stop")) {
        if (lwiperf_session != NULL) {
           lwiperf_abort(lwiperf_session);
           lwiperf_session = NULL;
           Serial.println("iPerf server stopped.");
        } else {
           Serial.println("iPerf server is not running.");
        }
      } else {
        Serial.println("Usage: iperf <start|stop>");
      }
    }
#endif
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("toe_iperf")) {
      if (arg.startsWith("start")) {
        if(toe_iperf_is_running()) {
            Serial.println("[TOE-iPerf] Already running. Please stop first.");
            return;
        }
        
        // Cache lwIP settings for TOE hardware to use before tearing down ETH
        toe_iperf_cfg_t cfg;
        cfg.port = 5003; // Default port
        cfg.is_server = true;
        cfg.local_ip = ETH.localIP();
        cfg.gateway = ETH.gatewayIP();
        cfg.subnet = ETH.subnetMask();
        esp_read_mac(cfg.mac_addr, ESP_MAC_ETH);
        
        cfg.is_udp = false;
        if (arg.indexOf("-u") != -1) {
             cfg.is_udp = true;
             arg.replace("-u", "");
             arg.trim();
        }
        
        Serial.printf("[TOE-iPerf] Shutting down ESP_ETH (lwIP) for HW %s test...\n", cfg.is_udp ? "UDP" : "TCP");
        ETH.end(); // Stop conflicting driver and release SPI bus
        
        int dash_idx = arg.indexOf('-');
        if (dash_idx != -1 && arg.length() > dash_idx + 1) {
            char mode = arg.charAt(dash_idx + 1);
            if (mode == 's') {
                cfg.is_server = true;
                if(toe_iperf_start(&cfg)) {
                    Serial.println("[TOE-iPerf] Hardware Server Test Started.");
                } else {
                    Serial.println("[TOE-iPerf] Already running.");
                }
            } else if (mode == 'c') {
                cfg.is_server = false;
                String ip_str = arg.substring(dash_idx + 3);
                ip_str.trim();
                cfg.target_ip = ip_str.c_str(); // Memory safety for arg lifetime? 
                if(toe_iperf_start(&cfg)) {
                    Serial.printf("[TOE-iPerf] Hardware Client Test Started to %s.\r\n", cfg.target_ip);
                } else {
                    Serial.println("[TOE-iPerf] Already running.");
                }
            } else {
             }
        } else {
             // Default to server if no args
             if(toe_iperf_start(&cfg)) {
                 Serial.println("[TOE-iPerf] Hardware Server Test Started (Default).");
             } else {
                 Serial.println("[TOE-iPerf] Already running.");
                 // Restart ETH if failed
                 ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN, SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40);
             }
        }
      } else if (arg.equalsIgnoreCase("stop")) {
        if (!toe_iperf_is_running()) {
            Serial.println("[TOE-iPerf] Hardware Test is not running.");
            return;
        }
        toe_iperf_stop();
        Serial.println("[TOE-iPerf] Hardware Test Stopped. Restoring ESP_ETH...");
        // Restart the ESP-IDF driver to restore NAT operation
        ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN, SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40);
        
#if USE_STATIC_IP
        ETH.config(local_ip, gateway, subnet, dns1, dns2);
#endif
        Serial.println("[TOE-iPerf] Internet sharing features resumed.");
      } else {
        Serial.println("Usage: toe_iperf <start|stop> [-s | -c ip_address]");
      }
    }
#endif
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("bridge")) {
      if (arg.startsWith("start")) {
        if(socket_bridge_is_running() || toe_iperf_is_running()) {
            Serial.println("Another TOE task is already running.");
            return;
        }
        
        int listen_port = 5003; 
        String target_ip = "192.168.0.100";
        int target_port = 5003;
        
        // Find first space after "start"
        int space1 = arg.indexOf(' ');
        if (space1 != -1) {
            int space2 = arg.indexOf(' ', space1 + 1);
            if (space2 != -1) {
                listen_port = arg.substring(space1 + 1, space2).toInt();
                int space3 = arg.indexOf(' ', space2 + 1);
                if (space3 != -1) {
                    target_ip = arg.substring(space2 + 1, space3);
                    target_port = arg.substring(space3 + 1).toInt();
                } else {
                    target_ip = arg.substring(space2 + 1);
                }
            }
        }

        socket_bridge_cfg_t cfg;
        cfg.listen_port = listen_port;
        
        IPAddress target_ip_addr;
        if (target_ip_addr.fromString(target_ip)) {
            for(int i=0; i<4; i++) cfg.target_ip[i] = target_ip_addr[i];
        } else {
            // Default if parsing fails
            cfg.target_ip[0] = 192; cfg.target_ip[1] = 168; cfg.target_ip[2] = 0; cfg.target_ip[3] = 100;
        }
        
        cfg.target_port = target_port;
        cfg.local_ip = ETH.localIP();
        cfg.gateway = ETH.gatewayIP();
        cfg.subnet = ETH.subnetMask();
        esp_read_mac(cfg.mac_addr, ESP_MAC_ETH);
        cfg.is_udp = false;

        Serial.printf("[Bridge] Connecting WiFi:%d <-> Ethernet:%s:%d\n", listen_port, cfg.target_ip, cfg.target_port);
        ETH.end(); 

        if(socket_bridge_start(&cfg)) {
            Serial.println("[Bridge] Started.");
        } else {
            Serial.println("[Bridge] Failed.");
            ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN, SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40);
        }
      } else if (arg.equalsIgnoreCase("stop")) {
        socket_bridge_stop();
        Serial.println("[Bridge] Stopped.");
        delay(500);
        ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN, SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40);
      } else {
        Serial.println("Usage: bridge start <listen_port> <target_ip> <target_port>");
        Serial.println("       bridge stop");
      }
    }
#endif
#ifdef ENABLE_ETHERNET
    else if (cmd.equalsIgnoreCase("udp_iperf")) {
      if (arg.equalsIgnoreCase("start")) {
        if (udp_iperf_running) {
           Serial.println("UDP iPerf server is already running on port 5002.");
        } else {
           if (udp_iperf_server.listen(5002)) {
               udp_iperf_server.onPacket(onUDPPacket);
               udp_iperf_running = true;
               udp_iperf_bytes = 0;
               udp_iperf_last_print = millis();
               Serial.println("UDP Test server started on Port 5002.");
               Serial.println("Connect with: iperf -c <IP> -u -p 5002 -b 20M -i 1 -t 10");
           } else {
               Serial.println("Failed to start UDP Test server.");
           }
        }
      } else if (arg.equalsIgnoreCase("stop")) {
        if (udp_iperf_running) {
           udp_iperf_server.close();
           udp_iperf_running = false;
           Serial.println("UDP Test server stopped.");
        } else {
           Serial.println("UDP Test server is not running.");
        }
      } else {
        Serial.println("Usage: udp_iperf <start|stop>");
      }
    }
#endif
    else {
      Serial.printf("Unknown command: %s. Type 'help' for list.\r\n", cmd.c_str());
    }

    inputString = "";
    stringComplete = false;
    Serial.print("> "); // Prompt for next command
  }
}

/**
 * @brief WiFi and Ethernet event handler
 *
 * Handles connection events for both Ethernet and WiFi AP interfaces.
 * When Ethernet gets an IP, starts the WiFi AP and enables NAPT for packet
 * forwarding.
 */
void NetworkEvent(arduino_event_id_t event) {
  switch (event) {
#ifdef ENABLE_ETHERNET
  case ARDUINO_EVENT_ETH_START:
    logMsg(LOG_INFO, "ETH Started");
    ETH.setHostname("eth2ap-bridge");
    installHooks(); // Try installing hooks on start
    break;

  case ARDUINO_EVENT_ETH_CONNECTED:
    logMsg(LOG_INFO, "ETH Connected");
    installHooks(); // Re-verify hooks on connection
    break;

  case ARDUINO_EVENT_ETH_GOT_IP:
    logMsg(LOG_INFO, "ETH MAC: %s, IPv4: %s, %s, %d Mbps", 
           ETH.macAddress().c_str(), ETH.localIP().toString().c_str(),
           ETH.fullDuplex() ? "FULL_DUPLEX" : "HALF_DUPLEX", ETH.linkSpeed());
    eth_connected = true;

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    if (WiFi.AP.enableNAPT(true)) {
      logMsg(LOG_INFO, "NAPT enabled - Bridge is active");
    } else {
      logMsg(LOG_ERROR, "NAPT enable failed!");
    }
#endif
    break;

  case ARDUINO_EVENT_ETH_DISCONNECTED:
    logMsg(LOG_WARN, "ETH Disconnected");
    eth_connected = false;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.AP.enableNAPT(false);
#endif
    break;

  case ARDUINO_EVENT_ETH_STOP:
    logMsg(LOG_INFO, "ETH Stopped");
    eth_connected = false;
    break;
#endif

  case ARDUINO_EVENT_WIFI_AP_START:
    logMsg(LOG_INFO, "WiFi AP Started (SSID: %s, IP: %s)", ap_ssid_custom.c_str(), WiFi.softAPIP().toString().c_str());
    ap_started = true;
    break;

  case ARDUINO_EVENT_WIFI_AP_STOP:
    logMsg(LOG_INFO, "WiFi AP Stopped");
    ap_started = false;
    break;

  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    logMsg(LOG_INFO, "WiFi AP: Station connected");
    break;

  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    logMsg(LOG_INFO, "WiFi AP: Station disconnected");
    break;

  default:
    break;
  }
  // Update LCD on any major network event
  updateLCD();
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to stabilize

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("\n\n[System] Reset Reason: %d\n", reason);

  Serial.println("===== eth2ap Bridge Example =====");
  Serial.println("Ethernet to WiFi AP Bridge using NAPT");
  Serial.println("=====================================\n");
  Serial.print("> "); // Initial prompt

  // Step 0: Load saved settings from NVS
  preferences.begin("wifi-config", false); // Open in read-write mode to save if not exists
  if (!preferences.isKey("ssid")) {
    preferences.putString("ssid", ap_ssid_custom);
    preferences.putString("password", ap_pw_custom);
  } else {
    ap_ssid_custom = preferences.getString("ssid", AP_SSID_BASE);
    ap_pw_custom = preferences.getString("password", AP_PASSWORD_BASE);
  }
  preferences.end();
  logMsg(LOG_INFO, "Loaded settings: SSID='%s'", ap_ssid_custom.c_str());

  // Step 1: Start WiFi AP immediately (Always-On Priority)
  logMsg(LOG_INFO, "Step 1: Starting WiFi AP (softAP)... %s", 
    WiFi.softAP(ap_ssid_custom.c_str(), ap_pw_custom.c_str(), AP_CHANNEL, 0, AP_MAX_CONN) ? "Success" : "FAILED");

  esp_err_t err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);
  logMsg(LOG_INFO, "Step 2: Setting WiFi Bandwidth to HT40... %s", (err == ESP_OK) ? "Success" : "FAILED");

  logMsg(LOG_INFO, "Step 3: Checking AP Status... SSID: %s, IP: %s", 
         ap_ssid_custom.c_str(), WiFi.softAPIP().toString().c_str());
  ap_started = true;

  // Step 4: Setting up WiFi events
  Serial.print("Step 4: Registering Network Events... ");
  WiFi.onEvent(NetworkEvent);
  Serial.println("Done.");

  Serial.println("\n[Note] WiFi AP is running.");
  Serial.println("       Internet access will be available after Ethernet connection.\n");

  // Step 5: Initialize LCD (Dynamically detected)
#ifdef ENABLE_LCD
  lcd_detected = detectLCD();
  if (lcd_detected) {
    Serial.print("Step 5: Initializing LCD Pins... ");
    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH); // Turn on backlight
    Serial.println("Done.");

    /* Initialize LCD DMA */
    tft.init();

    // ST7789 Frame Rate Control (FRCTRL2: 0xC6) 강제 조작
    // 다시 50Hz 수준으로 원복 (스캐너 속도를 극단적으로 낮추는 것은 해결책이 아니었음)
    tft.writecommand(0xC6);
    tft.writedata(0x15);
    Serial.println("ST7789 FRCTRL2 Overridden to 0x15 (~50Hz)");

    // ST7789 TE(Tearing Effect) 핀 출력 활성화 (0x35)
    // 0x00: V-Blanking 펄스만 출력 (가장 일반적인 V-Sync 동기화용 사각파)
    tft.writecommand(0x35);
    tft.writedata(0x00);
    Serial.println("ST7789 TE(Tearing Effect) Pin Output Enabled!");

    tft.initDMA(); // Start GDMA for SPI
    
    // Initialize TE Hardware Interrupt
    te_semaphore = xSemaphoreCreateBinary();
    pinMode(TE_PIN, INPUT_PULLDOWN); // 아무것도 안 꽂았을 때 노이즈를 안테나처럼 빨아들이는 것(플로팅) 방지
    attachInterrupt(TE_PIN, te_isr_handler, RISING);
    Serial.println("Hardware TE Interrupt Attached to GPIO 21");

    /* Initialize LVGL and Allocate DMA Buffers in Internal SRAM */
    lv_init();
    
    // Allocate 2 x 80KB buffers in Internal SRAM with DMA capability
    // 64 lines exactly matches the 32KB L1 Data Cache on ESP32-S3 (240x64x2 = 30.7KB)
    size_t lines = 64; 
    size_t buf_size = LCD_WIDTH * lines * sizeof(lv_color_t);
    
    buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    buf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (buf1 == NULL || buf2 == NULL) {
        Serial.println("Failed to allocate DMA buffers! Falling back to SRAM Single...");
        if(buf1) free(buf1);
        static lv_color_t sram_single[LCD_WIDTH * 20];
        lv_disp_draw_buf_init(&draw_buf, sram_single, NULL, LCD_WIDTH * 20);
    } else {
        Serial.printf("DMA Double Buffers allocated: 2 x %u bytes\n", buf_size);
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_WIDTH * lines);
    }

    /* Initialize the display driver for LVGL */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_HEIGHT; // Landscape
    disp_drv.ver_res = LCD_WIDTH;  // Landscape
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.monitor_cb = my_monitor_cb; // 추가: 렌더링 성능 모니터링 연결
    disp_drv.draw_buf = &draw_buf; // CRITICAL: This was missing!
    lv_disp_t * disp_obj = lv_disp_drv_register(&disp_drv);
    
    // Set refresh period to 16ms for 60 FPS
    lv_timer_t * refr_timer = _lv_disp_get_refr_timer(disp_obj);
    if (refr_timer) lv_timer_set_period(refr_timer, 16);

    lcd_initialized = true; 
    Serial.println("Done.");

    Serial.print("Step 7: LVGL UI creation... ");
    tft.setRotation(1); // Landscape
    initLVGLUI();
    updateLCD();
    Serial.println("Done.");
  } else {
    Serial.println("Step 5-7: LCD Hardware not found. Skipping.");
  }
#else
  Serial.println("Step 5-7: LCD Feature Disabled.");
#endif

  // Step 8: Initialize Ethernet
#ifdef ENABLE_ETHERNET
  logMsg(LOG_INFO, "Step 8: Initializing Ethernet...");

#if CONFIG_IDF_TARGET_ESP32
  ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN, ETH_CLK_MODE);
#else
  ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
            SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40);
#endif

#if USE_STATIC_IP
  ETH.config(local_ip, gateway, subnet, dns1, dns2);
#endif

  logMsg(LOG_INFO, "Ethernet initialization command sent");
  
  // Final Step: Install Hooks
  installHooks(); 
#else
  logMsg(LOG_WARN, "Step 8: Ethernet Feature Disabled (#define ENABLE_ETHERNET).");
#endif

  Serial.println("Waiting for Ethernet connection to enable internet sharing...");
  logMsg(LOG_INFO, "System ready. Type 'help' for commands.");
}

void loop() {
  // Print status every 5 seconds (Changed from 30 for debugging)
  static unsigned long lastStatusPrint = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastStatusPrint >= 5000) {
    lastStatusPrint = currentMillis;

    if (monitor_enabled) {
      logMsg(LOG_INFO, "----- Status -----");
#ifdef ENABLE_ETHERNET
      logMsg(LOG_INFO, "Ethernet: %s", eth_connected ? "Connected" : "Disconnected");
      if (eth_connected) {
        logMsg(LOG_INFO, "  IP: %s", ETH.localIP().toString().c_str());
      }
#endif
      logMsg(LOG_INFO, "WiFi AP : %s", ap_started ? "Running" : "Stopped");
      if (ap_started) {
        logMsg(LOG_INFO, "  Clients: %d", WiFi.softAPgetStationNum());
      }
      logMsg(LOG_INFO, "------------------\r\n");
    }

#ifdef ENABLE_ETHERNET
    if (monitor_traffic) {
        unsigned long now = millis();
        if (now - last_speed_check >= 2000) {
          last_speed_check = now;
          Serial.printf("[Monitor] ETH RX: %s, TX: %s | AP RX: %s, TX: %s\r\n", 
                        formatBytes(eth_stats.rx_bytes).c_str(), formatBytes(eth_stats.tx_bytes).c_str(), 
                        formatBytes(ap_stats.rx_bytes).c_str(), formatBytes(ap_stats.tx_bytes).c_str());
        }
    }
#endif

    // Update LCD periodically
    updateLCD();
  }

  // Handle Serial Shell
  handleShell();

  // Handle UDP iPerf Reporting
#ifdef ENABLE_ETHERNET
  if (udp_iperf_running) {
    uint32_t now = millis();
    if (now - udp_iperf_last_print >= 1000) {
      // Calculate Mbps and MB/s
      float mbps = (udp_iperf_bytes * 8.0) / 1000000.0;
      float mb_s = udp_iperf_bytes / (1024.0 * 1024.0);
      
      Serial.printf("\r\n[UDP iPerf] %.2f MB/s  |  %.2f Mbps  (Received: %u Bytes)\r\n> ", mb_s, mbps, (unsigned int)udp_iperf_bytes);
      refreshLine();
      udp_iperf_bytes = 0; // Reset for next second
      udp_iperf_last_print = now;
    }
  }
#endif

  // Infinite Ping
  if (ping_running) {
    if (currentMillis - last_ping_time >= 1000) {
      last_ping_time = currentMillis;
      bool success = Ping.ping(ping_target.c_str(), 1);
      if (success) {
        Serial.printf("Reply from %s: time=%.2fms\r\n", ping_target.c_str(), Ping.averageTime());
      } else {
        Serial.printf("Request timeout for %s\r\n", ping_target.c_str());
      }
    }
  }

  // Update LVGL tick and timer handler
  lv_timer_handler();

  // Update FPS Label every 1 second independently
  static uint32_t last_fps_millis = 0;
  if (millis() - last_fps_millis >= 1000) {
    if (lcd_initialized && ui_label_fps) {
        fps_val = lv_refr_get_fps_avg(); // Use LVGL internal average FPS
        char buf_fps[32]; // 크기 늘림
        snprintf(buf_fps, sizeof(buf_fps), "FPS:%u  %ums", fps_val, last_render_time_ms);
        lv_label_set_text(ui_label_fps, buf_fps);
    }
    last_fps_millis = millis();
  }

  // Minimal delay for system stability while maximizing loop throughput
  yield(); 
}
