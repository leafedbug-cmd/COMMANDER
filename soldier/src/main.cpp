#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>

// Master Coder's Pin Mapping
#define TX_PIN 18  // GPIO18 (safe pin, not strapping)
#define RX_PIN 17  // GPIO17 (safe pin, not strapping)
#define RGB_LED_PIN 48  // Onboard WS2812 RGB LED
#define NUM_PIXELS 1

Adafruit_NeoPixel statusLED(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    // 0. Initialize Status LED
    statusLED.begin();
    statusLED.setBrightness(50); // 0-255
    statusLED.setPixelColor(0, statusLED.Color(255, 0, 0)); // Red = Booting/Waiting
    statusLED.show();

    // 1. Initialize Serial Interfaces FIRST - critical for handshake timing
    Serial.begin(115200); // USB Monitor
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // Explicit GPIO43 (RX) / GPIO44 (TX)
    delay(100); // Brief stabilization for UART
    
    while(!Serial && millis() < 2000) { delay(10); } // Wait up to 2s for USB Serial
    
    Serial.println("\n\n========================================");
    Serial.println("SOLDIER: Booting...");
    Serial.printf("SOLDIER: RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);
    Serial.println("========================================");

    // 2. Handshake Loop: Wait for Commander (Lonely Binary) signal - NO TIMEOUT
    Serial.println("SOLDIER: Waiting for Commander's READY_QUERY...");
    Serial.println("SOLDIER: Will wait indefinitely for handshake...");
    
    // Clear any stale data in buffer
    while (Serial1.available()) { Serial1.read(); }
    
    // Wait forever until COMMANDER connects
    while (true) {
        if (Serial1.available()) {
            String msg = Serial1.readStringUntil('\n');
            msg.trim(); // Remove whitespace
            Serial.printf("SOLDIER: Received: '%s'\n", msg.c_str());
            
            if (msg.indexOf("READY_QUERY") != -1) {
                Serial.println("SOLDIER: Sending SOLDIER_READY response...");
                Serial1.println("SOLDIER_READY");
                Serial1.flush(); // Ensure data is sent
                
                Serial.println("SOLDIER: Handshake complete. Linked to Commander.");
                statusLED.setPixelColor(0, statusLED.Color(0, 255, 0)); // Green = Connected
                statusLED.show();
                delay(1000); // Hold green for visibility
                break;
            }
        }
        delay(10); // Small delay to prevent busy waiting
    }

    // 3. BLE Radio Setup
    Serial.println("SOLDIER: Initializing BLE Radio...");
    NimBLEDevice::init("");
    
    // Set Power to P9 (+9dBm) for maximum BLE saturation
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); 
    
    statusLED.setPixelColor(0, statusLED.Color(0, 0, 255)); // Blue = BLE Armed
    statusLED.show();
    
    Serial.println("SOLDIER: BLE Radio Armed at Max Power (+9dBm).");
    Serial.println("SOLDIER: Ready for SWEEP_START commands.");
}

void loop() {
    // 4. Reactive Jamming Loop - Listen for Commander's sweep commands
    if (Serial1.available()) {
        String cmd = Serial1.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.indexOf("SWEEP_START") != -1) {
            Serial.println("SOLDIER: >>> SWEEP_START - BLE BURST ACTIVE <<<");
            
            // Purple flash = Jamming active
            statusLED.setPixelColor(0, statusLED.Color(255, 0, 255));
            statusLED.show();
            
            // Generate high-intensity advertising packets on BLE channels
            NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
            
            // Flood the advertising channels (37, 38, 39) - 2.4GHz overlap with Wi-Fi
            pAdvertising->start();
            delay(80); // Match Commander's dwell time (100ms - processing overhead)
            pAdvertising->stop();
            
            // Back to blue = Armed and ready
            statusLED.setPixelColor(0, statusLED.Color(0, 0, 255));
            statusLED.show();
        }
    }
}
