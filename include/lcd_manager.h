#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "i2c_manager.h"
#include "config.h"

/**
 * LCD Manager - 16x2/20x4 I2C LCD Display Controller
 * 
 * Singleton pattern for consistent LCD access across application
 * Uses I2C Bus 1 (GPIO8/9 @ 100kHz, non-critical)
 * 
 * Hardware: 16x2 character LCD with PCF8574 I2C backpack
 * I2C Address: 0x27 (typical), configurable via CONFIG_H
 * 
 * Features:
 * - Automatic initialization on first use
 * - Safe I2C operation with timeout handling
 * - Custom character support
 * - Backlight control
 * - Health monitoring (pings device)
 * - Non-blocking operation (display failures don't block system)
 */
class LCDManager {
public:
  // Singleton instance accessor
  static LCDManager& getInstance() {
    static LCDManager instance;
    return instance;
  }

  // Initialization and shutdown
  bool begin();
  void end();

  // Display operations
  void clear();
  void home();
  void print(const String& text);
  void setCursor(uint8_t col, uint8_t row);
  void write(uint8_t character);
  void printf(const char* format, ...);

  // Line-based operations
  void printLine(uint8_t row, const String& text);
  void clearLine(uint8_t row);
  void printLineCenter(uint8_t row, const String& text);
  void printLineRight(uint8_t row, const String& text);

  // Backlight control
  void backlight();
  void noBacklight();
  void setBacklight(bool state);

  // Cursor control
  void noCursor();
  void cursor();
  void noBlink();
  void blink();

  // Display control
  void display();
  void noDisplay();

  // Custom character support (up to 8)
  void createChar(uint8_t location, uint8_t charmap[8]);

  // Scrolling (if supported)
  void autoscroll();
  void noAutoscroll();
  void leftToRight();
  void rightToLeft();

  // Health check
  bool isInitialized() const { return initialized; }
  bool isHealthy();
  String getLastError() const { return lastError; }

  // Status information
  uint8_t getCols() const { return cols; }
  uint8_t getRows() const { return rows; }
  uint8_t getAddress() const { return address; }

private:
  LCDManager();
  ~LCDManager();

  // Delete copy constructors
  LCDManager(const LCDManager&) = delete;
  LCDManager& operator=(const LCDManager&) = delete;

  // Hardware interface
  LiquidCrystal_I2C lcd;
  
  // Status tracking
  bool initialized = false;
  bool backlight_state = true;
  String lastError;

  // Configuration
  uint8_t address;
  uint8_t cols;
  uint8_t rows;

  // Helper method
  bool safeOperation(const char* op_name);
};

#endif // LCD_MANAGER_H
