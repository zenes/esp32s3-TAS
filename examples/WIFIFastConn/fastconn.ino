/**
 * @file      WiFiFastScan.ino
 * @author    Grok (adapted from ESP32 Arduino WiFi examples)
 * @license   MIT
 * @copyright Public Domain / Open Source Example
 * @date      2025-12-22
 * @note      ESP32 WiFi Fast Scan Example using Arduino framework.
 *            Demonstrates fast scanning of nearby WiFi networks with reduced channel dwell time.
 *            Works on most ESP32 boards (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.).
 *            Adjust board-specific settings if needed.
 */

#include <Arduino.h>
#include <WiFi.h>

// No specific pin definitions needed for WiFi scan (uses internal radio)
// If your board has custom constraints, you can add #if defined(...) here like in your example.

void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n===== START Wait =====");

    while (!Serial);  // Wait for serial port to connect (useful for native USB boards)
    Serial.println("\n\n===== START =====");

    WiFi.mode(WIFI_STA);     // Set to Station mode
    WiFi.disconnect(true);   // Disconnect from any previous connection and clear data
    delay(100);

    Serial.println();
    Serial.println("ESP32 WiFi Fast Scanner Ready!");
    Serial.println("Scanning will start in loop every 5 seconds.");
    Serial.println("---------------------------------------------------");
}

void loop()
{
    Serial.println("Starting Fast WiFi Scan...");

    // WiFi.scanNetworks(async, show_hidden, passive, max_ms_per_chan, channel_start, channel_end)
    // - async: false → synchronous (wait for results)
    // - show_hidden: true → include hidden SSIDs
    // - passive: false → active scan (faster and more reliable)
    // - max_ms_per_chan: 120 → fast scan (default is ~300ms, lower = faster but may miss weak networks)
    int networksFound = WiFi.scanNetworks(false, true, false, 120);

    if (networksFound == 0) {
        Serial.println("No WiFi networks found.");
    } else if (networksFound == WIFI_SCAN_FAILED) {
        Serial.println("WiFi scan failed (radio busy or error).");
    } else {
        Serial.printf("%d network(s) found:\n", networksFound);
        Serial.println("No | SSID                             | RSSI    | Channel | Encryption Type          | BSSID");

        for (int i = 0; i < networksFound; ++i) {
            Serial.printf("%2d | %-32s | %5d dBm | %3d     | ", 
                          i + 1,
                          WiFi.SSID(i).c_str(),
                          WiFi.RSSI(i),
                          WiFi.channel(i));

            // Encryption type
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN:            Serial.print("Open               "); break;
                case WIFI_AUTH_WEP:             Serial.print("WEP                "); break;
                case WIFI_AUTH_WPA_PSK:         Serial.print("WPA PSK            "); break;
                case WIFI_AUTH_WPA2_PSK:        Serial.print("WPA2 PSK           "); break;
                case WIFI_AUTH_WPA_WPA2_PSK:    Serial.print("WPA/WPA2 PSK       "); break;
                case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2 Enterprise    "); break;
                case WIFI_AUTH_WPA3_PSK:        Serial.print("WPA3 PSK           "); break;
                case WIFI_AUTH_WPA2_WPA3_PSK:   Serial.print("WPA2/WPA3 PSK      "); break;
                default:                        Serial.print("Unknown            "); break;
            }

            Serial.printf(" | %s", WiFi.BSSIDstr(i).c_str());
            Serial.println();
        }
    }

    // Important: Free the scan results to avoid memory leaks
    WiFi.scanDelete();

    Serial.println("---------------------------------------------------");
    delay(5000);  // Wait 5 seconds before next scan (adjust as needed)
}