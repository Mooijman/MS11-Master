#include "seesaw_rotary.h"
#include <Wire.h>

SeesawRotary::SeesawRotary() 
  : address(SEESAW_I2C_ADDRESS) {
  // Constructor does minimal initialization
}

SeesawRotary::~SeesawRotary() {
  end();
}

bool SeesawRotary::begin() {
  if (initialized) {
    return true;
  }

  // Check I2C manager is initialized
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "I2C Manager not initialized";
      Serial.println("[SeesawRotary] ERROR: " + lastError);
      return false;
    }
  }

  // Verify Seesaw device is present
  if (!I2CManager::getInstance().ping(address, I2C_BUS_DISPLAY)) {
    lastError = "Seesaw device not found at 0x" + String(address, HEX);
    Serial.println("[SeesawRotary] WARNING: " + lastError);
    // Continue anyway - may still work
  }

  // Try to read product ID to verify device
  uint32_t productID = getProductID();
  if (productID != 0x87) {  // Expected product ID for rotary encoder
    Serial.println("[SeesawRotary] WARNING: Unexpected product ID 0x" + String(productID, HEX));
  }

  // Initialize encoder
  // Set encoder position to 0
  resetPosition(0);
  
  // Note: Button GPIO initialization would require more complex register writes
  // This is optional - the device should work without explicitly configuring the button pin
  
  initialized = true;
  lastPosition = 0;
  
  Serial.println("[SeesawRotary] ✓ Seesaw Rotary Encoder initialized (I2C1: 0x" + 
                 String(address, HEX) + " @ 100kHz)");
  return true;
}

void SeesawRotary::end() {
  if (initialized) {
    initialized = false;
    Serial.println("[SeesawRotary] Encoder shutdown");
  }
}

bool SeesawRotary::readRegister(uint8_t regHigh, uint8_t regLow, 
                                uint8_t* buffer, uint8_t len) {
  if (!initialized) return false;
  
  // First write register address bytes
  uint8_t regBytes[2] = {regHigh, regLow};
  if (!I2CManager::getInstance().displayWrite(address, regBytes, 2, 50)) {
    return false;
  }
  
  // Small delay to allow device to prepare data
  delayMicroseconds(100);
  
  // Then read the data
  return I2CManager::getInstance().displayRead(address, buffer, len, 50);
}

bool SeesawRotary::writeRegister(uint8_t regHigh, uint8_t regLow,
                                 uint8_t* buffer, uint8_t len) {
  if (!initialized) return false;
  
  // Combine register address with data
  uint8_t txBuffer[len + 2];
  txBuffer[0] = regHigh;
  txBuffer[1] = regLow;
  memcpy(&txBuffer[2], buffer, len);
  
  return I2CManager::getInstance().displayWrite(address, txBuffer, len + 2, 50);
}

uint32_t SeesawRotary::read32() {
  uint8_t buffer[4];
  readRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_POSITION, buffer, 4);
  return (uint32_t)buffer[0] << 24 | 
         (uint32_t)buffer[1] << 16 |
         (uint32_t)buffer[2] << 8 |
         (uint32_t)buffer[3];
}

uint16_t SeesawRotary::read16() {
  uint8_t buffer[2];
  readRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_STATUS, buffer, 2);
  return (uint16_t)buffer[0] << 8 | (uint16_t)buffer[1];
}

uint8_t SeesawRotary::read8() {
  uint8_t buffer[1];
  readRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_STATUS, buffer, 1);
  return buffer[0];
}

int32_t SeesawRotary::getPosition() {
  if (!initialized) return 0;
  
  // Read 4-byte position register
  uint8_t buffer[4] = {0};
  if (!readRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_POSITION, buffer, 4)) {
    lastError = "Failed to read position";
    return lastPosition;
  }
  
  // Convert to int32 (big-endian)
  int32_t pos = ((int32_t)buffer[0] << 24) |
                ((int32_t)buffer[1] << 16) |
                ((int32_t)buffer[2] << 8) |
                ((int32_t)buffer[3]);
  
  lastPosition = pos;
  return pos;
}

int32_t SeesawRotary::getDelta() {
  if (!initialized) return 0;
  
  // Read delta (position change)
  uint8_t buffer[4] = {0};
  if (!readRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_DELTA, buffer, 4)) {
    lastError = "Failed to read delta";
    return 0;
  }
  
  int32_t delta = ((int32_t)buffer[0] << 24) |
                  ((int32_t)buffer[1] << 16) |
                  ((int32_t)buffer[2] << 8) |
                  ((int32_t)buffer[3]);
  
  return delta;
}

void SeesawRotary::setPosition(int32_t p) {
  if (!initialized) return;
  
  uint8_t buffer[4];
  buffer[0] = (p >> 24) & 0xFF;
  buffer[1] = (p >> 16) & 0xFF;
  buffer[2] = (p >> 8) & 0xFF;
  buffer[3] = p & 0xFF;
  
  writeRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_POSITION, buffer, 4);
  lastPosition = p;
}

void SeesawRotary::resetPosition(int32_t p) {
  setPosition(p);
}

bool SeesawRotary::isButtonPressed() {
  if (!initialized) return false;
  
  // Read GPIO state for button pin
  uint8_t buffer[3] = {0};
  if (!readRegister(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, buffer, 3)) {
    return false;
  }
  
  // Check bit for button pin (usually pin 24)
  uint32_t pinState = ((uint32_t)buffer[0] << 16) | 
                      ((uint32_t)buffer[1] << 8) | 
                      (uint32_t)buffer[2];
  
  // Button is pressed when bit is 0 (active low)
  return !(pinState & (1u << SEESAW_BUTTON_PIN));
}

bool SeesawRotary::getButtonPress() {
  bool pressed = isButtonPressed();
  if (pressed && !buttonPressed) {
    buttonPressed = true;
    return true;  // New press detected
  }
  if (!pressed && buttonPressed) {
    buttonPressed = false;
  }
  return false;
}

void SeesawRotary::setIntensity(uint8_t intensity) {
  if (!initialized) return;
  
  uint8_t buffer[1] = {intensity};
  writeRegister(SEESAW_ENCODER_BASE, SEESAW_ENCODER_INTENSITY, buffer, 1);
}

bool SeesawRotary::neoPixelBegin() {
  if (!initialized) return false;
  
  // Set NeoPixel pin (pin 6 on rotary encoder)
  uint8_t pin = SEESAW_NEO_PIN_NUMBER;
  if (!writeRegister(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_PIN, &pin, 1)) {
    Serial.println("[SeesawRotary] Failed to set NeoPixel pin");
    return false;
  }
  
  // Set NeoPixel speed (800kHz for WS2812)
  uint8_t speed = SEESAW_NEOPIXEL_KHZ800;
  if (!writeRegister(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SPEED, &speed, 1)) {
    Serial.println("[SeesawRotary] Failed to set NeoPixel speed");
    return false;
  }
  
  // Set buffer length (1 pixel)
  uint8_t buflen[2] = {0, 1};  // 16-bit value: 1 pixel
  if (!writeRegister(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF_LENGTH, buflen, 2)) {
    Serial.println("[SeesawRotary] Failed to set NeoPixel buffer length");
    return false;
  }
  
  neoPixelInitialized = true;
  Serial.println("[SeesawRotary] NeoPixel initialized");
  return true;
}

bool SeesawRotary::neoPixelShow() {
  if (!initialized || !neoPixelInitialized) return false;
  
  // Send show command
  uint8_t dummy = 0;
  return writeRegister(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SHOW, &dummy, 0);
}

bool SeesawRotary::setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!initialized) return false;
  
  // Initialize NeoPixel if not already done
  if (!neoPixelInitialized) {
    if (!neoPixelBegin()) return false;
  }
  
  // Write RGB data to buffer (GRB order for WS2812)
  uint8_t pixelData[3] = {g, r, b};
  if (!writeRegister(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, pixelData, 3)) {
    return false;
  }
  
  // Update the LED
  return neoPixelShow();
}

bool SeesawRotary::setNeoPixelColor(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  return setNeoPixelColor(r, g, b);
}

bool SeesawRotary::neoPixelOff() {
  return setNeoPixelColor(0, 0, 0);
}

bool SeesawRotary::isHealthy() {
  if (!initialized) return false;
  
  // Check if device responds to I2C
  return I2CManager::getInstance().ping(address, I2C_BUS_DISPLAY);
}

uint32_t SeesawRotary::getProductID() {
  uint8_t buffer[1] = {0};
  if (!readRegister(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, buffer, 1)) {
    return 0;
  }
  return buffer[0];
}

uint32_t SeesawRotary::getVersion() {
  uint8_t buffer[4] = {0};
  if (!readRegister(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION, buffer, 4)) {
    return 0;
  }
  return ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
         ((uint32_t)buffer[2] << 8) | (uint32_t)buffer[3];
}
