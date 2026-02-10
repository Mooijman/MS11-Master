#include "aht10_manager.h"
#include "Adafruit_AHTX0.h"
#include "i2c_manager.h"

// Global AHT10 sensor instance
Adafruit_AHTX0 gAHT10;
bool gAHT10Ready = false;

AHT10Manager::AHT10Manager() {
  // Constructor - initialization handled in begin()
}

AHT10Manager::~AHT10Manager() {
  end();
}

bool AHT10Manager::begin() {
  if (initialized) {
    Serial.println("[AHT10] Already initialized");
    return true;
  }

  // Ensure I2C Manager is initialized
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "I2C Manager not initialized";
      Serial.println("[AHT10] ERROR: " + lastError);
      return false;
    }
  }

  // Verify AHT10 device is present (using I2CManager ping on display bus)
  if (!I2CManager::getInstance().ping(AHT10_I2C_ADDRESS, I2C_BUS_DISPLAY)) {
    lastError = "AHT10 sensor not found at 0x38";
    Serial.println("[AHT10] WARNING: " + lastError);
    // Don't fail - sensor might be powered down or not present, but allow operation
    return false;
  }

  // Initialize AHT10 using default Wire (which is Bus 1 for display devices)
  if (!gAHT10.begin()) {
    lastError = "AHT10 initialization failed";
    Serial.println("[AHT10] WARNING: " + lastError);
    return false;
  }

  initialized = true;
  gAHT10Ready = true;
  
  // Read sensor immediately on startup to have valid data
  readSensor();
  
  Serial.println("[AHT10] ✓ Temperature & Humidity Sensor initialized (I2C Bus 1: 0x" + 
                 String(AHT10_I2C_ADDRESS, HEX) + ")");
  Serial.println("[AHT10] Initial reading: " + String(temperature, 1) + "°C, " + 
                 String(humidity, 0) + "%");
  
  return true;
}

void AHT10Manager::end() {
  if (initialized) {
    gAHT10Ready = false;
    initialized = false;
    Serial.println("[AHT10] Sensor shutdown");
  }
}

bool AHT10Manager::readSensor() {
  if (!initialized || !gAHT10Ready) {
    return false;  // Silently return if not ready
  }

  // Get sensor data
  sensors_event_t humidityEvent, tempEvent;
  
  if (!gAHT10.getEvent(&humidityEvent, &tempEvent)) {
    lastError = "Failed to read sensor data";
    return false;  // Keep last valid values
  }

  // Update readings and timestamp
  temperature = tempEvent.temperature;
  humidity = humidityEvent.relative_humidity;
  lastReadTime = millis();

  return true;
}

float AHT10Manager::getTemperature() {
  return temperature;
}

float AHT10Manager::getHumidity() {
  return humidity;
}

uint32_t AHT10Manager::getLastReadTime() {
  return lastReadTime;
}

bool AHT10Manager::isHealthy() {
  if (!initialized) {
    return false;
  }

  // Try a test read
  sensors_event_t humidityEvent, tempEvent;
  return gAHT10.getEvent(&humidityEvent, &tempEvent);
}
