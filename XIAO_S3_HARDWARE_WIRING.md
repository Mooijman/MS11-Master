# XIAO ESP32-S3 Hardware Wiring Diagram & Guide

**Date**: 8 February 2026  
**Board**: Seeed Studio XIAO ESP32-S3  
**Firmware Version**: 2026.1.1.11  
**Status**: ✅ Production Ready

---

## 📐 Complete Pinout Overview

```
┌────────────────────────────────────────────────────────────────┐
│                    XIAO ESP32-S3 (Top View)                   │
│                                                                │
│  D0  D2  D5  D6  D9              GND  3V3  5V  USB-C          │
│  ↓   ↓   ↓   ↓   ↓              ↑    ↑    ↑   ↑              │
│  ┌──┬──┬──┬──┬──┬──────....────┬──┬──┬──┬──┐              │
│  │1 │3 │6 │43│8 │              │GND│3V│5V│C│              │
│  │  │  │  │  │  │  [Seeed]     │   │3 │  │  │              │
│  │2 │4 │5 │44│7 │              │GND│  │  │  │              │
│  └──┴──┴──┴──┴──┴──────....────┴──┴──┴──┴──┘              │
│  D1  D3  D4  D7  D8  D10         GND  3V  5V                  │
│  ↑   ↑   ↑   ↑   ↑   ↑           ↓    ↓   ↓                  │
│                                                                │
│  GPIO Mapping (Official Seeed Pinout):                        │
│  D0=GPIO1   (PWM) → Status LED                               │
│  D1=GPIO2        → Button 2                                  │
│  D2=GPIO3        → Button 1                                  │
│  D3=GPIO4        → Power Switch                              │
│  D4=GPIO5   (I2C) → Slave Bus SDA (standard I2C)            │
│  D5=GPIO6   (I2C) → Slave Bus SCL (standard I2C)            │
│  D6=GPIO43  (TX)  → Not used                                 │
│  D7=GPIO44  (RX)  → Not used                                 │
│  D8=GPIO7        → Not used (available for GPIO)            │
│  D9=GPIO8   (I2C) → Display Bus SDA                          │
│  D10=GPIO9  (I2C) → Display Bus SCL                          │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## 🔌 Complete System Wiring Diagram

```
╔════════════════════════════════════════════════════════════════════════════╗
║                     XIAO ESP32-S3 Full System Diagram                      ║
║                                                                            ║
║  ┌──────────────────────────────────────────────────────────────────┐     ║
║  │                    XIAO ESP32-S3 Master                          │     ║
║  │  ┌───────────────────────────────────────────────────────────┐  │     ║
║  │  │ GPIO Digital I/O Control                                 │  │     ║
║  │  │                                                          │  │     ║
║  │  │  GPIO1 (D0) ──→ Status LED (PWM brightness 0-255)      │  │     ║
║  │  │             ───→ [Anode] LED [Cathode]→GND            │  │     ║
║  │  │                                                          │  │     ║
║  │  │  GPIO2 (D1) ←── Button 2 (active-low)                 │  │     ║
║  │  │             ───→ [C]ommon ← [NO]→GND (when pressed)   │  │     ║
║  │  │                                                          │  │     ║
║  │  │  GPIO3 (D2) ←── Button 1 (active-low)                 │  │     ║
║  │  │             ───→ [C]ommon ← [NO]→GND (when pressed)   │  │     ║
║  │  │                                                          │  │     ║
║  │  │  GPIO4 (D3) ←── Power Switch (active-high)            │  │     ║
║  │  │             ←──  [C]ommon connected to 3.3V (ON)      │  │     ║
║  │  └───────────────────────────────────────────────────────────┘  │     ║
║  │                                                                  │     ║
║  │  ┌────────────────────────────────────────────────────────────┐ │     ║
║  │  │ BUS 0: Critical Slave Control (100 kHz)                  │ │     ║
║  │  │ GPIO5 (D4) = SDA ─┬─→ [to pull-up network]             │ │     ║
║  │  │ GPIO6 (D5) = SCL ─┼─→ [to pull-up network]             │ │     ║
║  │  │                   │                                     │ │     ║
║  │  │  ⭐ Uses STANDARD I2C pins for best compatibility      │ │     ║
║  │  │  Pull-up: 4.7kΩ resistors to 3.3V                      │ │     ║
║  │  │  Speed: 100 kHz (conservative, reliable)                │ │     ║
║  │  └────────────────────────────────────────────────────────────┘ │     ║
║  │           ↓ (on same bus)                                       │     ║
║  │           │                                                     │     ║
║  │  ┌────────┴────────────────────────────────────────────────┐   │     ║
║  │  │  Slave Controller: ATmega328P @ 0x30                   │   │     ║
║  │  │  [Oven Control: Temp, Fan, Igniter, Auger, Safety]    │   │     ║
║  │  │  SDA ← GPIO5 (D4) from XIAO                            │   │     ║
║  │  │  SCL ← GPIO6 (D5) from XIAO                            │   │     ║
║  │  │  GND ← Common ground                                   │   │     ║
║  │  │  VCC ← 3.3V from XIAO                                  │   │     ║
║  │  │  Decap: 100nF ceramic (VCC-GND)                        │   │     ║
║  │  └────────────────────────────────────────────────────────┘   │     ║
║  │                                                                  │     ║
║  │  ┌────────────────────────────────────────────────────────────┐ │     ║
║  │  │ BUS 1: Display & Navigation (100 kHz)                   │ │     ║
║  │  │ GPIO8 (D9) = SDA ─┬─→ [to pull-up network]             │ │     ║
║  │  │ GPIO9 (D10)= SCL ─┼─→ [to pull-up network]             │ │     ║
║  │  │                   │                                     │ │     ║
║  │  │  Pull-up: 4.7kΩ resistors to 3.3V (same network)       │ │     ║
║  │  │  Speed: 100 kHz (conservative, reliable)                │ │     ║
║  │  └────────────────────────────────────────────────────────────┘ │     ║
║  │           ↓ (all devices on same bus)                          │     ║
║  │           │                                                     │     ║
║  │  ┌────────┴────────────────────────────────────────────────┐   │     ║
║  │  │  OLED Display: SSD1306 @ 0x3C                           │   │     ║
║  │  │  [128x64 pixels status display]                         │   │     ║
║  │  │  SDA ← GPIO8 (D9) from XIAO                             │   │     ║
║  │  │  SCL ← GPIO9 (D10) from XIAO                            │   │     ║
║  │  │  GND ← Common ground                                    │   │     ║
║  │  │  VCC ← 3.3V from XIAO                                   │   │     ║
║  │  │  Decap: 100nF ceramic (VCC-GND)                         │   │     ║
║  │  └────────────────────────────────────────────────────────┘   │     ║
║  │           ↓ (shared bus)                                       │     ║
║  │  ┌────────┴────────────────────────────────────────────────┐   │     ║
║  │  │  LCD 16x2: PCF8574 I2C Backpack @ 0x27                 │   │     ║
║  │  │  [16x2 character display with I2C interface]            │   │     ║
║  │  │  SDA ← GPIO8 (D9) from XIAO (shared)                    │   │     ║
║  │  │  SCL ← GPIO9 (D10) from XIAO (shared)                   │   │     ║
║  │  │  GND ← Common ground                                    │   │     ║
║  │  │  VCC ← 3.3V from XIAO                                   │   │     ║
║  │  │  Decap: 100nF ceramic (VCC-GND)                         │   │     ║
║  │  └────────────────────────────────────────────────────────┘   │     ║
║  │           ↓ (shared bus)                                       │     ║
║  │  ┌────────┴────────────────────────────────────────────────┐   │     ║
║  │  │  Seesaw Rotary Encoder @ 0x36                           │   │     ║
║  │  │  [Rotary encoder + button for UI navigation]            │   │     ║
║  │  │  SDA ← GPIO8 (D9) from XIAO (shared)                    │   │     ║
║  │  │  SCL ← GPIO9 (D10) from XIAO (shared)                   │   │     ║
║  │  │  GND ← Common ground                                    │   │     ║
║  │  │  VCC ← 3.3V from XIAO                                   │   │     ║
║  │  │  Decap: 100nF ceramic (VCC-GND)                         │   │     ║
║  │  └────────────────────────────────────────────────────────┘   │     ║
║  │                                                                  │     ║
║  └──────────────────────────────────────────────────────────────────┘     ║
║                                                                            ║
║  ┌────────────────────────────────────────────────────────────────────┐   ║
║  │ Optional: UART Serial (GPIO43/44)                                │   ║
║  │ (Currently unused - available for future serial debug)           │   ║
║  │ D6 (GPIO43) = TX (to USB→Serial adapter)                         │   ║
║  │ D7 (GPIO44) = RX (from USB→Serial adapter)                       │   ║
║  └────────────────────────────────────────────────────────────────────┘   ║
║                                                                            ║
║  ┌────────────────────────────────────────────────────────────────────┐   ║
║  │ Power & Ground (Critical)                                         │   ║
║  │                                                                    │   ║
║  │ USB-C 5V (500mA minimum) → Onboard LDO → 3.3V rail               │   ║
║  │ GND: Common to ALL devices (star topology recommended)           │   ║
║  │                                                                    │   ║
║  │ Recommended: 10µF bulk cap on 3.3V rail (optional)               │   ║
║  └────────────────────────────────────────────────────────────────────┘   ║
║                                                                            ║
╚════════════════════════════════════════════════════════════════════════════╝
```

---

## 📋 I2C Pull-up Network (CRITICAL!)

```
                    3.3V Rail
                       │
                       │
          ┌────────────┼────────────┐
          │            │            │
         4.7k         4.7k         │
         Ω             Ω            │
          │            │            │
          ├───SDA (Bus 0/1)         │
          │            │            │
          ├───SCL (Bus 0/1)         │
          │            │            │
    [Pull-up R]  [Pull-up R]
    (40-100mA    (40-100mA
     capacity)    capacity)

DETAILED CONNECTION:
┌──────────────────┬──────────────────┐
│  BUS 0            │  BUS 1            │
│ (100 kHz)         │ (100 kHz)         │
├──────────────────┼──────────────────┤
│ GPIO5 → resistor │ GPIO8 → resistor │
│ GPIO6 → resistor │ GPIO9 → resistor │
│ All tied to 3.3V │ All tied to 3.3V │
│ ⭐ STANDARD I2C  │                  │
└──────────────────┴──────────────────┘

DECOUPLING (per I2C device):
    VCC ──[100nF]──┬── GND
                   │
              [Device I2C]
                   │
                  GND
```

**RESISTANCE VALUES:**
- **Standard**: 4.7kΩ (most common, well-tested)
- **Alternative**: 2.7kΩ (if noise issues, lower impedance)
- **DO NOT USE**: >10kΩ (rise time too slow for 100kHz)

---

## 🔗 Detailed Connection Instructions

### **SECTION 1: GPIO Digital I/O**

#### **Status LED (GPIO1 / D0)**
```
Option A: Direct LED (low brightness)
┌─────────────────────────────────────┐
│  GPIO1 (D0)                         │
│    ├─→ [100Ω resistor] ──┐         │
│    │                      │         │
│    │                    [LED]       │
│    │                      │         │
│    └──────────────────────┴─ GND   │
│                                     │
│  LED Polarity: Long leg = Anode    │
│                (to GPIO side)      │
└─────────────────────────────────────┘

Option B: Higher Power LED (transistor driver)
┌────────────────────────────────────────────┐
│  GPIO1 (D0)                                │
│    │                                       │
│    └─→ [10k resistor] ──┐                │
│                          │                │
│                    [NPN Transistor]      │
│                    (2N2222 or 2N3904)    │
│                          │                │
│                          ├─→ [LED+resistor]
│                          │                │
│                          └─ GND          │
│                                           │
│  Advantage: Higher current capability    │
└────────────────────────────────────────────┘
```

**Testing**:
```cpp
// In loop:
GPIOManager::getInstance().setLEDBrightness(200);  // 0-255
GPIOManager::getInstance().ledBlink(100, 100, 5);  // 5 blinks
GPIOManager::getInstance().ledPulse(2000);         // 2s fade
```

---

#### **Button 1 (GPIO3 / D2) - Active Low**
```
         [NO contact + COM]
              ↓
         ┌─[Switch]─┐
         │           │
         ↓           ↓
      3.3V          GND
         │           │
      [Pull-up      │
       10kΩ]        │
         │           │
         ├───GPIO3 (D2) ← Digital Input
         │
    [Debounce Cap]
    (optional: 1nF)
         │
        GND

Wire from GPIO3 → common pin of button
Wire from opposite side of button → GND
(Button1 has internal pull-up in code)
```

**Button Specifications**:
- **Type**: Momentary tactile switch (6mm × 6mm)
- **Contact**: COM (common) to one terminal
- **Action**: When pressed → COM connects to GND
- **Debounce**: Hardware debounce 20ms in code

**Testing**:
```cpp
// In loop:
if (GPIOManager::getInstance().getButtonEvent(1) == BTN_EVENT_CLICK) {
    Serial.println("Button 1 clicked!");
}
if (GPIOManager::getInstance().getLastButtonEvent(1) == BTN_EVENT_LONGPRESS) {
    Serial.println("Button 1 long pressed!");
}
```

---

#### **Button 2 (GPIO2 / D1) - Active Low**
```
Same as Button 1 but uses GPIO2 (D1)

       [NO contact + COM]
            ↓
       ┌─[Switch]─┐
       │           │
       ↓           ↓
    3.3V          GND
       │           │
    [Pull-up      │
     10kΩ]        │
       │           │
       ├───GPIO2 (D1) ← Digital Input
       │
  [Debounce Cap]
  (optional: 1nF)
       │
      GND
```

**Testing**:
```cpp
// In loop:
if (GPIOManager::getInstance().getButtonEvent(2) == BTN_EVENT_CLICK) {
    Serial.println("Button 2 clicked!");
}
```

---

#### **Power Switch (GPIO4 / D3) - Active High**
```
         [Toggle Switch]
              ↓
         ┌─[Switch]─┐
         │           │
         ↓           ↓
       GND         3.3V
         │           │
      [Pull-dn      │
       10kΩ]        │
         │           │
         ├───GPIO4 (D3) ← Digital Input
         │
    [Debounce Cap]
    (optional: 1nF)
         │
        GND

Wire from GPIO4 → common pin of switch
Wire from opposite side of switch → 3.3V
(Switch ON = 3.3V on GPIO, Switch OFF = GND)
```

**Switch Specifications**:
- **Type**: Toggle switch (2-position)
- **Contact**: COM (common) to GPIO4
- **Action**: Toggle toward 3.3V rail to turn ON
- **Debounce**: Hardware debounce 20ms in code

**Testing**:
```cpp
// In loop:
if (GPIOManager::getInstance().isPowerSwitchOn()) {
    Serial.println("Power is ON");
}
if (GPIOManager::getInstance().isPowerSwitchChanged()) {
    Serial.println("Power state changed!");
}
```

---

### **SECTION 2: I2C BUS 0 (Slave Control - Critical)**

```
┌───────────────────────────────────────────────────────┐
│ BUS 0: Slave Control (100 kHz, Critical)             │
├───────────────────────────────────────────────────────┤
│                                                       │
│  From XIAO ESP32-S3 (STANDARD I2C PINS):            │
│  ├─ GPIO5 (D4) ──→ Pull-up network ──→ SDA (Bus 0) │
│  ├─ GPIO6 (D5) ──→ Pull-up network ──→ SCL (Bus 0) │
│  └─ GND ─────────────────────────────→ GND (all)   │
│                                                       │
│  To Devices (on same I2C bus):                      │
│  ├─ ATmega328P (Slave Controller) @ 0x30           │
│  │   ├─ SDA ← GPIO5 (D4)                           │
│  │   ├─ SCL ← GPIO6 (D5)                           │
│  │   ├─ GND ← Common ground                        │
│  │   ├─ VCC ← 3.3V                                 │
│  │   └─ Decap: 100nF (VCC-GND)                     │
│  │                                                   │
│  └─ (No other devices on Bus 0)                    │
│                                                       │
│  Pull-up Resistors (CRITICAL):                      │
│  ├─ 4.7kΩ from GPIO5 (D4) to 3.3V                 │
│  ├─ 4.7kΩ from GPIO6 (D5) to 3.3V                 │
│  └─ Caps: 100nF at GPIO side (optional)           │
│                                                       │
│  Twisted Pair Cable (Recommended):                  │
│  ├─ Length: < 1 meter                              │
│  ├─ Gauge: 22 AWG (0.6mm)                          │
│  ├─ Shielding: Yes (if >0.5m or noisy env)        │
│  └─ Pair: SDA with SCL (twist together)           │
│                                                       │
└───────────────────────────────────────────────────────┘
```

**Step-by-step wiring**:
1. Connect XIAO GPIO5 (D4) → 4.7kΩ resistor → 3.3V
2. Connect XIAO GPIO6 (D5) → 4.7kΩ resistor → 3.3V
3. Connect XIAO GND → ATmega328P GND (common star)
4. Connect GPIO5 (D4) → ATmega328P SDA
5. Connect GPIO6 (D5) → ATmega328P SCL
6. Connect 3.3V → ATmega328P VCC
7. Add 100nF capacitor across ATmega328P VCC-GND (close to pins)

---

### **SECTION 3: I2C BUS 1 (Display Bus - Non-Critical)**

```
┌───────────────────────────────────────────────────────────────┐
│ BUS 1: Display & Navigation (100 kHz, Non-critical)          │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  From XIAO ESP32-S3:                                        │
│  ├─ GPIO8 (D9)  ──→ Pull-up network ──→ SDA (Bus 1)        │
│  ├─ GPIO9 (D10) ──→ Pull-up network ──→ SCL (Bus 1)        │
│  └─ GND ──────────────────────────────→ GND (all)          │
│                                                               │
│  To Devices (ALL on same I2C bus, different addresses):     │
│  ├─ OLED Display (SSD1306) @ 0x3C                          │
│  │   ├─ SDA ← GPIO8 (D9)                                   │
│  │   ├─ SCL ← GPIO9 (D10)                                  │
│  │   ├─ GND ← Common ground                                │
│  │   ├─ VCC ← 3.3V                                         │
│  │   └─ Decap: 100nF (VCC-GND)                             │
│  │                                                           │
│  ├─ LCD 16x2 (PCF8574 Backpack) @ 0x27                    │
│  │   ├─ SDA ← GPIO8 (D9) [SHARED]                          │
│  │   ├─ SCL ← GPIO9 (D10) [SHARED]                         │
│  │   ├─ GND ← Common ground                                │
│  │   ├─ VCC ← 3.3V                                         │
│  │   └─ Decap: 100nF (VCC-GND)                             │
│  │                                                           │
│  └─ Seesaw Rotary Encoder @ 0x36                          │
│      ├─ SDA ← GPIO8 (D9) [SHARED]                          │
│      ├─ SCL ← GPIO9 (D10) [SHARED]                         │
│      ├─ GND ← Common ground                                │
│      ├─ VCC ← 3.3V                                         │
│      └─ Decap: 100nF (VCC-GND)                             │
│                                                               │
│  Pull-up Resistors (CRITICAL):                              │
│  ├─ 4.7kΩ from GPIO8 (D9) to 3.3V                         │
│  ├─ 4.7kΩ from GPIO9 (D10) to 3.3V                        │
│  └─ Note: Same resistor network as Bus 0 if done in        │
│     shared pull-up section (common practice)               │
│                                                               │
│  Twisted Pair Cable:                                        │
│  ├─ Length: < 1 meter each device                          │
│  ├─ Gauge: 22 AWG recommended                              │
│  └─ Bus topology: Daisy-chain or star (star preferred)    │
│                                                               │
│  Key Addresses Used:                                        │
│  ├─ 0x3C: OLED SSD1306                                     │
│  ├─ 0x27: LCD PCF8574 (alt: 0x3F)                         │
│  └─ 0x36: Seesaw Rotary Encoder                           │
│     (all UNIQUE - no conflicts)                            │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

**Step-by-step wiring**:
1. Connect XIAO GPIO8 (D9) → 4.7kΩ resistor → 3.3V
2. Connect XIAO GPIO9 (D10) → 4.7kΩ resistor → 3.3V
3. Connect XIAO GND → Common ground (star topology)

Then for each device:
4. **OLED Display**:
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common ground
   - Add 100nF cap VCC-GND

5. **LCD 16x2**:
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common ground
   - Add 100nF cap VCC-GND

6. **Seesaw Rotary Encoder**:
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common ground
   - Add 100nF cap VCC-GND

---

## 🔋 Power Supply Requirements

```
┌─────────────────────────────────────────────────────┐
│ Power Chain Analysis                                │
├─────────────────────────────────────────────────────┤
│                                                     │
│  USB-C 5V Input (500mA minimum REQUIRED)          │
│       ↓                                             │
│  XIAO onboard LDO regulator                       │
│       ↓                                             │
│  3.3V regulated output                            │
│       ↓                                             │
│  Distribution to devices:                         │
│                                                     │
│  Peak Current Draw:                                │
│  ├─ ESP32-S3 core: ~100mA (240MHz)               │
│  ├─ WiFi transmission: ~100-200mA                │
│  ├─ OLED display: ~20mA                          │
│  ├─ LCD display: ~10mA                           │
│  ├─ Seesaw encoder: ~5mA                         │
│  ├─ Buttons (input only): 0mA                    │
│  ├─ Power switch (input only): 0mA               │
│  ├─ Status LED (full brightness): ~20mA          │
│  ├─ ATmega328P (active): ~30mA                   │
│  └─ I2C pull-ups (4.7k × 4): ~7mA                │
│     ─────────────────────────────────            │
│     TOTAL PEAK: ~400mA                            │
│                                                     │
│  Typical Operating (WiFi idle):                    │
│  └─ ~80-120mA (well within 500mA limit)           │
│                                                     │
│  USB Cable Requirements:                           │
│  ├─ Minimum: 500mA capacity (5V/0.5A)            │
│  ├─ Recommended: 2A rated cable (1A+ per line)   │
│  ├─ CRITICAL: Use quality cables (no cheap ones) │
│  └─ Test: USB cable should not get warm          │
│                                                     │
│  Bulk Capacitor (Optional but Recommended):       │
│  ├─ 10µF electrolytic across 3.3V → GND         │
│  ├─ Placement: Close to XIAO VCC pin             │
│  └─ Benefit: Stabilizes voltage spikes           │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**CRITICAL**: A cheap USB-C cable with thin wires can cause:
- Voltage droop (drops below 3.0V)
- Erratic WiFi behavior
- I2C communication errors
- LED flickering
- Intermittent button failures

**Test your cable**: Use a multimeter on USB-C to verify 5V under load.

---

## 📋 Bill of Materials (BOM)

| Component | Quantity | Specifications | Notes |
|-----------|----------|----------------|-------|
| **XIAO ESP32-S3** | 1 | Seeed Studio | Main controller |
| **Tactile Button** | 2 | 6mm×6mm, NO contact | Buttons 1 & 2 |
| **Toggle Switch** | 1 | 2-position, SPDT | Power switch |
| **Status LED** | 1 | 5mm, any color | GPIO1 with 100Ω current limit |
| **Resistor 4.7kΩ** | 4 | 1/4W, 1% tolerance | I2C pull-ups (2 per bus) |
| **Resistor 100Ω** | 1 | 1/4W | LED current limiting |
| **Capacitor 100nF** | 5+ | Ceramic, 50V | Decoupling (1 per I2C device + extras) |
| **Capacitor 10µF** | 1 | Electrolytic, 16V | Bulk cap on 3.3V rail (optional) |
| **ATmega328P** | 1 | With bootloader | Slave controller |
| **SSD1306 OLED** | 1 | 0.96" 128×64 I2C | Display |
| **LCD 16x2** | 1 | PCF8574 backpack | Character display |
| **Seesaw Encoder** | 1 | Adafruit rotary @ 0x36 | Navigation control |
| **Twisted Pair Cable** | 3m | 22 AWG, shielded | I2C bus wiring |
| **USB-C Cable** | 1 | 500mA+ rated | Power supply (CRITICAL!) |

---

## ✅ Pre-Assembly Checklist

- [ ] All components from BOM acquired
- [ ] USB-C cable verified for 500mA+ rating
- [ ] Resistors sorted and labeled (especially 4.7k and 100Ω)
- [ ] Capacitors organized (100nF bulk, 10µF single)
- [ ] Buttons and switch mechanically tested
- [ ] LED polarity verified (long leg = anode)
- [ ] Breadboard or prototyping area prepared
- [ ] Multimeter available for testing
- [ ] Magnifying glass for checking connections

---

## 🔧 Step-by-Step Assembly Guide

### **Phase 1: GPIO Digital I/O (5 minutes)**

1. **Status LED (GPIO1/D0)**:
   - Solder 100Ω resistor to LED anode (long leg)
   - Connect resistor → GPIO1 (D0)
   - Connect LED cathode (short leg) → GND

2. **Button 1 (GPIO3/D2)**:
   - Button common pin → GPIO3 (D2)
   - Button NO pin → GND
   - (Pull-up resistor already in ESP32 code)

3. **Button 2 (GPIO2/D1)**:
   - Button common pin → GPIO2 (D1)
   - Button NO pin → GND

4. **Power Switch (GPIO4/D3)**:
   - Switch common pin → GPIO4 (D3)
   - Switch 3.3V position → 3.3V rail
   - Switch GND position → GND

### **Phase 2: I2C Bus 0 Pull-up Network (3 minutes)**

1. **Resistor Network Setup** (STANDARD I2C PINS):
   - Solder 4.7kΩ resistor: GPIO5 (D4) → 3.3V
   - Solder 4.7kΩ resistor: GPIO6 (D5) → 3.3V
   - Add 100nF decap near nodes (optional)

2. **Verify connectivity** with multimeter:
   - GPIO5 → 3.3V: ~3.0V with 10kΩ pull-down load
   - GPIO6 → 3.3V: ~3.0V with 10kΩ pull-down load

### **Phase 3: I2C Bus 1 Pull-up Network (3 minutes)**

1. **Same as Bus 0 but different pins**:
   - Solder 4.7kΩ resistor: GPIO8 (D9) → 3.3V
   - Solder 4.7kΩ resistor: GPIO9 (D10) → 3.3V
   - Add 100nF decap near nodes (optional)

### **Phase 4: ATmega328P Slave Controller (5 minutes)**

1. **Wiring** (STANDARD I2C PINS):
   - ATmega328P SDA → GPIO5 (D4)
   - ATmega328P SCL → GPIO6 (D5)
   - ATmega328P VCC → 3.3V
   - ATmega328P GND → Common GND
   - Add 100nF cap across VCC-GND

2. **Program ATmega328P** (via ISP):
   - Use Arduino ISP or compatible programmer
   - Load appropriate slave firmware
   - Verify I2C address 0x30

### **Phase 5: OLED Display (2 minutes)**

1. **Connect to Bus 1**:
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common GND
   - Add 100nF cap across VCC-GND

2. **Test with I2C Scanner**:
   - Should show 0x3C in scan results

### **Phase 6: LCD 16x2 Display (2 minutes)**

1. **Connect to Bus 1** (same as OLED):
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common GND
   - Add 100nF cap across VCC-GND

2. **Test address**:
   - Default: 0x27 (check your backpack)
   - Alternative: 0x3F (some models)

### **Phase 7: Seesaw Rotary Encoder (2 minutes)**

1. **Connect to Bus 1** (same as OLED/LCD):
   - SDA → GPIO8 (D9)
   - SCL → GPIO9 (D10)
   - VCC → 3.3V
   - GND → Common GND
   - Add 100nF cap across VCC-GND

2. **Test rotation**:
   - Watch serial output for position changes

### **Phase 8: Power & Final Assembly (5 minutes)**

1. **3.3V Rail final check**:
   - Multimeter: 3.3V ± 0.2V under load
   - No drops below 3.0V

2. **GND Star Connection**:
   - All GND connections tied together
   - No separate return paths

3. **USB-C Cable**:
   - Connect quality 500mA+ rated cable
   - Device should boot immediately
   - Serial monitor shows debug output

---

## 🧪 Testing & Verification

### **Test 1: GPIO Digital I/O**
```cpp
// Status LED
GPIOManager::getInstance().setLEDBrightness(255);  // LED on full
delay(500);
GPIOManager::getInstance().setLEDBrightness(0);    // LED off

// Button 1
Serial.print("Button 1 pressed: ");
Serial.println(GPIOManager::getInstance().isButtonPressed(1) ? "YES" : "NO");

// Button 2
Serial.print("Button 2 pressed: ");
Serial.println(GPIOManager::getInstance().isButtonPressed(2) ? "YES" : "NO");

// Power Switch
Serial.print("Power switch: ");
Serial.println(GPIOManager::getInstance().isPowerSwitchOn() ? "ON" : "OFF");
```

### **Test 2: I2C Bus Scan**
```
Open: http://<ip>/api/i2c/scan

Expected output:
{
  "devices": [
    {"address": "0x30", "name": "ATmega328P Slave"},
    {"address": "0x3C", "name": "SSD1306 OLED"},
    {"address": "0x27", "name": "PCF8574 LCD"},
    {"address": "0x36", "name": "Seesaw Rotary"}
  ],
  "count": 4
}
```

### **Test 3: OLED Display**
```
Serial output should show:
[DisplayManager] ✓ OLED Display initialized (SSD1306 @ 0x3C)
```

### **Test 4: LCD 16x2**
```
Serial output should show:
[LCDManager] ✓ LCD 16x2 initialized (PCF8574 @ 0x27)
```

### **Test 5: Seesaw Rotary**
```
Serial output should show:
[SeesawRotary] ✓ Seesaw initialized (Rotary @ 0x36)

Then rotate encoder and monitor:
Seesaw Position: 1234 (increases/decreases smoothly)
```

---

## 🆘 Troubleshooting

| Issue | Likely Cause | Solution |
|-------|-------------|----------|
| **OLED not showing** | I2C connection | Check GPIO8/9 and pull-ups |
| **Buttons not responding** | GPIO2/3 not connected properly | Verify GND connection |
| **LED not lighting** | GPIO1 not connected or LED polarity reversed | Check 100Ω resistor polarity |
| **I2C scan shows no devices** | Pull-up resistors missing/wrong value | Verify 4.7kΩ resistors to 3.3V |
| **Slave controller not responding** | I2C Bus 0 issue | Check GPIO6/7 and ATmega power |
| **WiFi drops randomly** | Unstable power supply | Use quality USB-C cable |
| **Power switch always reads OFF** | Wiring to wrong voltage | Verify connected to 3.3V rail |
| **5V overdriven onto 3.3V GPIO** | CRITICAL HARDWARE ERROR | Device may be damaged - test with multimeter |

---

## 📊 Voltage Reference

```
XIAO ESP32-S3 IO Voltage Levels:
├─ GPIO Input High (VIH): 2.7V minimum
├─ GPIO Input Low (VIL): 0.8V maximum
├─ GPIO Output High: 3.0V typical
├─ GPIO Output Low: 0.3V typical
│
I2C Voltage Levels (3.3V system):
├─ Logic High (SDA/SCL): 2.7V - 3.3V (open-drain with pull-ups)
├─ Logic Low: 0.0V - 0.4V
├─ Pull-up provides: ~5mA @ 0V (4.7k resistor)
│
CRITICAL: Do NOT exceed 3.3V on ANY GPIO pin!
```

---

## 🎯 Final Checklist Before Power-On

- [ ] All GPIO pins connected (1, 2, 3, 4)
- [ ] I2C pull-up resistors in place (4 resistors, 4.7kΩ each)
- [ ] All I2C devices have decoupling caps (100nF)
- [ ] Common ground star verified (all GND connected)
- [ ] 3.3V rail stable (no shorts to GND)
- [ ] No 5V devices on 3.3V lines
- [ ] USB-C cable quality verified (500mA+)
- [ ] LED polarity correct (long leg to GPIO side)
- [ ] Buttons wired to GND (active-low)
- [ ] Power switch wired to 3.3V (active-high)
- [ ] No loose wires or cold solder joints
- [ ] Multimeter tests complete before powering on

---

## 📞 Quick Reference

**GPIO Pins:**
```
D0 (GPIO1)  = Status LED (PWM output)
D1 (GPIO2)  = Button 2 (input, pulls to GND)
D2 (GPIO3)  = Button 1 (input, pulls to GND)
D3 (GPIO4)  = Power Switch (input, 3.3V when ON)
```

**I2C Buses:**
```
Bus 0: GPIO5(SDA) + GPIO6(SCL) @ 100kHz  → Slave only
       D4          + D5          ⭐ STANDARD I2C PINS
Bus 1: GPIO8(SDA) + GPIO9(SCL) @ 100kHz  → Display + Navigation
       D9          + D10
```

**I2C Addresses:**
```
0x30 = ATmega328P Slave Controller
0x3C = SSD1306 OLED Display
0x27 = PCF8574 LCD Backpack (alt: 0x3F)
0x36 = Seesaw Rotary Encoder
```

**Power:**
```
USB-C: 5V @ 500mA minimum
Regulator: 3.3V @ ~400mA peak
GND: Common star topology
```

---

**✅ Ready for Hardware Assembly!**

This guide provides everything needed to correctly wire your XIAO ESP32-S3 system. Follow each section carefully, test as you go, and refer back to this document during troubleshooting.

For questions or clarifications, check the serial monitor output during boot for initialization status messages.
