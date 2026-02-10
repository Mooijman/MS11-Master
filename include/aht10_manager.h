#ifndef AHT10_MANAGER_H
#define AHT10_MANAGER_H

#include <Arduino.h>
#include "i2c_manager.h"
#include "config.h"

/**
 * AHT10 Temperature & Humidity Sensor Manager
 * 
 * Singleton pattern for consistent sensor access
 * Uses I2C Bus 1 (GPIO8/9 @ 100kHz, non-critical)
 * 
 * Hardware: Adafruit AHT10 Temperature & Humidity Sensor
 * I2C Address: 0x38 (fixed, not configurable)
 * Measurement range: Temp -40°C to +85°C, Humidity 0-100% RH
 * 
 * Features:
 * - Non-blocking temperature and humidity reading
 * - Error handling and last valid values on read failure
 * - Health check via I2C
 */

class AHT10Manager {
public:
  // Singleton instance accessor
  static AHT10Manager& getInstance() {
    static AHT10Manager instance;
    return instance;
  }

  // Initialization and shutdown
  bool begin();
  void end();

  // Sensor readings
  bool readSensor();              // Read sensor (non-blocking, returns success)
  float getTemperature();         // Get last temperature reading (°C)
  float getHumidity();            // Get last humidity reading (%)
  uint32_t getLastReadTime();     // Get timestamp of last successful read

  // Status
  bool isInitialized() { return initialized; }
  bool isHealthy();               // Check if sensor responds to I2C
  String getLastError() { return lastError; }

private:
  AHT10Manager();  // Private constructor
  ~AHT10Manager();
  
  // Delete copy constructors
  AHT10Manager(const AHT10Manager&) = delete;
  AHT10Manager& operator=(const AHT10Manager&) = delete;

  bool initialized = false;
  float temperature = 0.0f;
  float humidity = 0.0f;
  uint32_t lastReadTime = 0;
  String lastError;

  // I2C address
  static constexpr uint8_t AHT10_I2C_ADDRESS = 0x38;
};

#endif
