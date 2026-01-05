#include <Arduino.h>
#include "esp_wifi.h"
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>

// UART to HQ (ESP32-C3) - safe GPIOs
#define TX_PIN 18  // GPIO18 (TX)
#define RX_PIN 17  // GPIO17 (RX)
#define RGB_LED_PIN 48
#define NUM_PIXELS 1

Adafruit_NeoPixel statusLED(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
NimBLEAdvertising* adv = nullptr;

static void setStatus(uint8_t r, uint8_t g, uint8_t b) {
    statusLED.setPixelColor(0, statusLED.Color(r, g, b));
    statusLED.show();
}

void setup() {
    statusLED.begin();
    statusLED.setBrightness(50);
    setStatus(255, 0, 0); // Red = booting

    Serial.begin(115200);      // USB debug
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // UART to HQ
    delay(100);
    while (!Serial && millis() < 2000) { delay(10); }

    Serial.println("\n\n========================================");
    Serial.println("COMMANDER: Booting (HQ-controlled)");
    Serial.printf("UART RX=%d TX=%d\n", RX_PIN, TX_PIN);
    Serial.println("========================================");

    // Clear buffer
    while (Serial1.available()) { Serial1.read(); }

    // Handshake with HQ
    Serial.println("COMMANDER: Waiting for HQ READY_QUERY...");
    while (true) {
        if (Serial1.available()) {
            String msg = Serial1.readStringUntil('\n');
            msg.trim();
            if (msg == "READY_QUERY") {
                Serial.println("COMMANDER: HQ detected, sending COMMANDER_READY");
                Serial1.println("COMMANDER_READY");
                Serial1.flush();
                setStatus(0, 255, 0); // Green = linked to HQ
                delay(500);
                break;
            }
        }
        delay(10);
    }

    Serial.println("COMMANDER: Initializing Wi-Fi radio...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    esp_wifi_set_max_tx_power(80); // 20 dBm

    Serial.println("COMMANDER: Initializing BLE radio...");
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    adv = NimBLEDevice::getAdvertising();

    setStatus(0, 0, 255); // Blue = armed/idle
    Serial.println("COMMANDER: Ready for HQ Wi-Fi and BLE commands.");
}

void loop() {
    if (Serial1.available()) {
        String cmd = Serial1.readStringUntil('\n');
        cmd.trim();

        if (cmd.startsWith("JAM_WIFI")) {
            int ch = cmd.substring(6).toInt();
            if (ch >= 1 && ch <= 13) {
                Serial.printf("COMMANDER: JAM_WIFI %d\n", ch);
                setStatus(255, 255, 0); // Yellow = active
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                delay(80); // dwell to match Soldier burst timing
                setStatus(0, 0, 255); // Back to blue
                Serial1.println("CMD_WIFI_ACK");
                Serial1.flush();
            } else {
                Serial.printf("COMMANDER: Invalid channel %d\n", ch);
            }
        } else if (cmd == "JAM_BLE") {
            Serial.println("COMMANDER: JAM_BLE");
            setStatus(255, 255, 0);
            adv->start();
            delay(80);
            adv->stop();
            setStatus(0, 0, 255);
            Serial1.println("CMD_BLE_ACK");
            Serial1.flush();
        } else if (cmd.startsWith("JAM_BOTH")) {
            int ch = cmd.substring(8).toInt();
            if (ch >= 1 && ch <= 13) {
                Serial.printf("COMMANDER: JAM_BOTH ch %d\n", ch);
                setStatus(255, 255, 0);
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                adv->start();
                delay(80);
                adv->stop();
                setStatus(0, 0, 255);
                Serial1.println("CMD_BOTH_ACK");
                Serial1.flush();
            }
        } else if (cmd == "PING") {
            Serial1.println("PONG_CMD");
            Serial1.flush();
        }
    }

    delay(1);
}
