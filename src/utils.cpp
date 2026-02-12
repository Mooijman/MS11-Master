#include "utils.h"
#include "config.h"
#include "LittleFS.h"

// ============================================================================
// LittleFS Initialization
// ============================================================================

void initLittleFS() {
  if (!LittleFS.begin(true, LITTLEFS_BASE_PATH, LITTLEFS_MAX_FILES, LITTLEFS_PARTITION_LABEL)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// ============================================================================
// JSON Helpers
// ============================================================================

String jsonEscape(const String& str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '"') {
      result += "\\\"";
    } else if (c == '\\') {
      result += "\\\\";
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else {
      result += c;
    }
  }
  return result;
}

// ============================================================================
// Blink State Helper
// ============================================================================

bool blinkState(unsigned long currentMillis, unsigned long onMs, unsigned long offMs) {
  unsigned long cycle = onMs + offMs;
  return (currentMillis % cycle) < onMs;
}
