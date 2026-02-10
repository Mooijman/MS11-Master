#ifndef SEESAW_ROTARY_H
#define SEESAW_ROTARY_H

#include <Arduino.h>
#include "i2c_manager.h"
#include "config.h"

/**
 * Seesaw Rotary Encoder Manager - Adafruit I2C Rotary Encoder
 * 
 * Singleton pattern for consistent rotary encoder access
 * Uses I2C Bus 1 (GPIO8/9 @ 100kHz, non-critical)
 * 
 * Hardware: Adafruit Seesaw I2C Rotary Encoder breakout
 * I2C Address: 0x36 (default), configurable via CONFIG_H
 * 
 * Features:
 * - Rotary encoder position tracking (32-bit counter)
 * - Button detection with debouncing
 * - Delta reading (position change since last read)
 * - Interrupt support (optional)
 * - Non-blocking operation
 */

// Seesaw register constants are defined in seesaw_rotary.cpp to avoid
// macro conflicts with Adafruit seesaw library enums.

class SeesawRotary {
public:
  // Singleton instance accessor
  static SeesawRotary& getInstance() {
    static SeesawRotary instance;
    return instance;
  }

  // Initialization and shutdown
  bool begin();
  void end();

  // Encoder operations
  int32_t getPosition();      // Get absolute position
  int32_t getDelta();          // Get position change since last read
  float getRotationSpeed();    // Get rotation speed (steps/second, based on position tracking)
  void setPosition(int32_t p); // Set absolute position
  void resetPosition(int32_t p = 0); // Reset to value (default 0)

  // Button operations
  bool isButtonPressed();      // Check if button is currently pressed
  bool getButtonPress();       // Get and clear button press flag

  // NeoPixel control
  bool setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b);  // Set RGB color (0-255 per channel)
  bool setNeoPixelColor(uint32_t color);                    // Set color as 0xRRGGBB
  bool neoPixelOff();                                       // Turn NeoPixel off

  // Status
  bool isInitialized() const { return initialized; }
  bool isHealthy();
  String getLastError() const { return lastError; }

  // Hardware info
  uint8_t getAddress() const { return address; }

  // NeoPixel initialization (public so it can be called once in setup)
  bool neoPixelBegin();

private:
  SeesawRotary();
  ~SeesawRotary();

  // Delete copy constructors
  SeesawRotary(const SeesawRotary&) = delete;
  SeesawRotary& operator=(const SeesawRotary&) = delete;

  // No raw register access; all I2C uses Adafruit seesaw

  // Status tracking
  bool initialized = false;
  bool neoPixelInitialized = false;
  String lastError;
  uint8_t address;
  int32_t lastPosition = 0;
  bool buttonPressed = false;
  
  // Speed tracking
  unsigned long lastSpeedCheck = 0;
  int32_t lastSpeedPosition = 0;
  float currentSpeed = 0.0f;
};

#endif // SEESAW_ROTARY_H
