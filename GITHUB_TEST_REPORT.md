# Test Report: setSkipPeripheralPins() Fix

## Test Environment
- **Board:** ESP32-D0WDQ6 (revision v1.0)
- **Arduino ESP32 Core:** 3.1.1 (ESP-IDF v5.3.2)
- **GPIOViewer Version:** 1.7.1+sha.c6eca0a (latest GitHub master with fix)
- **I2C Device:** SSD1306 OLED Display (address 0x3C)
- **I2C Pins:** GPIO 4 (SCL), GPIO 5 (SDA)
- **I2C Frequency:** 100kHz

## Test Setup
```cpp
// OLED initialization (before GPIO Viewer)
SSD1306 display(0x3c, 5, 4);
display.init();

// GPIO Viewer initialization
gpio_viewer = new GPIOViewer();
gpio_viewer->setSamplingInterval(100);
gpio_viewer->setPort(5555);
gpio_viewer->setSkipPeripheralPins(true);  // Explicitly set to true
gpio_viewer->begin();
```

## Peripheral Manager Verification
Serial output confirms I2C pins are **correctly registered** with Peripheral Manager:
```
[   784][V][esp32-hal-periman.c:160] perimanSetPinBus(): Pin 5 successfully set to type I2C_MASTER_SDA (33) with bus 0x1
[   795][V][esp32-hal-periman.c:160] perimanSetPinBus(): Pin 4 successfully set to type I2C_MASTER_SCL (34) with bus 0x1
```

Post-setup verification:
```
GPIO Info:
------------------------------------------
  GPIO : BUS_TYPE[bus/unit][chan]
  --------------------------------------  
     1 : UART_TX[0]
     3 : UART_RX[0]
     4 : I2C_MASTER_SCL[0]      ← Correctly registered
     5 : I2C_MASTER_SDA[0]      ← Correctly registered
```

## Test Results: ❌ FAILED

### Issue 1: digitalRead() Still Called on I2C Pins
Despite `setSkipPeripheralPins(true)`, GPIO Viewer **still calls digitalRead()** on GPIO 4 & 5:

```
[ 52827][E][esp32-hal-gpio.c:190] __digitalRead(): IO 4 is not set as GPIO.
[ 52834][E][esp32-hal-gpio.c:190] __digitalRead(): IO 5 is not set as GPIO.
[ 53010][E][esp32-hal-gpio.c:190] __digitalRead(): IO 4 is not set as GPIO.
[ 53017][E][esp32-hal-gpio.c:190] __digitalRead(): IO 5 is not set as GPIO.
[ 53215][E][esp32-hal-gpio.c:190] __digitalRead(): IO 4 is not set as GPIO.
[ 53222][E][esp32-hal-gpio.c:190] __digitalRead(): IO 5 is not set as GPIO.
```

These errors repeat **every 100ms** (the sampling interval), confirming that:
1. The I2C pins are NOT being skipped
2. `digitalRead()` is still being called on them
3. The calls fail because pins are allocated to I2C peripheral

### Issue 2: I2C Communication Failures
**Symptom:** OLED display stops updating when GPIO Viewer is enabled
- Display shows initial content correctly
- After enabling GPIO Viewer, display freezes
- No new content is rendered
- I2C communication is disrupted by the repeated `digitalRead()` calls

### Issue 3: isPinOnPeripheralBus() Not Working
The `isPinOnPeripheralBus()` check appears to return **false** for I2C pins, despite:
- Peripheral Manager correctly showing pins as `I2C_MASTER_SDA/SCL`
- `perimanGetPinBus()` working for other bus types (LEDC detection works)

## Root Cause Analysis

The error message "**IO X is not set as GPIO**" comes from `esp32-hal-gpio.c`:
```c
int __digitalRead(uint8_t pin)
{
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return 0;
    }
    if (perimanGetPinBus(pin, ESP32_BUS_TYPE_GPIO) == NULL) {
        log_e("IO %d is not set as GPIO.", pin);  // ← This error
        return 0;
    }
    return gpio_get_level((gpio_num_t)pin);
}
```

This means:
1. GPIO Viewer IS calling `digitalRead()` on pins 4 & 5
2. The `isPinOnPeripheralBus()` check is **NOT preventing** the read
3. The check fails because pins are allocated to I2C, not GPIO bus type

## Possible Causes

### Theory 1: Check Happens After digitalRead()
The `isPinOnPeripheralBus()` check might be placed AFTER `digitalRead()` is called, making the error unavoidable.

### Theory 2: Bus Type Constants Undefined
The `#ifdef ESP32_BUS_TYPE_I2C_MASTER_SDA` guards might be failing, causing the checks to be compiled out.

### Theory 3: perimanGetPinBus() Returns NULL
For some reason, `perimanGetPinBus(pin, ESP32_BUS_TYPE_I2C_MASTER_SDA)` might return NULL even though the pin IS registered to I2C bus.

## Suggested Investigation

1. **Add debug logging** in `isPinOnPeripheralBus()` to see:
   - If the function is being called
   - What `perimanGetPinBus()` returns for each I2C bus type
   - Whether the `#ifdef` guards are active

2. **Verify check placement** in `readGPIO()`:
   - Ensure `isPinOnPeripheralBus()` is called BEFORE any `digitalRead()`
   - Check if there are multiple code paths that call `digitalRead()`

3. **Test bus type detection**:
   ```cpp
   Serial.printf("Pin 4 I2C_SCL check: %p\n", perimanGetPinBus(4, ESP32_BUS_TYPE_I2C_MASTER_SCL));
   Serial.printf("Pin 5 I2C_SDA check: %p\n", perimanGetPinBus(5, ESP32_BUS_TYPE_I2C_MASTER_SDA));
   ```

## Conclusion

The `setSkipPeripheralPins()` feature **does not work as intended**. I2C pins are still being sampled despite:
- Correct Peripheral Manager registration
- Explicit `setSkipPeripheralPins(true)` call
- I2C pins showing in GPIO info dump

The I2C bus interference issue remains unresolved. Users must continue to disable GPIO Viewer when using I2C/SPI peripherals.

## Workaround

For now, the only reliable workaround is to **disable GPIO Viewer** when I2C/SPI devices are in use:
```cpp
// Do NOT enable GPIO Viewer if using I2C/SPI
if (gpioViewerEnabled == "true" || gpioViewerEnabled == "on") {
    // GPIO Viewer disabled due to I2C interference
    Serial.println("GPIO Viewer disabled - I2C device in use");
}
```

## Next Steps

Would you like me to:
1. Add detailed debug logging to identify where the check is failing?
2. Try a different approach (e.g., catching the error at `__digitalRead()` level)?
3. Implement manual pin exclusion as a fallback?

Thank you for the quick implementation attempt! The feature is much needed, but needs some debugging to make it work correctly.
