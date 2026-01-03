#include <Arduino.h>
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>

// Master Coder's Pin Mapping
#define TX_PIN 45
#define RX_PIN 21
#define RGB_LED_PIN 48  // Onboard WS2812 RGB LED
#define NUM_PIXELS 1

Adafruit_NeoPixel statusLED(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    // 0. Initialize Status LED
    statusLED.begin();
    statusLED.setBrightness(50); // 0-255
    statusLED.setPixelColor(0, statusLED.Color(255, 0, 0)); // Red = Booting/Waiting
    statusLED.show();

    // 1. Initialize Serial Interfaces
    Serial.begin(115200); // USB Monitor
    while(!Serial && millis() < 3000) { delay(10); } // Wait up to 3s for USB Serial
    delay(500); // Additional stabilization
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // RX on 21, TX on 45

    Serial.println("\n\n========================================");
    Serial.println("COMMANDER: Booting...");
    Serial.printf("COMMANDER: RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);
    Serial.println("========================================");

    // 2. Handshake Loop: Wait for Soldier board to be ready
    Serial.println("COMMANDER: Waiting for Soldier's SOLDIER_READY response...");

    unsigned long startTime = millis();
    bool handshakeComplete = false;

    while (millis() - startTime < 30000) { // 30 second timeout
        Serial.println("COMMANDER: Sending READY_QUERY...");
        Serial1.println("READY_QUERY");
        Serial1.flush(); // Make sure data is sent

        delay(500); // Give time for response

        if (Serial1.available()) {
            String response = Serial1.readStringUntil('\n');
            response.trim(); // Remove whitespace
            Serial.printf("COMMANDER: Received response: '%s'\n", response.c_str());

            if (response.indexOf("SOLDIER_READY") != -1) {
                Serial.println("COMMANDER: Soldier Linked. System Green.");
                statusLED.setPixelColor(0, statusLED.Color(0, 255, 0)); // Green = Connected
                statusLED.show();
                handshakeComplete = true;
                delay(1000); // Hold green for visibility
                break;
            }
        }
        delay(10); // Small delay to prevent busy waiting
    }

    if (!handshakeComplete) {
        Serial.println("COMMANDER: Handshake timeout - continuing anyway");
        statusLED.setPixelColor(0, statusLED.Color(255, 255, 0)); // Yellow = Warning
        statusLED.show();
        delay(1000);
    }

    // 3. Radio Warfare Setup
    Serial.println("COMMANDER: Initializing Wi-Fi Radio...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM); // Protect your 16MB Flash
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    // Set to 20dBm (Max power for the U.FL Antenna)
    esp_wifi_set_max_tx_power(80);

    statusLED.setPixelColor(0, statusLED.Color(0, 0, 255)); // Blue = Armed
    statusLED.show();

    Serial.println("COMMANDER: Wi-Fi Radio Armed at Max Power (20dBm).");
    Serial.println("COMMANDER: Ready for jamming sweep.");
}

void loop() {
    // Sweep through Wi-Fi Channels 1 to 13
    for (int ch = 1; ch <= 13; ch++) {
        Serial.printf("COMMANDER: >>> Jamming Wi-Fi Ch %d | Triggering SOLDIER BLE...\n", ch);

        // Purple flash = Jamming active
        statusLED.setPixelColor(0, statusLED.Color(255, 0, 255));
        statusLED.show();

        // Lock Radio to Channel
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

        // Signal Soldier to sweep its BLE range simultaneously
        Serial1.println("SWEEP_START");
        Serial1.flush();

        delay(100); // Dwell time on each frequency - matches Soldier's 80ms + overhead

        // Back to blue = Armed and ready
        statusLED.setPixelColor(0, statusLED.Color(0, 0, 255));
        statusLED.show();
    }
}
