#include "i2c_manager.h"
#include <Arduino.h>

// Private constructor
I2CManager::I2CManager() {
  // Initialize pointers
  slaveBus = &Wire;     // I2C0 (default for ESP32)
  displayBus = &Wire1;  // I2C1 (second I2C interface)
}

I2CManager::~I2CManager() {
  end();
}

bool I2CManager::begin() {
  if (initialized) {
    return true;  // Already initialized
  }

  // Create mutexes for thread safety
  slaveMutex = xSemaphoreCreateMutex();
  displayMutex = xSemaphoreCreateMutex();
  
  if (!slaveMutex || !displayMutex) {
    setError(I2C_ERROR_UNKNOWN);
    Serial.println("[I2CManager] ERROR: Failed to create mutexes");
    return false;
  }

  // Initialize Slave Bus (GPIO8/9 @ 100kHz) - Now used for LCD via LiquidCrystal_I2C
  // Wire (I2C0) uses GPIO8/9 because LiquidCrystal_I2C doesn't support Wire1
  // Conservative speed, same as display bus for reliability
  if (!slaveBus->begin(8, 9, 100000)) {
    setError(I2C_ERROR_NOT_INIT);
    Serial.println("[I2CManager] ERROR: Failed to initialize Slave Bus (GPIO8/9)");
    vSemaphoreDelete(slaveMutex);
    vSemaphoreDelete(displayMutex);
    return false;
  }
  
  slaveBus->setTimeout(100);  // 100ms timeout per transaction
  Serial.println("[I2CManager] ✓ Slave Bus initialized (GPIO8/9 @ 100kHz - LCD via Wire)");

  // Initialize Display Bus (GPIO5/6 @ 100kHz) - ATmega slave and other I2C_TWO devices
  // Wire1 (I2C1) uses GPIO5/6 for devices that support I2C_TWO selection
  // Non-critical: Conservative speed for reliability
  if (!displayBus->begin(5, 6, 100000)) {
    setError(I2C_ERROR_NOT_INIT);
    Serial.println("[I2CManager] ERROR: Failed to initialize Display Bus (GPIO5/6)");
    slaveBus->end();
    vSemaphoreDelete(slaveMutex);
    vSemaphoreDelete(displayMutex);
    return false;
  }
  
  displayBus->setTimeout(50);  // 50ms timeout for display
  Serial.println("[I2CManager] ✓ Display Bus initialized (GPIO5/6 @ 100kHz - Wire1)");

  initialized = true;
  setError(I2C_OK);
  return true;
}

void I2CManager::end() {
  if (!initialized) {
    return;
  }

  // Stop both buses
  if (slaveBus) {
    slaveBus->end();
  }
  if (displayBus) {
    displayBus->end();
  }

  // Delete mutexes
  if (slaveMutex) {
    vSemaphoreDelete(slaveMutex);
    slaveMutex = nullptr;
  }
  if (displayMutex) {
    vSemaphoreDelete(displayMutex);
    displayMutex = nullptr;
  }

  initialized = false;
  Serial.println("[I2CManager] Buses shutdown");
}

// Helper: Acquire mutex with timeout
bool I2CManager::acquireLock(SemaphoreHandle_t mutex, uint32_t timeout_ms) {
  if (!mutex) {
    return false;
  }
  
  TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(mutex, ticks) == pdTRUE;
}

// Helper: Release mutex
void I2CManager::releaseLock(SemaphoreHandle_t mutex) {
  if (mutex) {
    xSemaphoreGive(mutex);
  }
}

// Helper: Set error with detail
void I2CManager::setError(I2CErrorCode code, uint8_t wireError) {
  lastErrorCode = code;
  
  switch (code) {
    case I2C_OK:
      lastErrorMsg = "OK";
      break;
    case I2C_ERROR_TIMEOUT:
      lastErrorMsg = "Timeout";
      break;
    case I2C_ERROR_NACK:
      lastErrorMsg = "NACK (device not responding)";
      break;
    case I2C_ERROR_BUS_BUSY:
      lastErrorMsg = "Bus busy";
      break;
    case I2C_ERROR_NOT_INIT:
      lastErrorMsg = "Not initialized";
      break;
    case I2C_ERROR_INVALID_PARAM:
      lastErrorMsg = "Invalid parameter";
      break;
    default:
      lastErrorMsg = "Unknown error";
  }
  
  if (wireError) {
    lastErrorMsg += " (Wire error: " + String(wireError) + ")";
  }
}

// ============================================================================
// Slave Bus Operations (Critical, 100kHz)
// ============================================================================

bool I2CManager::writeRegister(uint8_t address, uint8_t reg, uint8_t value,
                                uint16_t timeout_ms, uint8_t retries) {
  if (!initialized) {
    setError(I2C_ERROR_NOT_INIT);
    return false;
  }

  if (!acquireLock(slaveMutex, timeout_ms)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  uint8_t data[2] = {reg, value};
  bool success = false;
  uint8_t attempt = 0;

  while (attempt <= retries) {
    uint8_t error = 0;
    
    slaveBus->beginTransmission(address);
    slaveBus->write(data, 2);
    error = slaveBus->endTransmission();

    if (error == 0) {
      success = true;
      setError(I2C_OK);
      break;
    }

    if (error == 4) {
      setError(I2C_ERROR_UNKNOWN, error);
    } else if (error == 2) {
      setError(I2C_ERROR_NACK, error);
    } else {
      setError(I2C_ERROR_UNKNOWN, error);
    }

    attempt++;
    if (attempt <= retries) {
      delay(10);  // Small delay before retry
    }
  }

  releaseLock(slaveMutex);
  return success;
}

bool I2CManager::readRegister(uint8_t address, uint8_t reg, uint8_t& value,
                               uint16_t timeout_ms, uint8_t retries) {
  if (!initialized) {
    setError(I2C_ERROR_NOT_INIT);
    return false;
  }

  if (!acquireLock(slaveMutex, timeout_ms)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  bool success = false;
  uint8_t attempt = 0;

  while (attempt <= retries) {
    uint8_t error = 0;

    // Write register address
    slaveBus->beginTransmission(address);
    slaveBus->write(reg);
    error = slaveBus->endTransmission();

    if (error == 0) {
      // Read value
      if (slaveBus->requestFrom(address, (uint8_t)1) == 1) {
        value = slaveBus->read();
        success = true;
        setError(I2C_OK);
        break;
      }
    }

    if (!success) {
      setError(I2C_ERROR_NACK);
    }

    attempt++;
    if (attempt <= retries) {
      delay(10);
    }
  }

  releaseLock(slaveMutex);
  return success;
}

bool I2CManager::readRegisterMulti(uint8_t address, uint8_t reg, uint8_t* buffer,
                                    uint16_t length, uint16_t timeout_ms) {
  if (!initialized || !buffer || length == 0) {
    setError(I2C_ERROR_INVALID_PARAM);
    return false;
  }

  if (!acquireLock(slaveMutex, timeout_ms)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  bool success = false;

  // Write register address
  slaveBus->beginTransmission(address);
  slaveBus->write(reg);
  if (slaveBus->endTransmission() == 0) {
    // Read bytes
    if (slaveBus->requestFrom(address, (uint8_t)length) == length) {
      for (uint16_t i = 0; i < length; i++) {
        buffer[i] = slaveBus->read();
      }
      success = true;
      setError(I2C_OK);
    }
  }

  if (!success) {
    setError(I2C_ERROR_NACK);
  }

  releaseLock(slaveMutex);
  return success;
}

bool I2CManager::write(uint8_t address, const uint8_t* data, uint16_t length,
                       uint16_t timeout_ms) {
  if (!initialized || !data || length == 0) {
    setError(I2C_ERROR_INVALID_PARAM);
    return false;
  }

  if (!acquireLock(slaveMutex, timeout_ms)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  slaveBus->beginTransmission(address);
  slaveBus->write(data, length);
  uint8_t error = slaveBus->endTransmission();

  bool success = (error == 0);
  if (success) {
    setError(I2C_OK);
  } else {
    setError(I2C_ERROR_NACK, error);
  }

  releaseLock(slaveMutex);
  return success;
}

bool I2CManager::read(uint8_t address, uint8_t* buffer, uint16_t length,
                      uint16_t timeout_ms) {
  if (!initialized || !buffer || length == 0) {
    setError(I2C_ERROR_INVALID_PARAM);
    return false;
  }

  if (!acquireLock(slaveMutex, timeout_ms)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  bool success = false;
  if (slaveBus->requestFrom(address, (uint8_t)length) == length) {
    for (uint16_t i = 0; i < length; i++) {
      buffer[i] = slaveBus->read();
    }
    success = true;
    setError(I2C_OK);
  } else {
    setError(I2C_ERROR_NACK);
  }

  releaseLock(slaveMutex);
  return success;
}

// ============================================================================
// Display Bus Operations (Non-critical, 100kHz)
// ============================================================================

bool I2CManager::displayWrite(uint8_t address, const uint8_t* data,
                               uint16_t length, uint16_t timeout_ms) {
  if (!initialized || !data || length == 0) {
    return false;  // Fail silently for display
  }

  if (!acquireLock(displayMutex, timeout_ms)) {
    return false;  // Fail silently for display
  }

  displayBus->beginTransmission(address);
  displayBus->write(data, length);
  uint8_t error = displayBus->endTransmission();

  bool success = (error == 0);
  releaseLock(displayMutex);
  return success;
}

bool I2CManager::displayRead(uint8_t address, uint8_t* buffer,
                              uint16_t length, uint16_t timeout_ms) {
  if (!initialized || !buffer || length == 0) {
    return false;
  }

  if (!acquireLock(displayMutex, timeout_ms)) {
    return false;
  }

  bool success = false;
  if (displayBus->requestFrom(address, (uint8_t)length) == length) {
    for (uint16_t i = 0; i < length; i++) {
      buffer[i] = displayBus->read();
    }
    success = true;
  }

  releaseLock(displayMutex);
  return success;
}

// ============================================================================
// Diagnostics & Health
// ============================================================================

bool I2CManager::scanBus(uint8_t* foundAddresses, uint8_t maxDevices, uint8_t& count) {
  if (!initialized || !foundAddresses) {
    setError(I2C_ERROR_INVALID_PARAM);
    return false;
  }

  if (!acquireLock(slaveMutex, 1000)) {
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  count = 0;
  Serial.println("[I2CManager] Scanning bus...");

  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    slaveBus->beginTransmission(addr);
    uint8_t error = slaveBus->endTransmission();

    if (error == 0) {
      if (count < maxDevices) {
        foundAddresses[count] = addr;
        Serial.printf("[I2CManager] Found device at 0x%02X\n", addr);
      }
      count++;
    }
  }

  releaseLock(slaveMutex);
  setError(I2C_OK);
  return true;
}

bool I2CManager::ping(uint8_t address, I2CBus bus) {
  if (!initialized) {
    return false;
  }

  TwoWire* wire = (bus == I2C_BUS_SLAVE) ? slaveBus : displayBus;
  SemaphoreHandle_t mutex = (bus == I2C_BUS_SLAVE) ? slaveMutex : displayMutex;

  if (!acquireLock(mutex, 100)) {
    return false;
  }

  wire->beginTransmission(address);
  uint8_t error = wire->endTransmission();

  releaseLock(mutex);
  return (error == 0);
}

bool I2CManager::isSlaveBusHealthy() {
  return ping(0x30, I2C_BUS_SLAVE);  // Ping slave controller
}

bool I2CManager::isDisplayBusHealthy() {
  return ping(0x3C, I2C_BUS_DISPLAY);  // Ping OLED display
}
