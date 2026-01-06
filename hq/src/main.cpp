/*
 * HQ Controller for ESP32-C3 with built-in 0.42" 72x40 OLED
 * 
 * CONFIRMED PIN ASSIGNMENTS:
 * - GPIO5: OLED SDA
 * - GPIO6: OLED SCL  
 * - GPIO8: LED (active LOW)
 * - GPIO9: Boot button
 *
 * UART PINS:
 * - Commander: TX=GPIO4, RX=GPIO3
 * - Soldier:   TX=GPIO2, RX=GPIO1
 */

#include <Arduino.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <ESPAsyncWebServer.h>

// OLED - Software I2C with SSD1306 72x40 driver
U8G2_SSD1306_72X40_ER_F_SW_I2C display(U8G2_R0, /*scl=*/6, /*sda=*/5, /*reset=*/U8X8_PIN_NONE);

// UART pins
#define CMD_TX 4
#define CMD_RX 3
#define SOL_TX 2
#define SOL_RX 1
#define LED_PIN 8

// Serial ports
HardwareSerial CmdSerial(1);
HardwareSerial SolSerial(0);

AsyncWebServer server(80);
bool cmdReady = false;
bool solReady = false;

const char* SSID = "HQ_JAM";
const char* PASS = "jamhq123";

// Display functions
void oledShow(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    if (l1) display.drawStr(0, 10, l1);
    if (l2) display.drawStr(0, 24, l2);
    if (l3) display.drawStr(0, 38, l3);
    display.sendBuffer();
}

void updateOLED() {
    oledShow("HQ JAM",
             cmdReady ? "CMD: RDY" : "CMD: ---",
             solReady ? "SOL: RDY" : "SOL: ---");
}

// Commands
void sendBoth(const char* cmd) {
    CmdSerial.println(cmd);
    SolSerial.println(cmd);
}

void jamBLE() { sendBoth("JAM_BLE"); }

void jamWifi() {
    for (int ch = 1; ch <= 13; ch++) {
        char buf[20];
        sprintf(buf, "JAM_WIFI %d", ch);
        sendBoth(buf);
        delay(100);
    }
}

void jamBoth() {
    for (int ch = 1; ch <= 13; ch++) {
        char buf[20];
        sprintf(buf, "JAM_BOTH %d", ch);
        sendBoth(buf);
        delay(100);
    }
}

// HTML page
String getHTML() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html += "<title>HQ</title><style>";
    html += "body{font-family:sans-serif;background:#111;color:#fff;padding:20px}";
    html += ".c{max-width:400px;margin:0 auto;background:#222;padding:20px;border-radius:10px}";
    html += "h1{text-align:center;margin:0 0 20px}";
    html += ".s{display:flex;gap:10px;margin-bottom:20px}";
    html += ".b{flex:1;padding:12px;border-radius:8px;text-align:center;font-weight:bold}";
    html += ".ok{background:#1a4;color:#4f8}.no{background:#411;color:#f66}";
    html += "button{width:100%;padding:15px;margin:5px 0;border:none;border-radius:8px;font-size:16px;cursor:pointer;color:#fff}";
    html += ".b1{background:#82e}.b2{background:#28d}.b3{background:#d50}";
    html += "</style></head><body><div class=\"c\">";
    html += "<h1>HQ Control</h1>";
    html += "<div class=\"s\"><div id=\"c\" class=\"b no\">CMD ---</div><div id=\"s\" class=\"b no\">SOL ---</div></div>";
    html += "<button class=\"b1\" onclick=\"f('/ble')\">BLE Jam</button>";
    html += "<button class=\"b2\" onclick=\"f('/wifi')\">WiFi Sweep</button>";
    html += "<button class=\"b3\" onclick=\"f('/both')\">Both</button>";
    html += "</div><script>";
    html += "function f(u){fetch(u,{method:'POST'})}";
    html += "function p(){fetch('/st').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('c').className='b '+(d.c?'ok':'no');";
    html += "document.getElementById('c').innerText='CMD '+(d.c?'RDY':'---');";
    html += "document.getElementById('s').className='b '+(d.s?'ok':'no');";
    html += "document.getElementById('s').innerText='SOL '+(d.s?'RDY':'---');";
    html += "})}setInterval(p,2000);p();";
    html += "</script></body></html>";
    return html;
}

void webSetup() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", getHTML());
    });
    server.on("/st", HTTP_GET, [](AsyncWebServerRequest* r) {
        char json[50];
        sprintf(json, "{\"c\":%s,\"s\":%s}", cmdReady?"true":"false", solReady?"true":"false");
        r->send(200, "application/json", json);
    });
    server.on("/ble", HTTP_POST, [](AsyncWebServerRequest* r) { jamBLE(); r->send(200); });
    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest* r) { jamWifi(); r->send(200); });
    server.on("/both", HTTP_POST, [](AsyncWebServerRequest* r) { jamBoth(); r->send(200); });
    server.begin();
}

// Handshake
void handshake() {
    Serial.println("Handshaking...");
    oledShow("HQ JAM", "Linking...", "");
    
    while (!cmdReady || !solReady) {
        if (!cmdReady) CmdSerial.println("READY_QUERY");
        if (!solReady) SolSerial.println("READY_QUERY");
        
        delay(200);
        
        while (CmdSerial.available()) {
            String r = CmdSerial.readStringUntil('\n');
            r.trim();
            if (r == "COMMANDER_READY") { cmdReady = true; Serial.println("CMD linked"); }
        }
        while (SolSerial.available()) {
            String r = SolSerial.readStringUntil('\n');
            r.trim();
            if (r == "SOLDIER_READY") { solReady = true; Serial.println("SOL linked"); }
        }
        
        updateOLED();
    }
    
    Serial.println("Both linked!");
}

// Setup
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== HQ BOOT ===");
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    
    Serial.println("OLED init...");
    if (display.begin()) {
        Serial.println("OLED OK");
        oledShow("HQ JAM", "Boot...", "");
    } else {
        Serial.println("OLED FAIL");
    }
    
    Serial.printf("CMD UART: RX=%d TX=%d\n", CMD_RX, CMD_TX);
    Serial.printf("SOL UART: RX=%d TX=%d\n", SOL_RX, SOL_TX);
    CmdSerial.begin(115200, SERIAL_8N1, CMD_RX, CMD_TX);
    SolSerial.begin(115200, SERIAL_8N1, SOL_RX, SOL_TX);
    
    WiFi.softAP(SSID, PASS);
    Serial.printf("AP: %s @ %s\n", SSID, WiFi.softAPIP().toString().c_str());
    
    webSetup();
    
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, LOW); delay(100);
        digitalWrite(LED_PIN, HIGH); delay(100);
    }
    
    handshake();
}

// Loop
void loop() {
    static uint32_t last = 0;
    if (millis() - last > 3000) {
        updateOLED();
        last = millis();
    }
    delay(50);
}
