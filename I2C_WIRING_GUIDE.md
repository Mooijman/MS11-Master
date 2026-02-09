# MS11-Master I2C Dual-Bus Wiring Guide

**Date**: February 8, 2026  
**Board**: Seeed Studio XIAO ESP32-S3  
**Configuration**: 2x separate I2C buses for critical isolation

---

## 📍 Pinout Overview

```
┌──────────────────────────────────────────────────────────┐
│           XIAO ESP32-S3 (Top View)                       │
│                                                          │
│   ┌─────────────────────────────────────┐              │
│   │ 6 5 4 3 2 1 0             D10 D9 D8 │              │
│   │                                       │              │
│   │  USB-C      [Seeed Logo]             │              │
│   │                                       │              │
│   │ D4 D3 D2 D1 TX RX GND 3V3   GND 5V  │              │
│   └─────────────────────────────────────┘              │
│                                                          │
│  D-pin mapping:                                         │
│  D4 = GPIO5   ← I2C Slave Bus (SDA) - STANDARD       │
│  D5 = GPIO6   ← I2C Slave Bus (SCL) - STANDARD       │
│  D9 = GPIO8   ← I2C Display Bus (SDA)                │
│  D10= GPIO9   ← I2C Display Bus (SCL)                │
│  D8 = GPIO8   ← I2C Display Bus (SDA)                  │
│  D9 = GPIO9   ← I2C Display Bus (SCL)                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## 🔌 I2C Bus Configuration

### **Bus 0: SLAVE CONTROL BUS** (Critical)
- **Purpose**: Communication with ATmega328P slave controller (0x30)
- **Speed**: 100 kHz (Conservative for reliable control)
- **Pins**:
  - **SDA**: GPIO5 (D4)
  - **SCL**: GPIO6 (D5)
- **Pull-up Resistors**: 4.7 kΩ to 3.3V (both pins)
- **Devices**:
  - ATmega328P @ 0x30 (oven controller)
- **Timeout**: 100ms per transaction
- **Thread-safety**: FreeRTOS mutex protected
- **Critical**: Yes - oven safety depends on this

### **Bus 1: DISPLAY BUS** (Non-critical)
- **Purpose**: OLED display communication (0x3C)
- **Speed**: 100 kHz (Conservative for reliability)
- **Pins**:
  - **SDA**: GPIO8 (D8)
  - **SCL**: GPIO9 (D9)
- **Pull-up Resistors**: 4.7 kΩ to 3.3V (both pins)
- **Devices**:
  - SSD1306 OLED Display @ 0x3C
- **Timeout**: 50ms per transaction
- **Thread-safety**: FreeRTOS mutex protected
- **Critical**: No - can tolerate delays/failures

---

## 🔧 Wiring Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    XIAO ESP32-S3                            │
│                     Master                                  │
└─────────────────────────────────────────────────────────────┘
                          │
     ┌────────────────────┼────────────────────┐
     │                    │                    │
     │        I2C0 (100kHz)      I2C1 (100kHz) │
     │      (Slave Bus)      (Display Bus)    │
     │                                         │
┌────┴────────────────────┐               ┌────────┴─────────┐
│   GPIO5/6 (D4/D5)   │               │  GPIO8/9 (D9/D10) │
│   SDA/SCL           │               │  SDA/SCL         │
│   4.7k pull-ups     │               │  4.7k pull-ups   │
└────┬────────────────┘               └────────┬─────────┘
     │                                         │
     │                                         │
┌────┴─────────────────┐                ┌────┴──────────┐
│ ATmega328P @ 0x30   │                │ SSD1306 @ 0x3C│
│ (Oven Controller)   │                │ (OLED Display) │
│                     │                │                │
│ • Temperature       │                │ • Status       │
│ • Fan Control       │                │ • Telemetry    │
│ • Igniter/Auger     │                │ • IP Address   │
│ • Safety Watchdog   │                │ • Version      │
└─────────────────────┘                └────────────────┘
```

---

## 🔌 Hardware BOM (I2C Components)

| Item | Qty | Value | Notes |
|------|-----|-------|-------|
| Pull-up Resistor | 4 | 4.7 kΩ 1/4W | Use 1% tolerance |
| Decoupling Cap | 2 | 100nF | Near each device |
| I2C Cable | 2x2 | 22AWG twisted pair | Use shielded if >1m |

**Alternative Pull-ups**: If noise issues occur, use 2.2 kΩ instead

---

## 📄 Connection Checklist

### **Slave Bus (Bus 0)**
- [ ] GPIO5 (D4) → ATmega328P SDA via 4.7k pull-up to 3.3V (STANDARD I2C)
- [ ] GPIO6 (D5) → ATmega328P SCL via 4.7k pull-up to 3.3V (STANDARD I2C)
- [ ] GND → ATmega328P GND (common ground)
- [ ] 3.3V → ATmega328P VCC (common power, 3.3V only!)
- [ ] 100nF cap across ATmega328P power pins

### **Display Bus (Bus 1)**
- [ ] GPIO8 (D8) → SSD1306 SDA via 4.7k pull-up to 3.3V
- [ ] GPIO9 (D9) → SSD1306 SCL via 4.7k pull-up to 3.3V
- [ ] GND → SSD1306 GND (common ground with Bus 0 GND!)
- [ ] 3.3V → SSD1306 VCC (common power)
- [ ] 100nF cap across SSD1306 power pins

---

## 🧪 Verification Steps

### Step 1: Power-on test
```bash
# Check serial output during boot:
[I2CManager] ✓ Slave Bus initialized (GPIO5/6 @ 100kHz)
[I2CManager] ✓ Display Bus initialized (GPIO8/9 @ 100kHz)
[SlaveController] ✓ Connected and ready
[DisplayManager] ✓ Display initialized
```

### Step 2: I2C Scan (via web UI)
Navigate to: `http://<IP>/api/i2c/scan`

Expected response:
```json
{
  "devices": [
    {"address": "0x30", "name": "MS11 Slave Controller"},
    {"address": "0x3C", "name": "SSD1306 OLED Display"}
  ],
  "count": 2
}
```

### Step 3: Quick test
- Display should show "MS11 Master" on startup
- Read slave temperature via: `/api/slave/temperature`
- Should return `{"oven_temp": 25, "system_temp": 23}`

---

## 🚨 Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| **Display doesn't appear** | GPIO8/9 bus not responding | Check SDA/SCL connections; verify 4.7k pull-ups |
| **Slave not responding** | GPIO6/7 bus failure | Check I2C wiring; verify slave power (3.3V) |
| **I2C bus lockup** | SDA/SCL stuck low | Check for shorts; disconnect one device at a time |
| **Timeout errors in logs** | noisy I2C line | Use shorter cables; add 100nF caps; switch to 2.2k pull-ups |
| **Both buses fail** | Common GND/power issue | Verify all GND connections; check 3.3V regulator |

---

## 🔐 Bus Isolation Benefits

### **Why Separate Buses?**
1. **Slave bus never blocked** by slow display
2. **Display failures** don't affect oven control
3. **Different speeds** optimized per device (400k vs 100k)
4. **Independent timeouts** (100ms vs 50ms)
5. **Graceful degradation**: Display failure = show error, oven keeps running

### **Example Failure Scenario**
```
Scenario: Display I2C line breaks

Without dual-bus (OLD):
  Wire.write(display) ← BLOCKS FOREVER
  Slave gets no commands ← OVEN STOPS
  ❌ CRITICAL FAILURE

With dual-bus (NEW):
  Bus 1: Display timeout ← LOGGED, display shows error
  Bus 0: Slave commands ← CONTINUE NORMALLY
  ✅ SAFE DEGRADATION
```

---

## 📊 Timing Specifications

| Operation | Bus | Timeout | Retries | Notes |
|-----------|-----|---------|---------|-------|
| Temperature read | Bus 0 | 100ms | 2 | Critical - retried |
| Fan control write | Bus 0 | 100ms | 2 | Critical - retried |
| Display clear | Bus 1 | 50ms | 0 | Non-critical - fail silent |
| OLED refresh | Bus 1 | 50ms | 0 | ~50ms needed per frame |
| Slave ping | Bus 0 | 100ms | 0 | Health check |
| Display ping | Bus 1 | 50ms | 0 | Health check |

---

## 💻 Software APIs

### **Temperature Reading**
```cpp
int16_t temp;
SlaveController::getInstance().readOvenTemp(temp);  // Uses Bus 0 (100kHz)
```

### **Display Update**
```cpp
DisplayManager& display = DisplayManager::getInstance();
display.clear();
display.drawStringCenter(32, "Temperature: 42°C");
display.updateDisplay();  // Uses Bus 1 (100kHz)
```

### **Bus Health Check**
```cpp
bool slaveBusOK = I2CManager::getInstance().isSlaveBusHealthy();   // Pings 0x30
bool displayBusOK = I2CManager::getInstance().isDisplayBusHealthy(); // Pings 0x3C
```

---

## 📚 Related Documentation

- [AGENT_INSTRUCTIONS.md](../../AGENT_INSTRUCTIONS.md#i2c-communication-protocol-v2) - I2C protocol details
- [i2c_manager.h](include/i2c_manager.h) - Dual-bus implementation  
- [slave_controller.h](include/slave_controller.h) - Slave API
- [display_manager.h](include/display_manager.h) - Display API

---

**Last Updated**: February 8, 2026  
**Version**: 1.0  
**Board**: XIAO ESP32-S3 with MS11 dual-bus architecture
