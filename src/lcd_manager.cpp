#include "lcd_manager.h"

LCDManager::LCDManager()
  : lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS),
    address(LCD_I2C_ADDRESS),
    cols(LCD_COLS),
    rows(LCD_ROWS) {
  // Constructor does minimal initialization
}

LCDManager::~LCDManager() {
  end();
}

bool LCDManager::begin() {
  if (initialized) {
    return true;
  }

  // Check I2C manager is initialized
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "I2C Manager not initialized";
      Serial.println("[LCDManager] ERROR: " + lastError);
      return false;
    }
  }

  // Verify LCD is present on I2C bus
  if (!I2CManager::getInstance().isDisplayBusHealthy()) {
    Serial.println("[LCDManager] WARNING: Display bus may not be ready");
    // Continue anyway - LCD may still work
  }

  // Initialize LCD
  try {
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.home();
    
    initialized = true;
    backlight_state = true;
    
    Serial.println("[LCDManager] ✓ LCD 16x2 initialized (I2C1: 0x" + 
                   String(address, HEX) + " @ 100kHz)");
    return true;
  } catch (...) {
    lastError = "LCD initialization failed";
    Serial.println("[LCDManager] ERROR: " + lastError);
    return false;
  }
}

void LCDManager::end() {
  if (initialized) {
    noBacklight();
    noDisplay();
    initialized = false;
    Serial.println("[LCDManager] LCD shutdown");
  }
}

bool LCDManager::safeOperation(const char* op_name) {
  if (!initialized) {
    lastError = String("Not initialized");
    return false;
  }
  return true;
}

void LCDManager::clear() {
  if (!safeOperation("clear")) return;
  lcd.clear();
}

void LCDManager::home() {
  if (!safeOperation("home")) return;
  lcd.home();
}

void LCDManager::print(const String& text) {
  if (!safeOperation("print")) return;
  lcd.print(text);
}

void LCDManager::setCursor(uint8_t col, uint8_t row) {
  if (!safeOperation("setCursor")) return;
  if (col >= cols || row >= rows) {
    lastError = "Cursor position out of bounds";
    return;
  }
  lcd.setCursor(col, row);
}

void LCDManager::write(uint8_t character) {
  if (!safeOperation("write")) return;
  lcd.write(character);
}

void LCDManager::printf(const char* format, ...) {
  if (!safeOperation("printf")) return;
  
  char buffer[33];  // Max length for 16x2 display
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  lcd.print(buffer);
}

void LCDManager::printLine(uint8_t row, const String& text) {
  if (!safeOperation("printLine")) return;
  if (row >= rows) {
    lastError = "Row out of bounds";
    return;
  }
  
  setCursor(0, row);
  // Print text padded to full line width (clear any remaining chars)
  String padded = text;
  while (padded.length() < cols) {
    padded += " ";
  }
  print(padded.substring(0, cols));
}

void LCDManager::clearLine(uint8_t row) {
  if (!safeOperation("clearLine")) return;
  printLine(row, "");
}

void LCDManager::printLineCenter(uint8_t row, const String& text) {
  if (!safeOperation("printLineCenter")) return;
  if (row >= rows) {
    lastError = "Row out of bounds";
    return;
  }
  
  // Calculate center position
  int startCol = (cols - text.length()) / 2;
  if (startCol < 0) startCol = 0;
  
  String padded = "";
  // Add leading spaces
  for (int i = 0; i < startCol; i++) padded += " ";
  padded += text;
  // Add trailing spaces
  while (padded.length() < cols) padded += " ";
  
  printLine(row, padded.substring(0, cols));
}

void LCDManager::printLineRight(uint8_t row, const String& text) {
  if (!safeOperation("printLineRight")) return;
  if (row >= rows) {
    lastError = "Row out of bounds";
    return;
  }
  
  // Calculate right-aligned position
  int startCol = cols - text.length();
  if (startCol < 0) startCol = 0;
  
  String padded = "";
  // Add leading spaces
  for (int i = 0; i < startCol; i++) padded += " ";
  padded += text;
  
  printLine(row, padded.substring(0, cols));
}

void LCDManager::backlight() {
  if (!safeOperation("backlight")) return;
  lcd.backlight();
  backlight_state = true;
}

void LCDManager::noBacklight() {
  if (!safeOperation("noBacklight")) return;
  lcd.noBacklight();
  backlight_state = false;
}

void LCDManager::setBacklight(bool state) {
  if (!safeOperation("setBacklight")) return;
  if (state) {
    backlight();
  } else {
    noBacklight();
  }
}

void LCDManager::noCursor() {
  if (!safeOperation("noCursor")) return;
  lcd.noCursor();
}

void LCDManager::cursor() {
  if (!safeOperation("cursor")) return;
  lcd.cursor();
}

void LCDManager::noBlink() {
  if (!safeOperation("noBlink")) return;
  lcd.noBlink();
}

void LCDManager::blink() {
  if (!safeOperation("blink")) return;
  lcd.blink();
}

void LCDManager::display() {
  if (!safeOperation("display")) return;
  lcd.display();
}

void LCDManager::noDisplay() {
  if (!safeOperation("noDisplay")) return;
  lcd.noDisplay();
}

void LCDManager::createChar(uint8_t location, uint8_t charmap[8]) {
  if (!safeOperation("createChar")) return;
  if (location > 7) {
    lastError = "Custom char location must be 0-7";
    return;
  }
  lcd.createChar(location, charmap);
}

void LCDManager::autoscroll() {
  if (!safeOperation("autoscroll")) return;
  lcd.autoscroll();
}

void LCDManager::noAutoscroll() {
  if (!safeOperation("noAutoscroll")) return;
  lcd.noAutoscroll();
}

void LCDManager::leftToRight() {
  if (!safeOperation("leftToRight")) return;
  lcd.leftToRight();
}

void LCDManager::rightToLeft() {
  if (!safeOperation("rightToLeft")) return;
  lcd.rightToLeft();
}

bool LCDManager::isHealthy() {
  if (!initialized) return false;
  
  // Try to ping the LCD via I2C
  // LCD should respond to address scan
  return I2CManager::getInstance().isDisplayBusHealthy();
}
