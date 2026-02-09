#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <SSD1306.h>
#include "i2c_manager.h"

// Display configuration
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

class DisplayManager {
public:
  // Singleton
  static DisplayManager& getInstance() {
    static DisplayManager instance;
    return instance;
  }

  // Initialize display (use Display Bus I2C1)
  bool begin();
  void end();

  // Core drawing functions
  void clear();
  void updateDisplay();  // Refresh screen (was display())
  
  // Text drawing and font
  void setFont(const uint8_t *fontData);
  void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT align);
  void drawString(uint16_t x, uint16_t y, const String& text);
  void drawStringCenter(uint16_t y, const String& text);
  void drawStringMaxWidth(uint16_t x, uint16_t y, uint16_t maxWidth, const String& text);
  
  // Graphics
  void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  void drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
  void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
  void drawCircle(uint16_t x, uint16_t y, uint16_t radius);
  void fillCircle(uint16_t x, uint16_t y, uint16_t radius);
  void drawXbm(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *xbm);
  
  // Pixel operations
  void setPixel(uint16_t x, uint16_t y);
  void clearPixel(uint16_t x, uint16_t y);
  
  // Buffer operations
  void invert(boolean inv);
  void flipVertical(boolean flip);
  void flipHorizontal(boolean flip);
  
  // Status check
  bool isInitialized() { return initialized; }
  bool isHealthy();  // Check if display responds to I2C
  
  String getLastError() { return lastError; }

private:
  DisplayManager();  // Private constructor
  ~DisplayManager();
  
  // Delete copy constructors
  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;

  SSD1306 display;
  bool initialized = false;
  String lastError;
  
  // Helper: Safe I2C write for display
  bool safeWrite(uint8_t* data, uint16_t length);
};

#endif // DISPLAY_MANAGER_H
