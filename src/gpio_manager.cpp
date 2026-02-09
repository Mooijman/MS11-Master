#include "gpio_manager.h"
#include "seesaw_rotary.h"

// NeoPixel color rotation array (R, G, B)
struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  const char* name;
};

const RGBColor COLOR_ROTATION[] = {
  {255, 0, 0, "Red"},       // Rood
  {255, 128, 0, "Orange"},  // Oranje
  {255, 255, 0, "Yellow"},  // Geel
  {0, 255, 0, "Green"},     // Groen
  {0, 0, 255, "Blue"}       // Blauw
};
const uint8_t COLOR_COUNT = sizeof(COLOR_ROTATION) / sizeof(COLOR_ROTATION[0]);

// Current color index tracker
static uint8_t currentColorIndex = 0;

GPIOManager::GPIOManager() {
  // Constructor does minimal initialization
}

GPIOManager::~GPIOManager() {
  end();
}

bool GPIOManager::begin() {
  if (initialized) {
    return true;
  }

  // Configure power switch pin as input with pull-down
  pinMode(GPIO_POWER_SWITCH, INPUT_PULLDOWN);
  
  // Configure button pins as inputs with pull-up
  pinMode(GPIO_CONTROL_BTN1, INPUT_PULLUP);
  pinMode(GPIO_CONTROL_BTN2, INPUT_PULLUP);
  
  // Configure LED pin as output
  pinMode(GPIO_STATUS_LED, OUTPUT);
  digitalWrite(GPIO_STATUS_LED, LOW);  // LED off initially
  
  // Configure PWM for LED using Arduino's analogWrite
  // ESP32 uses PWM automatically on supported pins
  // GPIO1 (D1) is PWM capable on XIAO S3
  
  initialized = true;
  
  Serial.println("[GPIOManager] ✓ GPIO control initialized:");
  Serial.println("[GPIOManager]   - Power Switch: GPIO" + String(GPIO_POWER_SWITCH) + " (D4)");
  Serial.println("[GPIOManager]   - Button 1: GPIO" + String(GPIO_CONTROL_BTN1) + " (D3)");
  Serial.println("[GPIOManager]   - Button 2: GPIO" + String(GPIO_CONTROL_BTN2) + " (D2)");
  Serial.println("[GPIOManager]   - Status LED: GPIO" + String(GPIO_STATUS_LED) + " (D1) with PWM");
  
  return true;
}

void GPIOManager::end() {
  if (initialized) {
    ledOff();
    initialized = false;
    Serial.println("[GPIOManager] GPIO control shutdown");
  }
}

// ============================================================================
// Power Switch Operations
// ============================================================================

bool GPIOManager::isPowerSwitchOn() {
  if (!initialized) return false;
  return digitalRead(GPIO_POWER_SWITCH) == HIGH;
}

bool GPIOManager::wasPowerSwitchOn() {
  return powerSwitchPrevState;
}

bool GPIOManager::isPowerSwitchChanged() {
  return powerSwitchState != powerSwitchPrevState;
}

// ============================================================================
// Button Operations
// ============================================================================

bool GPIOManager::isButtonPressed(uint8_t button) {
  if (!initialized) return false;
  
  if (button == 1) {
    return button1.debounced;
  } else if (button == 2) {
    return button2.debounced;
  }
  return false;
}

ButtonEvent GPIOManager::getButtonEvent(uint8_t button) {
  ButtonEvent event = (button == 1) ? button1.lastEvent : button2.lastEvent;
  
  // Clear event after reading
  if (button == 1) {
    button1.lastEvent = BTN_EVENT_NONE;
  } else if (button == 2) {
    button2.lastEvent = BTN_EVENT_NONE;
  }
  
  return event;
}

ButtonEvent GPIOManager::getLastButtonEvent(uint8_t button) {
  if (button == 1) {
    return button1.lastEvent;
  } else if (button == 2) {
    return button2.lastEvent;
  }
  return BTN_EVENT_NONE;
}

void GPIOManager::onButtonEvent(uint8_t button, ButtonCallback callback) {
  if (button == 1) {
    button1.callback = callback;
  } else if (button == 2) {
    button2.callback = callback;
  }
}

// ============================================================================
// LED Operations
// ============================================================================

void GPIOManager::setLED(bool state) {
  if (!initialized) return;
  
  ledState = state;
  if (state) {
    analogWrite(GPIO_STATUS_LED, ledBrightness);
  } else {
    analogWrite(GPIO_STATUS_LED, 0);
  }
}

bool GPIOManager::getLED() {
  return ledState;
}

void GPIOManager::setLEDBrightness(uint8_t brightness) {
  if (!initialized) return;
  
  ledBrightness = brightness;
  if (ledState) {
    analogWrite(GPIO_STATUS_LED, brightness);
  }
}

uint8_t GPIOManager::getLEDBrightness() {
  return ledBrightness;
}

void GPIOManager::ledBlink(uint16_t ms_on, uint16_t ms_off, uint8_t count) {
  if (!initialized) return;
  
  isPulsing = false;
  isFading = false;
  isBlinking = true;
  blinkOnTime = ms_on;
  blinkOffTime = ms_off;
  blinkCount = count;
  blinkCounter = 0;
  blinkStartTime = millis();
  
  setLED(true);
}

void GPIOManager::ledPulse(uint16_t period_ms) {
  if (!initialized) return;
  
  isBlinking = false;
  isFading = false;
  isPulsing = true;
  pulsePeriodMs = (period_ms == 0) ? 2000 : period_ms;
  pulseStartTime = millis();
}

void GPIOManager::ledFadeIn(uint16_t duration_ms) {
  if (!initialized) return;

  isBlinking = false;
  isPulsing = false;
  isFading = true;
  fadeIn = true;
  fadeDurationMs = (duration_ms == 0) ? 1000 : duration_ms;
  fadeStartTime = millis();
  ledState = true;
}

void GPIOManager::ledFadeOut(uint16_t duration_ms) {
  if (!initialized) return;

  isBlinking = false;
  isPulsing = false;
  isFading = true;
  fadeIn = false;
  fadeDurationMs = (duration_ms == 0) ? 1000 : duration_ms;
  fadeStartTime = millis();
  ledState = true;  // Keep PWM active during fade-out
}

// ============================================================================
// Update Function
// ============================================================================

void GPIOManager::update() {
  if (!initialized) return;
  
  unsigned long now = millis();
  
  // ========== Power Switch Update ==========
  powerSwitchPrevState = powerSwitchState;
  powerSwitchState = isPowerSwitchOn();
  
  // Toggle LED when power switch is pressed (transition from OFF to ON)
  if (powerSwitchState && !powerSwitchPrevState) {
    setLED(!ledState);  // Toggle LED state
    Serial.println("[GPIOManager] Power switch pressed - LED toggled to " + String(ledState ? "ON" : "OFF"));
  }
  
  // ========== Button Debouncing ==========
  updateButtonState(button1, GPIO_CONTROL_BTN1);
  updateButtonState(button2, GPIO_CONTROL_BTN2);
  
  checkButtonPress(button1, 1);
  checkButtonPress(button2, 2);
  
  // ========== Seesaw NeoPixel Control ==========
  // Button 1: Rotate through colors (Red → Orange → Yellow → Green → Blue → Red...)
  if (button1.lastEvent == BTN_EVENT_CLICK) {
    SeesawRotary& seesaw = SeesawRotary::getInstance();
    if (seesaw.isInitialized()) {
      const RGBColor& color = COLOR_ROTATION[currentColorIndex];
      if (seesaw.setNeoPixelColor(color.r, color.g, color.b)) {
        Serial.println("[GPIOManager] Button 1 pressed - NeoPixel set to " + String(color.name));
        currentColorIndex = (currentColorIndex + 1) % COLOR_COUNT;  // Rotate to next color
      } else {
        Serial.println("[GPIOManager] Failed to set NeoPixel color");
      }
    }
  }
  
  // Button 2: Turn NeoPixel off
  if (button2.lastEvent == BTN_EVENT_CLICK) {
    SeesawRotary& seesaw = SeesawRotary::getInstance();
    if (seesaw.isInitialized()) {
      if (seesaw.neoPixelOff()) {
        Serial.println("[GPIOManager] Button 2 pressed - NeoPixel turned OFF");
      } else {
        Serial.println("[GPIOManager] Failed to turn off NeoPixel");
      }
    }
  }
  
  // ========== LED Blink Animation ==========
  if (isBlinking) {
    unsigned long elapsed = now - blinkStartTime;
    uint16_t period = blinkOnTime + blinkOffTime;
    uint16_t phase = elapsed % period;
    
    if (phase < blinkOnTime) {
      analogWrite(GPIO_STATUS_LED, ledBrightness);
    } else {
      analogWrite(GPIO_STATUS_LED, 0);
    }
    
    // Count complete cycles
    if (elapsed >= (period * blinkCount)) {
      isBlinking = false;
      setLED(ledState);  // Restore previous state
    }
  }
  
  // ========== LED Pulse Animation ==========
  if (isPulsing) {
    unsigned long elapsed = now - pulseStartTime;
    uint16_t period = pulsePeriodMs;
    if (period < 2) {
      period = 2;
    }
    uint16_t phase = elapsed % period;
    uint16_t half = period / 2;
    if (half == 0) {
      half = 1;
    }
    
    uint8_t brightness;
    if (phase < half) {
      // Fade in
      brightness = (phase * 255) / half;
    } else {
      // Fade out
      brightness = ((period - phase) * 255) / half;
    }
    
    if (ledState) {
      analogWrite(GPIO_STATUS_LED, brightness);
    }
  }

  // ========== LED Fade Animation ==========
  if (isFading) {
    unsigned long elapsed = now - fadeStartTime;
    if (elapsed >= fadeDurationMs) {
      if (fadeIn) {
        ledBrightness = 255;
        analogWrite(GPIO_STATUS_LED, ledBrightness);
      } else {
        ledBrightness = 0;
        analogWrite(GPIO_STATUS_LED, 0);
        ledState = false;
      }
      isFading = false;
    } else {
      uint8_t brightness = (elapsed * 255) / fadeDurationMs;
      if (!fadeIn) {
        brightness = 255 - brightness;
      }
      analogWrite(GPIO_STATUS_LED, brightness);
    }
  }
}

// ============================================================================
// Helper Methods
// ============================================================================

void GPIOManager::updateButtonState(ButtonState& btn, uint8_t pin) {
  bool raw = digitalRead(pin) == LOW;  // LOW = pressed (active low)
  
  // Debouncing logic
  if (raw != btn.currentState) {
    btn.currentState = raw;
    btn.pressTime = millis();
  }
  
  if (millis() - btn.pressTime >= GPIO_DEBOUNCE_MS) {
    btn.debounced = btn.currentState;
  }
}

void GPIOManager::checkButtonPress(ButtonState& btn, uint8_t button_num) {
  unsigned long now = millis();
  bool state = btn.debounced;
  bool prevState = btn.prevState;
  
  // Detect press (transition from released to pressed)
  if (state && !prevState) {
    btn.lastEvent = BTN_EVENT_PRESS;
    btn.pressTime = now;
    triggerButtonEvent(button_num, BTN_EVENT_PRESS);
  }
  
  // Detect release (transition from pressed to released)
  if (!state && prevState) {
    unsigned long pressDuration = now - btn.pressTime;
    
    if (pressDuration >= GPIO_LONGPRESS_MS) {
      // Long press detected
      btn.lastEvent = BTN_EVENT_LONGPRESS;
      triggerButtonEvent(button_num, BTN_EVENT_LONGPRESS);
    } else {
      // Short click
      btn.lastEvent = BTN_EVENT_CLICK;
      triggerButtonEvent(button_num, BTN_EVENT_CLICK);
    }
    
    btn.lastEvent = BTN_EVENT_RELEASE;
    triggerButtonEvent(button_num, BTN_EVENT_RELEASE);
  }
  
  btn.prevState = state;
}

void GPIOManager::triggerButtonEvent(uint8_t button, ButtonEvent event) {
  ButtonState& btn = (button == 1) ? button1 : button2;
  
  if (btn.callback != nullptr) {
    btn.callback(button, event);
  }
}
