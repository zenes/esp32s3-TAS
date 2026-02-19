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
#include <esp_wifi.h> // For esp_wifi_set_bandwidth
#include <esp_system.h> // For esp_reset_reason
#include <rom/rtc.h>    // For rtc_get_reset_reason
#include <TFT_eSPI.h> // LCD Library
#include <lvgl.h>    // LVGL Graphic Library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
#define ENABLE_LCD // Enable logic, but will be checked dynamically
static bool lcd_initialized = false;
static bool lcd_detected = false;

/* LVGL Buffer */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LCD_WIDTH * 20];

/* UI Objects */
static lv_obj_t *ui_label_eth;
static lv_obj_t *ui_label_ap;
static lv_obj_t *ui_label_clients;

/* Display flushing callback */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// WiFi AP Configuration
#define AP_SSID "eth2ap"
#define AP_PASSWORD "12345678"
#define AP_CHANNEL 1
#define AP_MAX_CONN 4

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
  // Use a local SPI instance to probe without affecting global state
  SPIClass spi_probe(FSPI); // FSPI is SPI2_HOST on S3
  spi_probe.begin(LCD_SCLK_PIN, LCD_MISO_PIN, LCD_MOSI_PIN, LCD_CS_PIN);
  
  pinMode(LCD_CS_PIN, OUTPUT);
  digitalWrite(LCD_CS_PIN, LOW);
  
  // Try to Read Display ID (0x04)
  // ST7789/ILI9341 ID: dummy, then ID1, ID2, ID3
  spi_probe.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  spi_probe.transfer(0x04);
  spi_probe.transfer(0x00); // dummy
  uint8_t id1 = spi_probe.transfer(0x00);
  uint8_t id2 = spi_probe.transfer(0x00);
  uint8_t id3 = spi_probe.transfer(0x00);
  spi_probe.endTransaction();
  
  digitalWrite(LCD_CS_PIN, HIGH);
  spi_probe.end(); 

  Serial.printf("ID: %02X %02X %02X -> ", id1, id2, id3);
  
  // Floating PINs usually return 0xFF or 0x00
  if ((id1 == 0xFF || id1 == 0x00) && (id2 == 0xFF || id2 == 0x00)) {
    Serial.println("Not Found.");
    return false;
  }
  Serial.println("Found!");
  return true;
}

/**
 * @brief Initialize LVGL UI widgets
 */
void initLVGLUI() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  /* Title */
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "ETH2AP Bridge");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_YELLOW), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

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
    snprintf(buf_ap, sizeof(buf_ap), "AP: #00FF00 Running#\n SSID: %s", AP_SSID);
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
 * @brief WiFi and Ethernet event handler
 *
 * Handles connection events for both Ethernet and WiFi AP interfaces.
 * When Ethernet gets an IP, starts the WiFi AP and enables NAPT for packet
 * forwarding.
 */
void NetworkEvent(arduino_event_id_t event) {
  switch (event) {
  // Ethernet Events
  case ARDUINO_EVENT_ETH_START:
    Serial.println("ETH Started");
    ETH.setHostname("eth2ap-bridge");
#if USE_STATIC_IP
    if (ETH.config(local_ip, gateway, subnet, dns1, dns2)) {
      Serial.println("Static IP configuration applied.");
    } else {
      Serial.println("Static IP configuration failed!");
    }
#endif
    break;

  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("ETH Connected");
    break;

  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.print("ETH MAC: ");
    Serial.print(ETH.macAddress());
    Serial.print(", IPv4: ");
    Serial.print(ETH.localIP());
    if (ETH.fullDuplex()) {
      Serial.print(", FULL_DUPLEX");
    }
    Serial.print(", ");
    Serial.print(ETH.linkSpeed());
    Serial.println("Mbps");

    eth_connected = true;

    // Start WiFi AP if not already started (it should be started in setup)
    if (!ap_started) {
      Serial.println("Starting WiFi AP...");

      // Set WiFi Bandwidth to HT40 for higher throughput
      esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);

      WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
      ap_started = true;
    }

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Enable NAPT (Network Address Port Translation) for packet forwarding
    // This allows devices connected to the WiFi AP to access the network
    // through Ethernet (Internal or External network)
    if (WiFi.AP.enableNAPT(true)) {
      Serial.println("NAPT enabled - Bridge is active");
    } else {
      Serial.println("NAPT enable failed!");
    }
#else
    Serial.println("WARNING: NAPT is not supported in Arduino core < 3.0.0");
    Serial.println("Please upgrade to Arduino-ESP32 core 3.0.0 or higher");
    Serial.println("for full bridge functionality.");
    Serial.println("WiFi AP started but packet forwarding is disabled.");
#endif
    break;

  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("ETH Disconnected");
    eth_connected = false;

    // Disable NAPT when Ethernet is disconnected (WiFi AP stays running)
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.AP.enableNAPT(false);
    Serial.println("NAPT disabled - Internet sharing stopped.");
    Serial.println("WiFi AP is still running but without internet access.");
#endif
    break;

  case ARDUINO_EVENT_ETH_STOP:
    Serial.println("ETH Stopped");
    eth_connected = false;
    break;

  // WiFi AP Events
  case ARDUINO_EVENT_WIFI_AP_START:
    Serial.println("WiFi AP Started");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    ap_started = true;
    break;

  case ARDUINO_EVENT_WIFI_AP_STOP:
    Serial.println("WiFi AP Stopped");
    ap_started = false;
    break;

  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    Serial.println("WiFi AP: Station connected");
    break;

  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    Serial.println("WiFi AP: Station disconnected");
    break;

  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    Serial.print("WiFi AP: Station IP assigned: ");
    Serial.println(IPAddress(WiFi.softAPIP()));
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

  // Step 1: Start WiFi AP immediately (Always-On Priority)
  Serial.print("Step 1: Starting WiFi AP (softAP)... ");
  if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN)) {
    Serial.println("Success.");
  } else {
    Serial.println("FAILED.");
  }

  Serial.print("Step 2: Setting WiFi Bandwidth to HT40... ");
  esp_err_t err = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);
  if (err == ESP_OK) {
    Serial.println("Success.");
  } else {
    Serial.printf("FAILED (err: %d).\n", err);
  }

  Serial.print("Step 3: Checking AP Status... ");
  ap_started = true;
  Serial.print("SSID: ");
  Serial.print(AP_SSID);
  Serial.print(", IP: ");
  Serial.println(WiFi.softAPIP());

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

    Serial.print("Step 6: tft.init() and LVGL Setup... ");
    tft.init();
    
    /* Initialize LVGL */
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_WIDTH * 20);

    /* Initialize the display driver for LVGL */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_HEIGHT; // Landscape
    disp_drv.ver_res = LCD_WIDTH;  // Landscape
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

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
  Serial.println("Step 8: Initializing Ethernet...");

#if CONFIG_IDF_TARGET_ESP32
  // For ESP32 with internal Ethernet (LAN8720, RTL8201, etc.)
  if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN,
                 ETH_CLK_MODE)) {
    Serial.println("ETH start Failed!");
    Serial.println(
        "WiFi AP is running, but internet sharing is not available.");
    return;
  }
#else
  // For ESP32-S3 with W5500 SPI Ethernet
  // Increase SPI frequency to 80MHz (Max supported) for maximum throughput
  if (!ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                 SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, 40)) {
    Serial.println("ETH start Failed!");
    Serial.println(
        "WiFi AP is running, but internet sharing is not available.");
    return;
  }
#endif

  Serial.println("Ethernet initialized successfully");
  Serial.println(
      "Waiting for Ethernet connection to enable internet sharing...");
}

#include <ESP32Ping.h>

void loop() {
  // Print status every 5 seconds (Changed from 30 for debugging)
  static unsigned long lastStatusPrint = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastStatusPrint >= 5000) {
    lastStatusPrint = currentMillis;

    Serial.println("\n----- Status -----");
    Serial.print("Ethernet: ");
    Serial.println(eth_connected ? "Connected" : "Disconnected");

    if (eth_connected) {
      Serial.print("  IP: ");
      Serial.println(ETH.localIP());

      // Ping Test to Gateway (Device A)
      Serial.print("  Ping Gateway (");
      Serial.print(gateway);
      Serial.print("): ");
      if (Ping.ping(gateway)) {
        Serial.println("Success!");
      } else {
        Serial.println("FAILED!");
      }
    }

    Serial.print("WiFi AP: ");
    Serial.println(ap_started ? "Running" : "Stopped");

    if (ap_started) {
      Serial.print("  Connected Stations: ");
      Serial.println(WiFi.softAPgetStationNum());
    }
    Serial.println("------------------\n");

    // Update LCD periodically
    updateLCD();
  }

  // Update LVGL timer handler
  lv_timer_handler();

  delay(5);
}
