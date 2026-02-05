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
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n\n===== eth2ap Bridge Example =====");
  Serial.println("Ethernet to WiFi AP Bridge using NAPT");
  Serial.println("=====================================\n");

  // Register event handler for both WiFi and Ethernet events
  WiFi.onEvent(NetworkEvent);

  // Start WiFi AP immediately (always visible)
  Serial.println("Starting WiFi AP...");
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
  Serial.println("WiFi AP Started!");
  Serial.print("  SSID: ");
  Serial.println(AP_SSID);
  Serial.print("  Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("  IP Address: ");
  Serial.println(WiFi.softAPIP());
  ap_started = true;

  Serial.println("\n[Note] WiFi AP is running.");
  Serial.println(
      "       Internet access will be available after Ethernet connection.\n");

  // Initialize Ethernet for T-ETH-Lite-ESP32S3 (W5500 via SPI)
  Serial.println("Initializing Ethernet...");

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
  if (!ETH.begin(ETH_PHY_W5500, ETH_ADDR, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                 SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
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
  }

  delay(100);
}
