#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============================================================================
// I2C BUS CONFIGURATION (PINS SWAPPED FOR COMPATIBILITY)
// ============================================================================
// XIAO ESP32-S3 Pinout:
//
// Bus 0 (Display Bus - Wire/I2C0):
//   SDA: GPIO8 (D9) - 100kHz (conservative, reliable)
//   SCL: GPIO9 (D10)
//   Devices: LCD 16x2 (0x27), OLED Display (0x3C), Seesaw (0x36)
//   Note: LiquidCrystal_I2C only supports Wire, so all display on Bus 0
//
// Bus 1 (Slave Bus - Wire1/I2C1 - CRITICAL):
//   SDA: GPIO5 (D4) - 100kHz (conservative, reliable)
//   SCL: GPIO6 (D5)
//   Devices: Slave Controller (ATmega328P @ 0x30)
//
// Wiring Diagram:
// ┌─────────────────┬─────────────────┐
// │  XIAO ESP32-S3  │     Device      │
// ├─────────────────┼─────────────────┤
// │ GPIO8 (D9)   ---|─ SDA (Bus 0)    │ Pull-ups to 3.3V
// │ GPIO9 (D10)  ---|─ SCL (Bus 0)    │ (4.7kΩ recommended)
// │ (LCD,OLED,Seesaw)                 │
// ├─────────────────┼─────────────────┤
// │ GPIO5 (D4)   ---|─ SDA (Bus 1)    │ Pull-ups to 3.3V
// │ GPIO6 (D5)   ---|─ SCL (Bus 1)    │ (4.7kΩ recommended)
// │ (ATmega slave)                     │
// ├─────────────────┼─────────────────┤
// │ GND           --|─ GND (common)   │ Common ground
// │ 3V3           --|─ VCC (all)      │ Common power
// └─────────────────┴─────────────────┘

// Bus enumeration
// Note: After pin swap, Bus 0 (Wire) = GPIO8/9 (Display), Bus 1 (Wire1) = GPIO5/6 (Slave)
enum I2CBus {
  I2C_BUS_DISPLAY = 0,  // GPIO8/9 @ 100kHz - Display devices (Wire/I2C0)
  I2C_BUS_SLAVE = 1     // GPIO5/6 @ 100kHz - Critical slave controller (Wire1/I2C1)
};

// I2C Error codes
enum I2CErrorCode {
  I2C_OK = 0,
  I2C_ERROR_TIMEOUT = 1,
  I2C_ERROR_NACK = 2,      // NACK from slave
  I2C_ERROR_BUS_BUSY = 3,
  I2C_ERROR_NOT_INIT = 4,
  I2C_ERROR_INVALID_PARAM = 5,
  I2C_ERROR_UNKNOWN = 255
};

class I2CManager {
public:
  // Singleton pattern
  static I2CManager& getInstance() {
    static I2CManager instance;
    return instance;
  }

  // Lifecycle
  bool begin();  // Initialize both I2C buses
  void end();    // Shutdown buses

  // ========================================================================
  // Transactional API (Slave Bus - 100kHz, Critical)
  // ========================================================================
  
  // Write to slave register with timeout & retry
  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value, 
                     uint16_t timeout_ms = 100, uint8_t retries = 2);
  
  // Read from slave register
  bool readRegister(uint8_t address, uint8_t reg, uint8_t& value, 
                    uint16_t timeout_ms = 100, uint8_t retries = 2);
  
  // Read multi-byte register (e.g., int16_t)
  bool readRegisterMulti(uint8_t address, uint8_t reg, uint8_t* buffer, 
                         uint16_t length, uint16_t timeout_ms = 100);
  
  // Generic write (raw bytes)
  bool write(uint8_t address, const uint8_t* data, uint16_t length, 
             uint16_t timeout_ms = 100);
  
  // Generic read
  bool read(uint8_t address, uint8_t* buffer, uint16_t length, 
            uint16_t timeout_ms = 100);

  // ========================================================================
  // Display Bus API (100kHz, Non-critical)
  // ========================================================================
  
  // Display write (no retry, fail-safe)
  bool displayWrite(uint8_t address, const uint8_t* data, uint16_t length, 
                    uint16_t timeout_ms = 50);
  
  // Display read
  bool displayRead(uint8_t address, uint8_t* buffer, uint16_t length, 
                   uint16_t timeout_ms = 50);

  // ========================================================================
  // Diagnostics
  // ========================================================================
  
  // Scan bus for devices (uses slave bus)
  bool scanBus(uint8_t* foundAddresses, uint8_t maxDevices, uint8_t& count);
  
  // Quick ping test (for connection health)
  bool ping(uint8_t address, I2CBus bus = I2C_BUS_SLAVE);
  
  // Get last error
  I2CErrorCode getLastErrorCode() { return lastErrorCode; }
  String getLastError() { return lastErrorMsg; }
  
  // Status checks
  bool isInitialized() { return initialized; }
  bool isSlaveBusHealthy();  // Check slave bus status
  bool isDisplayBusHealthy(); // Check display bus status

private:
  I2CManager();  // Private constructor (singleton)
  ~I2CManager();
  
  // Delete copy/move constructors
  I2CManager(const I2CManager&) = delete;
  I2CManager& operator=(const I2CManager&) = delete;
  I2CManager(I2CManager&&) = delete;
  I2CManager& operator=(I2CManager&&) = delete;

  // State
  bool initialized = false;
  TwoWire* slaveBus = nullptr;    // I2C0: GPIO5/6 @ 100kHz
  TwoWire* displayBus = nullptr;  // I2C1: GPIO8/9 @ 100kHz
  
  // Thread-safety (one mutex per bus)
  SemaphoreHandle_t slaveMutex = nullptr;
  SemaphoreHandle_t displayMutex = nullptr;
  
  // Error tracking
  I2CErrorCode lastErrorCode = I2C_OK;
  String lastErrorMsg;
  
  // Helper: Lock acquisition with timeout
  bool acquireLock(SemaphoreHandle_t mutex, uint32_t timeout_ms);
  void releaseLock(SemaphoreHandle_t mutex);
  
  // Helper: Parse Wire error codes
  void setError(I2CErrorCode code, uint8_t wireError = 0);
  
  // Helper: Perform write with timeout
  bool performWrite(TwoWire* wire, uint8_t address, const uint8_t* data, 
                    uint16_t length, uint16_t timeout_ms);
};

#endif // I2C_MANAGER_H
