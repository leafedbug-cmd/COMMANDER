#include <Arduino.h>
#include "esp_wifi.h"

// Master Coder's Pin Mapping
#define TX_PIN 43 
#define RX_PIN 44

void setup() {
    // 1. Initialize Serial Interfaces
    Serial.begin(115200); // USB Monitor
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // Inter-chip link to SparkleIoT

    // 2. Handshake Loop: Wait for SparkleIoT to be ready
    Serial.println("COMMANDER: Initializing handshake...");
    while (true) {
        Serial1.println("READY_QUERY");
        if (Serial1.available()) {
            String response = Serial1.readStringUntil('\n');
            if (response.indexOf("SOLDIER_READY") != -1) {
                Serial.println("COMMANDER: SparkleIoT Linked. System Green.");
                break;
            }
        }
        delay(500); // Don't flood the UART during boot
    }

    // 3. Radio Warfare Setup
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM); // Protect your 16MB Flash
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    
    // Set to 20dBm (Max power for the U.FL Antenna)
    esp_wifi_set_max_tx_power(80); 
}

void loop() {
    // Sweep through Wi-Fi Channels 1 to 13
    for (int ch = 1; ch <= 13; ch++) {
        Serial.printf("COMMANDER: Jamming Wi-Fi Ch %d | Triggering SparkleIoT BLE...\n", ch);
        
        // Lock Radio to Channel
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        
        // Signal SparkleIoT to sweep its BLE range simultaneously
        Serial1.println("SWEEP_START");
        
        delay(100); // Dwell time on each frequency
    }
}
