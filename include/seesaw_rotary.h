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

// Seesaw I2C Protocol constants
#define SEESAW_STATUS_BASE        0x00
#define SEESAW_GPIO_BASE          0x01
#define SEESAW_SERCOM0_BASE       0x02
#define SEESAW_TIMER_BASE         0x08
#define SEESAW_ADC_BASE           0x09
#define SEESAW_DAC_BASE           0x0A
#define SEESAW_INTERRUPT_BASE     0x0B
#define SEESAW_DAP_BASE           0x0D
#define SEESAW_ENCODER_BASE       0x11
#define SEESAW_NEOPIXEL_BASE      0x0E

// NeoPixel registers
#define SEESAW_NEOPIXEL_STATUS    0x01
#define SEESAW_NEOPIXEL_PIN       0x02
#define SEESAW_NEOPIXEL_SPEED     0x03
#define SEESAW_NEOPIXEL_BUF_LENGTH 0x04
#define SEESAW_NEOPIXEL_BUF       0x05
#define SEESAW_NEOPIXEL_SHOW      0x06

// Encoder registers
#define SEESAW_ENCODER_STATUS     0x02
#define SEESAW_ENCODER_POSITION   0x03
#define SEESAW_ENCODER_DELTA      0x04
#define SEESAW_ENCODER_INTENSITY  0x06

// Product ID register
#define SEESAW_STATUS_HW_ID       0x01
#define SEESAW_STATUS_VERSION     0x02

// GPIO registers for button
#define SEESAW_GPIO_DIRSET_BULK   0x0C
#define SEESAW_GPIO_BULK_SET      0x0E
#define SEESAW_GPIO_BULK_CLEAR    0x0F
#define SEESAW_GPIO_BULK          0x10

// Default pin for rotary encoder button (usually pin 24)
#define SEESAW_BUTTON_PIN         24

// Hardware pin number for NeoPixel on rotary encoder PCB
#define SEESAW_NEO_PIN_NUMBER     6

// NeoPixel speed constant
#define SEESAW_NEOPIXEL_KHZ800    0x01

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
  void setPosition(int32_t p); // Set absolute position
  void resetPosition(int32_t p = 0); // Reset to value (default 0)

  // Button operations
  bool isButtonPressed();      // Check if button is currently pressed
  bool getButtonPress();       // Get and clear button press flag

  // Configuration
  void setIntensity(uint8_t intensity);  // LED intensity for rotary indicator (0-255)

  // NeoPixel control
  bool setNeoPixelColor(uint8_t r, uint8_t g, uint8_t b);  // Set RGB color (0-255 per channel)
  bool setNeoPixelColor(uint32_t color);                    // Set color as 0xRRGGBB
  bool neoPixelOff();                                       // Turn NeoPixel off

  // Status
  bool isInitialized() const { return initialized; }
  bool isHealthy();
  String getLastError() const { return lastError; }

  // Hardware info
  uint32_t getProductID();
  uint32_t getVersion();
  uint8_t getAddress() const { return address; }

private:
  SeesawRotary();
  ~SeesawRotary();

  // Delete copy constructors
  SeesawRotary(const SeesawRotary&) = delete;
  SeesawRotary& operator=(const SeesawRotary&) = delete;

  // I2C communication
  bool readRegister(uint8_t regHigh, uint8_t regLow, uint8_t* buffer, uint8_t len);
  bool writeRegister(uint8_t regHigh, uint8_t regLow, uint8_t* buffer, uint8_t len);
  uint32_t read32();
  uint16_t read16();
  uint8_t read8();
  
  // NeoPixel helper
  bool neoPixelBegin();
  bool neoPixelShow();

  // Status tracking
  bool initialized = false;
  bool neoPixelInitialized = false;
  String lastError;
  uint8_t address;
  int32_t lastPosition = 0;
  bool buttonPressed = false;
};

#endif // SEESAW_ROTARY_H
