#include "display_manager.h"

DisplayManager::DisplayManager() 
  : display(DISPLAY_I2C_ADDRESS, 8, 9, GEOMETRY_128_64, I2C_ONE, 100000) {  // Bus 0 I2C (GPIO8/9 @ 100kHz, Wire/I2C0)
}

DisplayManager::~DisplayManager() {
  end();
}

bool DisplayManager::begin() {
  if (initialized) {
    return true;
  }

  // Initialize I2C manager if needed
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "I2C Manager initialization failed";
      return false;
    }
  }

  // Initialize SSD1306 display
  // Note: I2C address and pins already configured in constructor
  if (!display.init()) {
    lastError = "SSD1306 initialization failed";
    Serial.println("[DisplayManager] ERROR: " + lastError);
    return false;
  }

  display.flipScreenVertically();
  display.clear();
  display.display();

  initialized = true;
  Serial.println("[DisplayManager] ✓ Display initialized (I2C1: GPIO8/9 @ 100kHz)");
  return true;
}

void DisplayManager::end() {
  initialized = false;
  Serial.println("[DisplayManager] Display shutdown");
}

void DisplayManager::clear() {
  if (!initialized) return;
  display.clear();
}

void DisplayManager::updateDisplay() {
  if (!initialized) return;
  display.display();
}

void DisplayManager::setFont(const uint8_t *fontData) {
  if (!initialized) return;
  display.setFont(fontData);
}

void DisplayManager::setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT align) {
  if (!initialized) return;
  display.setTextAlignment(align);
}

void DisplayManager::drawString(uint16_t x, uint16_t y, const String& text) {
  if (!initialized) return;
  display.drawString(x, y, text);
}

void DisplayManager::drawStringCenter(uint16_t y, const String& text) {
  if (!initialized) return;
  
  // Calculate center position based on text width
  // Approximate: 6 pixels per character with default font
  uint16_t textWidth = text.length() * 6;
  uint16_t x = (DISPLAY_WIDTH - textWidth) / 2;
  
  display.drawString(x, y, text);
}

void DisplayManager::drawStringMaxWidth(uint16_t x, uint16_t y, uint16_t maxWidth, const String& text) {
  if (!initialized) return;
  display.drawStringMaxWidth(x, y, maxWidth, text);
}

void DisplayManager::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  if (!initialized) return;
  display.drawLine(x1, y1, x2, y2);
}

void DisplayManager::drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  if (!initialized) return;
  display.drawRect(x, y, width, height);
}

void DisplayManager::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  if (!initialized) return;
  display.fillRect(x, y, width, height);
}

void DisplayManager::drawCircle(uint16_t x, uint16_t y, uint16_t radius) {
  if (!initialized) return;
  display.drawCircle(x, y, radius);
}

void DisplayManager::fillCircle(uint16_t x, uint16_t y, uint16_t radius) {
  if (!initialized) return;
  display.fillCircle(x, y, radius);
}

void DisplayManager::drawXbm(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *xbm) {
  if (!initialized) return;
  display.drawXbm(x, y, width, height, xbm);
}

void DisplayManager::setPixel(uint16_t x, uint16_t y) {
  if (!initialized) return;
  display.setPixel(x, y);
}

void DisplayManager::clearPixel(uint16_t x, uint16_t y) {
  if (!initialized) return;
  display.clearPixel(x, y);
}

void DisplayManager::invert(boolean inv) {
  if (!initialized) return;
  if (inv) {
    display.invertDisplay();  // Method takes no args, toggles state
  }
}

void DisplayManager::flipVertical(boolean flip) {
  if (!initialized) return;
  display.flipScreenVertically();
}

void DisplayManager::flipHorizontal(boolean flip) {
  if (!initialized) return;
  // Note: SSD1306 library may not support horizontal flip
  // This is a placeholder
}

bool DisplayManager::isHealthy() {
  if (!initialized) return false;
  return I2CManager::getInstance().isDisplayBusHealthy();
}

bool DisplayManager::safeWrite(uint8_t* data, uint16_t length) {
  if (!initialized) return false;
  return I2CManager::getInstance().displayWrite(DISPLAY_I2C_ADDRESS, data, length, 50);
}
