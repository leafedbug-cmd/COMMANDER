#include <Arduino.h>
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>

// Master Coder's Pin Mapping
#define TX_PIN 45 
#define RX_PIN 21
#define RGB_PIN 48
#define NUM_PIXELS 1

Adafruit_NeoPixel statusLED(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    // 0. Initialize Status LED
    statusLED.begin();
    statusLED.setPixelColor(0, statusLED.Color(255, 0, 0)); // Start RED (Searching)
    statusLED.show();
    
    // 1. Initialize Serial Interfaces
    Serial.begin(115200); // USB Monitor
    delay(1000); // Wait for USB Serial to be ready
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // Inter-chip link to SparkleIoT

    // 2. Handshake Loop: Wait for SparkleIoT to be ready
    Serial.println("COMMANDER: Initializing handshake...");
    Serial.printf("COMMANDER: TX Pin: %d, RX Pin: %d\n", TX_PIN, RX_PIN);
    
    int attemptCount = 0;
    while (true) {
        attemptCount++;
        Serial.printf("COMMANDER: Sending READY_QUERY (attempt %d)...\n", attemptCount);
        Serial1.println("READY_QUERY");
        Serial1.flush(); // Make sure data is sent
        
        delay(100); // Give time for response
        
        if (Serial1.available()) {
            String response = Serial1.readStringUntil('\n');
            Serial.printf("COMMANDER: Received response: '%s'\n", response.c_str());
            
            if (response.indexOf("SOLDIER_READY") != -1) {
                Serial.println("COMMANDER: SparkleIoT Linked. System Green.");
                statusLED.setPixelColor(0, statusLED.Color(0, 255, 0)); // Turn GREEN (Linked)
                statusLED.show();
                break;
            }
        } else {
            Serial.println("COMMANDER: No response from SparkleIoT");
        }
        
        // Skip handshake after 10 attempts for testing
        if (attemptCount >= 10) {
            Serial.println("COMMANDER: Handshake timeout - continuing anyway for testing");
            statusLED.setPixelColor(0, statusLED.Color(255, 255, 0)); // Turn YELLOW (Warning)
            statusLED.show();
            break;
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
