# Hardware Configuration Guide

## System Overview

This project consists of two **ESP32-S3-DevKitC-1** boards working in tandem:
- **COMMANDER**: Wi-Fi jamming controller (root project)
- **SOLDIER**: BLE (Bluetooth Low Energy) jamming unit (soldier/ subdirectory)

Both boards communicate via UART serial connection to coordinate synchronized jamming operations.

---

## Board Specifications

### COMMANDER Board (ESP32-S3-DevKitC-1)
- **MCU**: ESP32-S3 (Dual-core Xtensa LX7, 240MHz)
- **Flash Memory**: 16MB
- **PSRAM**: 8MB (QIO OPI configuration)
- **USB**: Native USB CDC support (monitoring via USB port)
- **Antenna**: U.FL connector for external antenna
- **Max Wi-Fi Power**: 20dBm (configured in firmware)

### SOLDIER Board (ESP32-S3-DevKitC-1)
- **MCU**: ESP32-S3 (Dual-core Xtensa LX7, 240MHz)
- **Flash Memory**: 16MB (default)
- **PSRAM**: Available (default board configuration)
- **USB**: Native USB CDC support
- **BLE Radio**: NimBLE stack
- **Max BLE Power**: +9dBm (ESP_PWR_LVL_P9)

---

## Pin Mapping & Wiring Configuration

### UART Inter-Board Communication

Both boards use the same GPIO pins for serial communication (cross-wired):

| Function | COMMANDER Pin | SOLDIER Pin | Wire Connection |
|----------|--------------|-------------|----------------|
| TX (Transmit) | GPIO 18 | GPIO 18 | Connect COMMANDER GPIO 18 → SOLDIER GPIO 17 |
| RX (Receive) | GPIO 17 | GPIO 17 | Connect COMMANDER GPIO 17 → SOLDIER GPIO 18 |
| Ground | GND | GND | Connect GND → GND |

**Important**: These are **non-strapping pins** chosen specifically to avoid boot mode conflicts.

### Detailed Wire Connections

```
COMMANDER Board                    SOLDIER Board
┌─────────────────┐               ┌─────────────────┐
│                 │               │                 │
│  GPIO 18 (TX) ──┼───────────────┼──> GPIO 17 (RX)│
│                 │               │                 │
│  GPIO 17 (RX) <─┼───────────────┼─── GPIO 18 (TX)│
│                 │               │                 │
│  GND ───────────┼───────────────┼──── GND        │
│                 │               │                 │
│  USB ──> PC     │               │  USB ──> PC    │
│  (Monitoring)   │               │  (Monitoring)  │
└─────────────────┘               └─────────────────┘
```

### Status LED (Both Boards)

| Component | GPIO Pin | Configuration |
|-----------|----------|---------------|
| Onboard WS2812 RGB LED | GPIO 48 | NEO_GRB, 800KHz, Brightness: 50/255 |

**LED Color Codes:**
- 🔴 **Red**: Booting/Waiting for handshake
- 🟢 **Green**: Handshake complete, boards linked
- 🔵 **Blue**: Radio armed and ready
- 🟡 **Yellow**: Active jamming operation (COMMANDER only)

---

## Power Requirements

### Power Supply Options

1. **USB Power (Recommended for Development)**
   - Connect both boards via USB to PC
   - Provides 5V @ up to 500mA per board
   - Allows simultaneous serial monitoring

2. **External Power**
   - 5V regulated supply via 5V pin
   - Recommended: 1A minimum per board under full radio load
   - **Do not exceed 5.5V input**

### Power Consumption Estimates

| Mode | COMMANDER | SOLDIER |
|------|-----------|---------|
| Idle | ~80mA | ~70mA |
| Wi-Fi TX (20dBm) | ~250-350mA | N/A |
| BLE TX (+9dBm) | N/A | ~120-180mA |
| Peak (Both Active) | ~350mA | ~180mA |

---

## Serial Configuration

### UART Settings (Inter-Board Communication)
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Mode**: SERIAL_8N1

### USB Serial Monitor Settings
- **Baud Rate**: 115200
- **COMMANDER**: Auto-detected USB port
- **SOLDIER**: COM6 (configured in platformio.ini, adjust as needed)

---

## Communication Protocol

### Handshake Sequence (Boot)

1. Both boards boot and initialize hardware
2. COMMANDER waits 5 seconds for SOLDIER to stabilize
3. COMMANDER sends `READY_QUERY` via UART
4. SOLDIER responds with `SOLDIER_READY`
5. Both boards set status LED to green, confirming link
6. System proceeds to armed state (blue LEDs)

**Note**: Handshake will retry indefinitely until successful (no timeout).

### Runtime Command Protocol

| Command | Direction | Purpose | Timing |
|---------|-----------|---------|--------|
| `SWEEP_START` | COMMANDER → SOLDIER | Trigger BLE burst | Sent once per Wi-Fi channel |

**Sweep Coordination:**
- COMMANDER sweeps Wi-Fi channels 1-13 sequentially
- For each channel, COMMANDER sends `SWEEP_START` to SOLDIER
- SOLDIER generates BLE advertising bursts for 80ms
- Total dwell time per channel: ~100ms
- Both radios operate simultaneously for maximum interference

---

## Firmware Libraries

### COMMANDER Dependencies
```ini
adafruit/Adafruit NeoPixel @ ^1.12.0
```

### SOLDIER Dependencies
```ini
h2zero/NimBLE-Arduino @ ^1.4.2
adafruit/Adafruit NeoPixel @ ^1.15.2
```

---

## Build Configuration

### COMMANDER (platformio.ini)
```ini
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 16MB
board_build.arduino.memory_type = qio_opi  # Critical for 8MB PSRAM
board_build.f_flash = 80000000L
board_build.partitions = default_16MB.csv
build_flags = 
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
```

### SOLDIER (soldier/platformio.ini)
```ini
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
monitor_port = COM6  # Adjust to your system
upload_port = COM6
```

---

## Safety & Warnings

⚠️ **IMPORTANT WARNINGS:**

1. **Strapping Pins**: GPIO 0, 3, 45, 46 are **strapping pins** on ESP32-S3 and **must not** be used during boot. This design uses GPIO 17/18 specifically to avoid these.

2. **Antenna Connection**: The COMMANDER board operates at maximum Wi-Fi power (20dBm). **Always connect an external antenna to the U.FL connector** before powering on. Operating at high power without an antenna can damage the RF front-end.

3. **Legal Compliance**: This system generates intentional radio interference. **Use only in controlled, shielded environments** or with proper authorization. Unauthorized jamming of Wi-Fi or Bluetooth is **illegal in most jurisdictions** and can result in severe penalties.

4. **Grounding**: Always connect GND between both boards. Improper grounding can cause UART communication failures and unpredictable behavior.

5. **USB Ports**: Both boards can be connected to the same PC via USB. Ensure USB ports provide adequate current (500mA minimum per port).

---

## Troubleshooting

### Handshake Failure (Red LED stuck)

**Symptoms**: One or both boards remain with red LED, handshake never completes.

**Solutions**:
1. Verify cross-wired TX/RX connections (COMMANDER TX → SOLDIER RX, vice versa)
2. Check GND connection between boards
3. Ensure both boards are powered on
4. Re-upload firmware to both boards
5. Check serial monitor for error messages

### Communication Errors During Operation

**Symptoms**: SOLDIER doesn't respond to `SWEEP_START`, erratic behavior.

**Solutions**:
1. Verify baud rate matches (115200) on both boards
2. Check wire connections for loose contacts
3. Add ferrite beads to UART wires if near high-power RF
4. Verify ground connection is solid
5. Check for buffer overflows in serial monitor logs

### Weak Radio Performance

**Symptoms**: Expected jamming range is lower than anticipated.

**Solutions**:
1. COMMANDER: Verify external antenna is connected to U.FL port
2. Check antenna VSWR/impedance matching (50Ω)
3. Verify power settings in firmware (20dBm Wi-Fi, +9dBm BLE)
4. Ensure PSRAM configuration is correct for COMMANDER board
5. Check for RF shielding issues in enclosure

---

## Quick Start Checklist

- [ ] Verify both boards are ESP32-S3-DevKitC-1
- [ ] Connect COMMANDER GPIO 18 → SOLDIER GPIO 17
- [ ] Connect COMMANDER GPIO 17 → SOLDIER GPIO 18
- [ ] Connect GND → GND between boards
- [ ] Attach external antenna to COMMANDER U.FL connector
- [ ] Flash COMMANDER firmware to root project board
- [ ] Flash SOLDIER firmware to soldier project board
- [ ] Connect both boards to PC via USB
- [ ] Open serial monitors (115200 baud)
- [ ] Power on both boards
- [ ] Verify green LEDs after handshake
- [ ] Confirm blue LEDs indicate armed state
- [ ] Observe purple flashes during active jamming

---

## Project Structure

```
COMMANDER/
├── platformio.ini          # COMMANDER board config
├── src/
│   └── main.cpp           # COMMANDER firmware (Wi-Fi jamming)
└── soldier/
    ├── platformio.ini     # SOLDIER board config
    └── src/
        └── main.cpp       # SOLDIER firmware (BLE jamming)
```

---

## Revision History

- **v1.0** (2026-01-05): Initial hardware configuration documentation

---

## Support & Contact

For issues with this hardware configuration:
1. Check serial monitor output from both boards
2. Verify all wire connections with multimeter
3. Review ESP32-S3 datasheet for pin limitations
4. Check PlatformIO build output for memory/partition errors
