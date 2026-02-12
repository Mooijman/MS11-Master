#include "slave_controller.h"

bool SlaveController::begin() {
  // Initialize I2C manager if not already done
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "Failed to initialize I2C manager";
      return false;
    }
  }

  // Test connection to slave
  if (!ping()) {
    lastError = "Slave controller not responding at 0x30";
    Serial.println("[SlaveController] ERROR: " + lastError);
    return false;
  }

  // Get protocol version (should be 0x02)
  if (!I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, REG_PROTOCOL_VER, lastProtoVersion)) {
    lastError = "Could not read protocol version";
    Serial.println("[SlaveController] WARNING: " + lastError);
  } else {
    if (lastProtoVersion != 0x02) {
      Serial.printf("[SlaveController] WARNING: Protocol mismatch! Expected 0x02, got 0x%02X\n", lastProtoVersion);
    } else {
      Serial.println("[SlaveController] ✓ Protocol v2 confirmed");
    }
  }

  // Get firmware version
  if (I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, REG_FW_VERSION, lastFwVersion)) {
    Serial.printf("[SlaveController] ✓ Slave firmware v%X.%X.%X\n",
                  (lastFwVersion >> 4) & 0xF,
                  (lastFwVersion >> 2) & 0x3,
                  lastFwVersion & 0x3);
  }

  Serial.println("[SlaveController] ✓ Connected and ready");
  return true;
}

bool SlaveController::readOvenTemp(int16_t& temp_c) {
  if (!readRegisterInt16(REG_OVEN_TEMP_H, REG_OVEN_TEMP_L, cachedOvenTemp)) {
    stats.failedReads++;
    lastError = "Failed to read oven temperature";
    return false;
  }

  temp_c = cachedOvenTemp;
  stats.successfulReads++;
  lastTempReadTime = millis();
  return true;
}

bool SlaveController::readSystemTemp(int16_t& temp_c) {
  if (!readRegisterInt16(REG_SYS_TEMP_H, REG_SYS_TEMP_L, cachedSystemTemp)) {
    stats.failedReads++;
    lastError = "Failed to read system temperature";
    return false;
  }

  temp_c = cachedSystemTemp;
  stats.successfulReads++;
  return true;
}

bool SlaveController::refreshTemperatures() {
  int16_t dummy;
  return readOvenTemp(dummy) && readSystemTemp(dummy);
}

bool SlaveController::setFanPercent(uint8_t percent) {
  if (percent > 100) {
    lastError = "Fan percent out of range (0-100)";
    return false;
  }

  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, REG_FAN_CMD, percent)) {
    stats.failedWrites++;
    lastError = I2CManager::getInstance().getLastError();
    return false;
  }

  lastFanPercent = percent;
  stats.successfulWrites++;
  return true;
}

uint8_t SlaveController::getFanPercent() {
  return lastFanPercent;
}

bool SlaveController::getFanSpeed(uint8_t& pwm) {
  if (!I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, REG_FAN_SPEED, pwm)) {
    lastError = I2CManager::getInstance().getLastError();
    return false;
  }
  return (pwm <= 39);  // Valid range check
}

bool SlaveController::setIgniter(bool on) {
  uint8_t value = on ? 0x01 : 0x00;
  
  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, REG_IGNITER_CMD, value)) {
    stats.failedWrites++;
    lastError = I2CManager::getInstance().getLastError();
    return false;
  }

  lastIgniterState = on;
  stats.successfulWrites++;
  return true;
}

bool SlaveController::isIgniterOn() {
  return lastIgniterState;
}

bool SlaveController::setAuger(bool on) {
  uint8_t value = on ? 0x01 : 0x00;
  
  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, REG_AUGER_CMD, value)) {
    stats.failedWrites++;
    lastError = I2CManager::getInstance().getLastError();
    return false;
  }

  lastAugerState = on;
  stats.successfulWrites++;
  return true;
}

bool SlaveController::isAugerOn() {
  return lastAugerState;
}

bool SlaveController::readStatus(uint8_t& statusByte) {
  if (!I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, REG_STATUS, statusByte)) {
    lastError = I2CManager::getInstance().getLastError();
    return false;
  }

  lastStatus = statusByte;
  
  // Update cached states from status byte
  lastIgniterState = (statusByte & STATUS_IGNITER_BIT) != 0;
  lastAugerState = (statusByte & STATUS_AUGER_BIT) != 0;
  
  return true;
}

uint8_t SlaveController::getErrorCode() {
  return (lastStatus >> STATUS_ERROR_SHIFT) & 0x0F;
}

uint8_t SlaveController::getFirmwareVersion() {
  return lastFwVersion;
}

uint8_t SlaveController::getProtocolVersion() {
  return lastProtoVersion;
}

bool SlaveController::ping() {
  return I2CManager::getInstance().ping(SLAVE_I2C_ADDR, I2C_BUS_SLAVE);
}

bool SlaveController::isHealthy() {
  // Quick health check: ping + read status
  if (!ping()) {
    return false;
  }

  uint8_t status;
  if (!readStatus(status)) {
    return false;
  }

  // Check error code in status byte
  uint8_t errorCode = getErrorCode();
  return (errorCode == 0);  // 0 = OK
}

bool SlaveController::runSelfTest() {
  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, REG_SELFTEST_CMD, 0x01)) {
    lastError = "Failed to send selftest command";
    return false;
  }

  // Wait for test to complete (~500ms)
  delay(500);

  // Read status to check result
  uint8_t status;
  if (!readStatus(status)) {
    lastError = "Failed to read selftest result";
    return false;
  }

  uint8_t errorCode = getErrorCode();
  if (errorCode != 0) {
    lastError = "Selftest failed with error code: " + String(errorCode);
    return false;
  }

  return true;
}

bool SlaveController::setLed(bool on) {
  uint8_t value = on ? 1 : 0;
  Serial.printf("[SlaveController] Setting LED to %s (reg 0x%02X = %d)\n", on ? "ON" : "OFF", SLAVE_REG_LED_ONOFF, value);
  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, SLAVE_REG_LED_ONOFF, value)) {
    lastError = "Failed to set LED";
    stats.failedWrites++;
    Serial.println("[SlaveController] ERROR: LED write failed");
    return false;
  }
  stats.successfulWrites++;
  Serial.println("[SlaveController] ✓ LED set successfully");
  return true;
}

bool SlaveController::pulseLed(uint16_t durationMs) {
  // Initiate LED pulse - main loop handles the timing
  Serial.printf("[SlaveController] Pulsing LED for %d ms (starting now)\n", durationMs);
  if (!setLed(true)) {
    return false;
  }
  return true;
}

void SlaveController::resetStats() {
  stats.successfulReads = 0;
  stats.failedReads = 0;
  stats.successfulWrites = 0;
  stats.failedWrites = 0;
}

// ============================================================================
// Private Helpers
// ============================================================================

bool SlaveController::readRegisterInt16(uint8_t regHigh, uint8_t regLow, int16_t& value) {
  uint8_t high = 0, low = 0;

  if (!I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, regHigh, high, 100, 1)) {
    return false;
  }

  if (!I2CManager::getInstance().readRegister(SLAVE_I2C_ADDR, regLow, low, 100, 1)) {
    return false;
  }

  // Combine bytes (big-endian)
  value = ((int16_t)high << 8) | low;
  
  // Handle signed conversion
  if (value & 0x8000) {
    value = -(int16_t)(~value + 1);
  }
  
  return true;
}

bool SlaveController::writeRegisterInt16(uint8_t regHigh, uint8_t regLow, int16_t value) {
  uint8_t high = (value >> 8) & 0xFF;
  uint8_t low = value & 0xFF;

  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, regHigh, high)) {
    return false;
  }

  if (!I2CManager::getInstance().writeRegister(SLAVE_I2C_ADDR, regLow, low)) {
    return false;
  }

  return true;
}
