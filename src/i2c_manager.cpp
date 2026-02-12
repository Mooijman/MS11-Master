#include "i2c_manager.h"
#include <Arduino.h>

// Private constructor
I2CManager::I2CManager() {
  // Initialize pointers
  // CRITICAL: Slave bus on Wire1 (I2C1), Display bus on Wire (I2C0)
  // This maps to enum: I2C_BUS_SLAVE=1 (Wire1), I2C_BUS_DISPLAY=0 (Wire)
  slaveBus = &Wire1;    // I2C1 (GPIO5/6 @ 100kHz - ATmega slave)
  displayBus = &Wire;   // I2C0 (GPIO8/9 @ 100kHz - Display devices)
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

  // Initialize Slave Bus (GPIO5/6 @ 100kHz - Wire1/I2C1 - CRITICAL)
  // ATmega328P slave controller at 0x30
  // Conservative speed for reliability
  if (!slaveBus->begin(5, 6, 100000)) {
    setError(I2C_ERROR_NOT_INIT);
    Serial.println("[I2CManager] ERROR: Failed to initialize Slave Bus (GPIO5/6)");
    vSemaphoreDelete(slaveMutex);
    vSemaphoreDelete(displayMutex);
    return false;
  }
  
  slaveBus->setTimeout(100);  // 100ms timeout for critical slave
  Serial.println("[I2CManager] ✓ Slave Bus initialized (Wire1, GPIO5/6 @ 100kHz - ATmega328P @ 0x30)");

  // Initialize Display Bus (GPIO8/9 @ 100kHz - Wire/I2C0 - NON-CRITICAL)
  // LCD (0x27), OLED (0x3C), Seesaw (0x36), and other display devices
  // Conservative speed for reliability
  if (!displayBus->begin(8, 9, 100000)) {
    setError(I2C_ERROR_NOT_INIT);
    Serial.println("[I2CManager] ERROR: Failed to initialize Display Bus (GPIO8/9)");
    slaveBus->end();
    vSemaphoreDelete(slaveMutex);
    vSemaphoreDelete(displayMutex);
    return false;
  }
  
  displayBus->setTimeout(50);  // 50ms timeout for display (non-critical)
  Serial.println("[I2CManager] ✓ Display Bus initialized (Wire, GPIO8/9 @ 100kHz - LCD/OLED/Seesaw)");

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

  Serial.printf("[I2CManager] WRITE attempt: 0x%02X, %d bytes, timeout=%dms\n", address, length, timeout_ms);

  // First try slave bus (GPIO5/6)
  Serial.println("[I2CManager] Trying slave bus (GPIO5/6)...");
  if (!acquireLock(slaveMutex, timeout_ms)) {
    Serial.println("[I2CManager] Slave bus MUTEX LOCKED");
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  slaveBus->beginTransmission(address);
  size_t written = slaveBus->write(data, length);
  uint8_t error = slaveBus->endTransmission();
  
  Serial.printf("[I2CManager] Slave bus: wrote=%d bytes, error=%d\n", written, error);

  bool success = (error == 0);
  releaseLock(slaveMutex);
  
  if (success) {
    Serial.printf("[I2CManager] ✓ WRITE to 0x%02X SUCCEEDED on slave bus\n", address);
    setError(I2C_OK);
    return true;
  }
  
  // If slave bus failed, try display bus as fallback
  Serial.println("[I2CManager] Slave bus failed, trying display bus (GPIO8/9)...");
  
  if (!acquireLock(displayMutex, timeout_ms)) {
    Serial.println("[I2CManager] Display bus MUTEX LOCKED");
    setError(I2C_ERROR_BUS_BUSY);
    return false;
  }

  displayBus->beginTransmission(address);
  written = displayBus->write(data, length);
  error = displayBus->endTransmission();

  Serial.printf("[I2CManager] Display bus: wrote=%d bytes, error=%d\n", written, error);

  success = (error == 0);
  releaseLock(displayMutex);
  
  if (success) {
    Serial.printf("[I2CManager] ✓ WRITE to 0x%02X SUCCEEDED on display bus!\n", address);
  } else {
    Serial.printf("[I2CManager] ✗ WRITE to 0x%02X FAILED on both buses\n", address);
  }
  
  if (success) {
    setError(I2C_OK);
  } else {
    setError(I2C_ERROR_NACK, error);
  }

  return success;
}

bool I2CManager::read(uint8_t address, uint8_t* buffer, uint16_t length,
                      uint16_t timeout_ms) {
  if (!initialized || !buffer || length == 0) {
    setError(I2C_ERROR_INVALID_PARAM);
    return false;
  }

  // TEMPORARY DEBUG: Try both buses for read
  // First try slave bus (GPIO5/6)
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
  }
  releaseLock(slaveMutex);
  
  // If slave bus failed, try display bus as fallback
  if (!success) {
    Serial.printf("[I2CManager] Read from 0x%02X failed on slave bus, trying display bus...\n", address);
    
    if (!acquireLock(displayMutex, timeout_ms)) {
      setError(I2C_ERROR_BUS_BUSY);
      return false;
    }

    if (displayBus->requestFrom(address, (uint8_t)length) == length) {
      for (uint16_t i = 0; i < length; i++) {
        buffer[i] = displayBus->read();
      }
      success = true;
      setError(I2C_OK);
    }
    releaseLock(displayMutex);
    
    if (success) {
      Serial.printf("[I2CManager] Read from 0x%02X succeeded on display bus!\n", address);
    }
  }
  
  if (!success) {
    setError(I2C_ERROR_NACK);
  }

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
