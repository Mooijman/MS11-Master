# Feature Request: Skip I2C/SPI pins to prevent bus interference

## Problem Description
When GPIO Viewer monitors pins that are actively used by I2C or SPI peripherals, the continuous `digitalRead()` calls interfere with bus communication, causing data corruption and communication failures.

## Current Behavior
- GPIO Viewer calls `digitalRead()` on ALL pins every sampling interval
- I2C and SPI pins are NOT excluded from monitoring
- This causes bus timing issues and protocol violations
- The library already has a comment acknowledging this issue (line 835): `// Pin is configured as OUTPUT (e.g., SPI may have taken over), skip it`

## Real-World Impact
I'm using an SSD1306 OLED display on I2C pins (GPIO 4=SCL, GPIO 5=SDA). When GPIO Viewer is enabled, the OLED shows corrupted data or stops responding due to bus interference from the monitoring reads every 100ms.

This affects:
- ❌ I2C displays (OLED, LCD)
- ❌ I2C sensors (BME280, MPU6050, etc.)
- ❌ SPI devices (SD cards, TFT displays, etc.)
- ❌ UART communication
- ❌ Any timing-sensitive peripheral protocol

## Proposed Solution
Use ESP32's Peripheral Manager API (`perimanGetPinBus()`) to detect and skip pins that are allocated to I2C, SPI, UART, or other peripherals.

The library already uses `perimanGetPinBus()` for LEDC detection (line 773), so this implementation is consistent with existing code patterns.

### Implementation Suggestion

Add this check in the `readGPIO()` function, **before** calling `digitalRead()`:

```cpp
#ifdef GPIOVIEWER_ESP32CORE_VERSION_3
    // Check if pin is used by I2C
    if (perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_I2C_MASTER_SDA) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_I2C_MASTER_SCL) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_I2C_SLAVE_SDA) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_I2C_SLAVE_SCL) != NULL) {
        // Skip I2C pins to prevent bus interference
        *pintype = digitalPin;
        *originalValue = 0;
        return 0;
    }
    
    // Check if pin is used by SPI
    if (perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_SPI_MASTER_SCK) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_SPI_MASTER_MISO) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_SPI_MASTER_MOSI) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_SPI_MASTER_SS) != NULL) {
        // Skip SPI pins to prevent bus interference
        *pintype = digitalPin;
        *originalValue = 0;
        return 0;
    }
    
    // Optionally: Check UART pins
    if (perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_UART_RX) != NULL ||
        perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_UART_TX) != NULL) {
        // Skip UART pins
        *pintype = digitalPin;
        *originalValue = 0;
        return 0;
    }
#endif
```

**Location in code:** Insert before line 938 (ESP32 Core 3.x version) where `digitalRead(gpioNum)` is called.

## Benefits
- ✅ Prevents bus interference and communication errors
- ✅ Uses existing Arduino Core 3.x API (`perimanGetPinBus()`)
- ✅ Consistent with existing LEDC detection pattern (already in library)
- ✅ Automatically detects peripheral allocation - no user configuration needed
- ✅ Backward compatible (wrapped in `#ifdef GPIOVIEWER_ESP32CORE_VERSION_3`)
- ✅ Zero overhead when peripherals are not in use
- ✅ Solves issues mentioned in existing code comments about SPI/OUTPUT pins

## Alternative Solution (if above is too broad)
Add a configuration method for manual pin exclusion:
```cpp
gpio_viewer.excludePins({4, 5});  // Manually exclude I2C pins
```

However, automatic detection is preferred as it:
- Requires no user action or knowledge
- Works for all peripheral types
- Updates automatically if pin assignments change

## Technical References
- **ESP32 Peripheral Manager API:** `esp32-hal-periman.h`
- **Available bus types:** 
  - `ESP32_BUS_TYPE_I2C_MASTER_SDA`, `ESP32_BUS_TYPE_I2C_MASTER_SCL`
  - `ESP32_BUS_TYPE_SPI_MASTER_SCK`, `ESP32_BUS_TYPE_SPI_MASTER_MISO`, etc.
  - `ESP32_BUS_TYPE_UART_RX`, `ESP32_BUS_TYPE_UART_TX`
- **Existing usage in library:** Line 773 already uses `perimanGetPinBus(gpioNum, ESP32_BUS_TYPE_LEDC)`

## Why This Matters
GPIO Viewer is an excellent debugging tool, but currently it can make projects **less reliable** when I2C/SPI peripherals are in use. This fix would make it a **non-intrusive** debugging tool that users can safely leave enabled during development without worrying about breaking their peripheral communication.

## Environment
- Arduino ESP32 Core: 3.x
- GPIOViewer: 1.7.1
- Board: ESP32 DevKit
- Affected Peripherals: SSD1306 OLED (I2C on GPIO 4 & 5)

## Workaround
Currently, users must disable GPIO Viewer when using I2C/SPI peripherals, which defeats the purpose of continuous monitoring during development.

---

Thank you for this excellent library! This enhancement would make it even more valuable for the ESP32 community.
