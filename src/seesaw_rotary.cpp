#include "seesaw_rotary.h"
#include "Adafruit_seesaw.h"
#include "seesaw_neopixel.h"

namespace {
constexpr uint8_t kSeesawButtonPin = 24;
constexpr uint8_t kSeesawNeoPinNumber = 6;

Adafruit_seesaw gSeesawNeo;
seesaw_NeoPixel gSeesawPixels(1, kSeesawNeoPinNumber, NEO_GRB + NEO_KHZ800);
bool gSeesawReady = false;
}

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

int32_t SeesawRotary::getPosition() {
  if (!initialized) return 0;
  if (!gSeesawReady) return lastPosition;

  int32_t pos = gSeesawNeo.getEncoderPosition();
  lastPosition = pos;
  return pos;
}

int32_t SeesawRotary::getDelta() {
  if (!initialized) return 0;
  if (!gSeesawReady) return 0;

  return gSeesawNeo.getEncoderDelta();
}

float SeesawRotary::getRotationSpeed() {
  if (!initialized) return 0.0f;
  
  unsigned long now = millis();
  
  // Sample every 50ms for responsive speed calculation
  if (now - lastSpeedCheck < 50) {
    return currentSpeed;
  }
  
  int32_t currentPos = getPosition();
  int32_t delta = currentPos - lastSpeedPosition;
  unsigned long elapsed = now - lastSpeedCheck;
  
  // Calculate steps per second
  if (elapsed > 0) {
    currentSpeed = (delta * 1000.0f) / elapsed;
  }
  
  lastSpeedCheck = now;
  lastSpeedPosition = currentPos;
  
  return currentSpeed;
}

void SeesawRotary::setPosition(int32_t p) {
  if (!initialized) return;
  if (!gSeesawReady) return;

  gSeesawNeo.setEncoderPosition(p);
  lastPosition = p;
}

void SeesawRotary::resetPosition(int32_t p) {
  setPosition(p);
}

bool SeesawRotary::isButtonPressed() {
  if (!initialized) return false;
  if (!gSeesawReady) return false;

  return gSeesawNeo.digitalRead(kSeesawButtonPin) == 0;
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

bool SeesawRotary::neoPixelBegin() {
  if (!initialized) return false;

  if (!gSeesawNeo.begin(address)) {
    Serial.println("[SeesawRotary] Failed to init Adafruit seesaw");
    return false;
  }

  gSeesawReady = true;
  
  // Configure button pin
  gSeesawNeo.pinMode(kSeesawButtonPin, INPUT_PULLUP);
  
  // Initialize encoder position to 0
  gSeesawNeo.setEncoderPosition(0);
  lastPosition = 0;

  if (!gSeesawPixels.begin(address)) {
    Serial.println("[SeesawRotary] Failed to init seesaw NeoPixel");
    return false;
  }

  gSeesawPixels.setBrightness(32);
  gSeesawPixels.show();

  neoPixelInitialized = true;
  Serial.println("[SeesawRotary] ✓ NeoPixel + Encoder initialized (Adafruit)");
  return true;
}

bool SeesawRotary::setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!initialized || !neoPixelInitialized) return false;
  if (!gSeesawReady) return false;

  gSeesawPixels.setPixelColor(0, gSeesawPixels.Color(r, g, b));
  gSeesawPixels.show();
  return true;
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

