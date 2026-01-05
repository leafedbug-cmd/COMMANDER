#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESPAsyncWebServer.h>

// UART pins to Commander and Soldier
// Commander: HQ TX4 -> CMD RX17, HQ RX3 <- CMD TX18
// Soldier:   HQ TX2 -> SOL RX17, HQ RX1 <- SOL TX18
#define CMD_TX 4
#define CMD_RX 3
#define SOL_TX 2
#define SOL_RX 1

// OLED (SH1106 72x40) on I2C - built-in on board
#define OLED_SDA 5
#define OLED_SCL 6

// Onboard LED (active low) and boot button (active low)
#define LED_PIN 8
#define BOOT_BTN 9

HardwareSerial CmdSerial(1);
HardwareSerial SolSerial(2);

// U8g2 for 72x40 SH1106 (NOT SSD1306!)
U8G2_SH1106_72X40_WISE_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

AsyncWebServer server(80);
bool commanderReady = false;
bool soldierReady = false;
bool displayReady = false;

// AP credentials
const char* AP_SSID = "HQ_JAM";
const char* AP_PASS = "jamhq123";

// Helpers
static void sendBoth(const String& msg) {
    CmdSerial.println(msg);
    CmdSerial.flush();
    SolSerial.println(msg);
    SolSerial.flush();
}

static void updateDisplay(const char* note = nullptr) {
    if (!displayReady) return;
    display.clearBuffer();
    
    // Use compact font that fits 72px width
    display.setFont(u8g2_font_6x10_tf);
    
    // 3 lines centered vertically in 40px height
    // Line spacing ~13px, start at Y=11
    display.drawStr(12, 11, "HQ JAM");
    display.drawStr(0, 25, commanderReady ? "CMD:RDY" : "CMD:---");
    display.drawStr(0, 38, soldierReady ? "SOL:RDY" : "SOL:---");
    
    display.sendBuffer();
}

static void jamBleBoth() {
    sendBoth("JAM_BLE");
}

static void jamWifiSweepBoth() {
    for (int ch = 1; ch <= 13; ++ch) {
        String cmd = "JAM_WIFI " + String(ch);
        sendBoth(cmd);
        delay(120);
    }
}

static void jamBothSweep() {
    for (int ch = 1; ch <= 13; ++ch) {
        String cmd = "JAM_BOTH " + String(ch);
        sendBoth(cmd);
        delay(120);
    }
}

static String htmlPage() {
    return String(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>HQ Control</title>
  <style>
        body { font-family: Arial, sans-serif; background: #0d1117; color: #e6edf3; display: flex; justify-content: center; padding: 20px; }
        .card { background: #161b22; padding: 20px; border-radius: 12px; width: 340px; box-shadow: 0 10px 30px rgba(0,0,0,0.35); }
        h1 { margin-top: 0; text-align: center; }
        button { width: 100%; padding: 14px; margin: 8px 0; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; color: #fff; }
    .btn-ble { background: #8e44ad; }
    .btn-wifi { background: #2d89ef; }
    .btn-both { background: #e67e22; }
    .btn-ble:hover { background: #7d3aa0; }
    .btn-wifi:hover { background: #2779d4; }
    .btn-both:hover { background: #cf6d16; }
        .status-box { margin: 12px 0; padding: 12px; background: #0b1620; border-radius: 10px; font-size: 14px; }
        .badge-row { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 6px; }
        .badge { padding: 8px 10px; border-radius: 8px; text-align: center; font-weight: 600; transition: background 0.2s ease, color 0.2s ease; }
        .ok { background: #12351b; color: #4ade80; }
        .wait { background: #3a1a1a; color: #f87171; }
        .muted { color: #a0a6ad; font-size: 13px; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h1>HQ Control</h1>
        <div class="status-box">
            <div class="badge-row">
                <div id="cmdBadge" class="badge wait">Commander: --</div>
                <div id="solBadge" class="badge wait">Soldier: --</div>
            </div>
            <div class="muted" id="status">Waiting for links...</div>
        </div>
    <button class="btn-ble" onclick="cmd('/ble')">BLE Burst (both)</button>
    <button class="btn-wifi" onclick="cmd('/wifiSweep')">WiFi Sweep 1-13 (both)</button>
    <button class="btn-both" onclick="cmd('/bothSweep')">WiFi+BLE Sweep (both)</button>
  </div>
  <script>
        function cmd(path){fetch(path,{method:'POST'}).then(r=>r.json()).then(update)}
        function poll(){fetch('/status').then(r=>r.json()).then(update)}
        function setBadge(el, label, ready){
            el.className = `badge ${ready?'ok':'wait'}`;
            el.innerText = `${label}: ${ready?'Ready':'Waiting'}`;
        }
        function update(data){
            setBadge(document.getElementById('cmdBadge'),'Commander',data.cmd==='Ready');
            setBadge(document.getElementById('solBadge'),'Soldier',data.sol==='Ready');
            document.getElementById('status').innerText = (data.cmd==='Ready'&&data.sol==='Ready') ? 'Both linked. Commands will fire on both.' : 'Waiting for links...';
        }
    setInterval(poll,2000); poll();
  </script>
</body>
</html>
)rawliteral");
}

void setupAP() {
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("HQ AP: %s (%s)\n", AP_SSID, ip.toString().c_str());
}

void setupWeb() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){ req->send(200, "text/html", htmlPage()); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req){
        String json = "{\"cmd\":\"" + String(commanderReady ? "Ready" : "Waiting") + "\",";
        json += "\"sol\":\"" + String(soldierReady ? "Ready" : "Waiting") + "\"}";
        req->send(200, "application/json", json);
    });

    server.on("/ble", HTTP_POST, [](AsyncWebServerRequest* req){ jamBleBoth(); req->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/wifiSweep", HTTP_POST, [](AsyncWebServerRequest* req){ jamWifiSweepBoth(); req->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/bothSweep", HTTP_POST, [](AsyncWebServerRequest* req){ jamBothSweep(); req->send(200, "application/json", "{\"ok\":true}"); });

    server.begin();
    Serial.println("HQ web server started at http://192.168.4.1");
}

void handshake() {
    Serial.println("HQ: Handshaking with Commander & Soldier...");
    unsigned long start = millis();
    updateDisplay("Handshaking");
    while (!(commanderReady && soldierReady)) {
        if (!commanderReady) CmdSerial.println("READY_QUERY");
        if (!soldierReady)   SolSerial.println("READY_QUERY");
        CmdSerial.flush();
        SolSerial.flush();

        unsigned long t0 = millis();
        while (millis() - t0 < 200) {
            if (!commanderReady && CmdSerial.available()) {
                String r = CmdSerial.readStringUntil('\n'); r.trim();
                if (r == "COMMANDER_READY") commanderReady = true;
            }
            if (!soldierReady && SolSerial.available()) {
                String r = SolSerial.readStringUntil('\n'); r.trim();
                if (r == "SOLDIER_READY") soldierReady = true;
            }
            if (displayReady) updateDisplay();
            if (commanderReady && soldierReady) break;
            delay(5);
        }
        if (millis() - start > 10000) {
            Serial.println("HQ: retrying handshake...");
            start = millis();
        }
    }
    Serial.println("HQ: Both Commander and Soldier linked.");
    updateDisplay();
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("HQ: Booting (dual-UART controller)");

    // Initialize display FIRST before UARTs
    Wire.begin(OLED_SDA, OLED_SCL);
    delay(50);
    display.begin();
    display.setContrast(255);
    display.clearBuffer();
    displayReady = true;
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(6, 24, "BOOTING");
    display.sendBuffer();

    // Now init UARTs
    CmdSerial.begin(115200, SERIAL_8N1, CMD_RX, CMD_TX);
    SolSerial.begin(115200, SERIAL_8N1, SOL_RX, SOL_TX);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // off (active low)

    setupAP();
    setupWeb();
    handshake();
}

void loop() {
    // Poll for pings if desired
    static unsigned long lastPing = 0;
    static unsigned long lastOled = 0;
    if (millis() - lastPing > 2000) {
        CmdSerial.println("PING"); CmdSerial.flush();
        SolSerial.println("PING"); SolSerial.flush();
        lastPing = millis();
    }
    if (displayReady && millis() - lastOled > 2000) {
        updateDisplay();
        lastOled = millis();
    }
    delay(10);
}
