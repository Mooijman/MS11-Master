#ifndef SLAVE_CONTROLLER_H
#define SLAVE_CONTROLLER_H

#include <Arduino.h>
#include "i2c_manager.h"

// ============================================================================
// MS11 SLAVE CONTROLLER (ATmega328P @ 0x30)
// I2C Protocol v2 - Register Map
// ============================================================================

#define SLAVE_I2C_ADDR 0x30

// Read Registers (Master → Slave)
#define REG_OVEN_TEMP_H      0x00  // int16_t high byte
#define REG_OVEN_TEMP_L      0x01  // int16_t low byte
#define REG_SYS_TEMP_H       0x02  // int16_t high byte
#define REG_SYS_TEMP_L       0x03  // int16_t low byte
#define REG_FAN_SPEED        0x06  // uint8_t PWM (0-39)
#define REG_STATUS           0x07  // uint8_t status byte
#define REG_FW_VERSION       0x08  // uint8_t BCD (0x61 = v0.6.1)
#define REG_PROTOCOL_VER     0x09  // uint8_t (0x02 = v2)
#define REG_DEBUG_MODE       0x0A  // uint8_t (0/1)
#define REG_FAN_PERCENT      0x0B  // uint8_t (0-100)
#define REG_LAST_ACK         0x0C  // uint8_t (0=fail, 1=success)
#define REG_MIN_MASTER_VER   0x0D  // uint8_t (0x10 = v1.0)
#define REG_DISPLAY_ENABLED  0x0E  // uint8_t (0/1)

// Configuration Registers
#define REG_OVEN_TEMP_LIMIT_L_H  0x0F  // uint16_t low byte
#define REG_OVEN_TEMP_LIMIT_L_L  0x10  // uint16_t low byte
#define REG_OVEN_TEMP_LIMIT_H_H  0x11  // uint16_t high byte
#define REG_OVEN_TEMP_LIMIT_H_L  0x12  // uint16_t low byte
#define REG_IGNITER_MAX_TIME_H   0x13  // uint16_t high byte
#define REG_IGNITER_MAX_TIME_L   0x14  // uint16_t low byte
#define REG_SYS_TEMP_ALARM       0x15  // uint8_t threshold (°C)

// Write Registers (Master ← Slave)
#define REG_FAN_CMD          0x20  // uint8_t (0-100 percent)
#define REG_IGNITER_CMD      0x21  // uint8_t (0=off, 1=on)
#define REG_AUGER_CMD        0x22  // uint8_t (0=off, 1=on)
#define REG_DEBUG_CMD        0x23  // uint8_t toggle
#define REG_SELFTEST_CMD     0x24  // uint8_t trigger
#define REG_OVEN_TEMP_CMD_H  0x25  // Config write
#define REG_OVEN_TEMP_CMD_L  0x26
#define REG_IGNITER_CMD_H    0x27
#define REG_IGNITER_CMD_L    0x28
#define REG_SYS_TEMP_ALARM_CMD 0x29

// LED control registers (I2C test mode)
#define SLAVE_REG_LED_ONOFF  0x10  // 0=off, 1=on
#define SLAVE_REG_LED_BLINK  0x11  // 0=off, 1=1Hz, 2=4Hz

// Status byte bits
#define STATUS_IGNITER_BIT   0x01
#define STATUS_AUGER_BIT     0x02
#define STATUS_ERROR_MASK    0xF0
#define STATUS_ERROR_SHIFT   4

class SlaveController {
public:
  // Singleton
  static SlaveController& getInstance() {
    static SlaveController instance;
    return instance;
  }

  // Initialize I2C communication
  bool begin();

  // ========================================================================
  // Temperature Reading (Critical)
  // ========================================================================
  
  // Read oven temperature (°C) - cached reading
  bool readOvenTemp(int16_t& temp_c);
  
  // Read system temperature (°C) - cached reading
  bool readSystemTemp(int16_t& temp_c);
  
  // Force immediate refresh from slave
  bool refreshTemperatures();

  // ========================================================================
  // Fan Control
  // ========================================================================
  
  // Set fan speed by percentage (0-100)
  // PWM = round(percent × 39 / 100)
  bool setFanPercent(uint8_t percent);
  
  // Get last commanded percentage
  uint8_t getFanPercent();
  
  // Get actual PWM value (0-39)
  bool getFanSpeed(uint8_t& pwm);

  // ========================================================================
  // Control Systems
  // ========================================================================
  
  // Igniter control
  bool setIgniter(bool on);
  bool isIgniterOn();
  
  // Auger control
  bool setAuger(bool on);
  bool isAugerOn();

  // ========================================================================
  // Status & Health
  // ========================================================================
  
  // Read status byte (bit 0=igniter, bit 1=auger, bits 4-7=error code)
  bool readStatus(uint8_t& statusByte);
  
  // Get error code from status (bits 4-7)
  uint8_t getErrorCode();
  
  // Get firmware version (BCD format)
  uint8_t getFirmwareVersion();
  
  // Get protocol version (should be 0x02)
  uint8_t getProtocolVersion();
  
  // Quick ping (connection test)
  bool ping();
  
  // Check if slave is responding and healthy
  bool isHealthy();

  // ========================================================================
  // Diagnostics
  // ========================================================================
  
  // Run slave self-test
  bool runSelfTest();
  
  // Direct LED control via I2C
  bool setLed(bool on);  // Turn LED on/off
  bool pulseLed(uint16_t durationMs);  // Start a non-blocking pulse
  
  // Get last error message
  String getLastError() { return lastError; }
  
  // Get connection statistics
  struct Stats {
    uint32_t successfulReads = 0;
    uint32_t failedReads = 0;
    uint32_t successfulWrites = 0;
    uint32_t failedWrites = 0;
  };
  
  Stats getStats() { return stats; }
  void resetStats();

private:
  SlaveController() = default;
  ~SlaveController() = default;
  
  // Delete copy constructors
  SlaveController(const SlaveController&) = delete;
  SlaveController& operator=(const SlaveController&) = delete;

  // State
  String lastError;
  uint8_t lastStatus = 0;
  uint8_t lastFwVersion = 0;
  uint8_t lastProtoVersion = 0;
  uint8_t lastFanPercent = 0;
  bool lastIgniterState = false;
  bool lastAugerState = false;
  
  // Cached temperature readings
  int16_t cachedOvenTemp = 0;
  int16_t cachedSystemTemp = 0;
  unsigned long lastTempReadTime = 0;
  
  // Statistics
  Stats stats;
  
  // Helpers
  bool readRegisterInt16(uint8_t regHigh, uint8_t regLow, int16_t& value);
  bool writeRegisterInt16(uint8_t regHigh, uint8_t regLow, int16_t value);
};

#endif // SLAVE_CONTROLLER_H
