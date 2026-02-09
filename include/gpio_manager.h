#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * GPIO Manager - Digital I/O Control for Power Switch, Buttons, and LED
 * 
 * Singleton pattern for consistent GPIO control access
 * 
 * Hardware:
 * - Power Switch: GPIO4 (D4) active high
 * - Control Button 1: GPIO3 (D3) active low (with internal pull-up)
 * - Control Button 2: GPIO2 (D2) active low (with internal pull-up)
 * - Status LED: GPIO1 (D1) active high with PWM support
 * 
 * Features:
 * - Non-blocking input polling
 * - Software debouncing for buttons
 * - Long press detection
 * - PWM LED brightness control (0-255)
 * - Event callback support
 */

enum ButtonEvent {
  BTN_EVENT_NONE = 0,
  BTN_EVENT_PRESS = 1,        // Button just pressed (after debounce)
  BTN_EVENT_RELEASE = 2,      // Button just released
  BTN_EVENT_LONGPRESS = 3,    // Long press detected (held > 1 second)
  BTN_EVENT_CLICK = 4         // Single click (press then release)
};

typedef void (*ButtonCallback)(uint8_t button, ButtonEvent event);

class GPIOManager {
public:
  // Singleton instance accessor
  static GPIOManager& getInstance() {
    static GPIOManager instance;
    return instance;
  }

  // Initialization and shutdown
  bool begin();
  void end();

  // ========================================================================
  // Power Switch Operations
  // ========================================================================
  
  // Get current power switch state (true = ON, false = OFF)
  bool isPowerSwitchOn();
  
  // Get previous power switch state
  bool wasPowerSwitchOn();
  
  // Check if power state changed
  bool isPowerSwitchChanged();

  // ========================================================================
  // Button Operations
  // ========================================================================
  
  // Get current button state (true = pressed, false = released)
  bool isButtonPressed(uint8_t button);  // button: 1 or 2
  
  // Get button event (will be cleared after reading)
  ButtonEvent getButtonEvent(uint8_t button);
  
  // Get last button event (doesn't clear it)
  ButtonEvent getLastButtonEvent(uint8_t button);
  
  // Register button event callback
  void onButtonEvent(uint8_t button, ButtonCallback callback);

  // ========================================================================
  // LED Operations
  // ========================================================================
  
  // LED control (on/off)
  void setLED(bool state);
  void ledOn()  { setLED(true); }
  void ledOff() { setLED(false); }
  bool getLED();
  
  // PWM brightness control (0-255)
  void setLEDBrightness(uint8_t brightness);
  uint8_t getLEDBrightness();
  
  // LED effects
  void ledBlink(uint16_t ms_on, uint16_t ms_off, uint8_t count);
  void ledPulse(uint16_t period_ms);  // Smooth PWM fade (non-blocking)
  void ledFadeIn(uint16_t duration_ms);   // Fade to ON (non-blocking)
  void ledFadeOut(uint16_t duration_ms);  // Fade to OFF (non-blocking)

  // ========================================================================
  // Update Function (Call in loop())
  // ========================================================================
  
  // Must be called regularly in loop() to process debouncing and events
  void update();
  
  // ========================================================================
  // Status
  // ========================================================================
  
  bool isInitialized() const { return initialized; }
  String getLastError() const { return lastError; }

private:
  GPIOManager();
  ~GPIOManager();

  // Delete copy constructors
  GPIOManager(const GPIOManager&) = delete;
  GPIOManager& operator=(const GPIOManager&) = delete;

  // Internal state tracking
  bool initialized = false;
  String lastError;
  
  // Power switch state
  bool powerSwitchState = false;
  bool powerSwitchPrevState = false;
  
  // Button state and debouncing
  struct ButtonState {
    bool currentState = false;
    bool prevState = false;
    bool debounced = false;
    unsigned long pressTime = 0;
    ButtonEvent lastEvent = BTN_EVENT_NONE;
    ButtonCallback callback = nullptr;
  };
  
  ButtonState button1;
  ButtonState button2;
  
  // LED state
  bool ledState = false;
  uint8_t ledBrightness = 255;  // Current PWM value
  unsigned long pulseStartTime = 0;
  uint16_t pulsePeriodMs = 2000;
  bool isPulsing = false;

  // Fade animation
  bool isFading = false;
  bool fadeIn = true;
  unsigned long fadeStartTime = 0;
  uint16_t fadeDurationMs = 0;
  
  // Blink animation
  bool isBlinking = false;
  unsigned long blinkStartTime = 0;
  uint16_t blinkOnTime = 0;
  uint16_t blinkOffTime = 0;
  uint8_t blinkCount = 0;
  uint8_t blinkCounter = 0;
  
  // Helper methods
  void updateButtonState(ButtonState& btn, uint8_t pin);
  void checkButtonPress(ButtonState& btn, uint8_t button_num);
  void triggerButtonEvent(uint8_t button, ButtonEvent event);
};

#endif // GPIO_MANAGER_H
