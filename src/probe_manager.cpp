#include "probe_manager.h"
#include "aht10_manager.h"
#include "slave_controller.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

ProbeManager::ProbeManager() {
  // Constructor - initialization handled in begin()
}

ProbeManager::~ProbeManager() {
  end();
}

bool ProbeManager::begin() {
  if (initialized) {
    return true;
  }

  // Ensure I2C Manager is initialized
  if (!I2CManager::getInstance().isInitialized()) {
    if (!I2CManager::getInstance().begin()) {
      lastError = "I2C Manager not initialized";
      return false;
    }
  }

  probe_count = 0;

  // Scan and detect all available temperature probes
  scanAndDetectProbes();

  // Initialize calibration from NVS/LittleFS
  if (probe_count > 0) {
    
    // Step 1: Check NVS and load or save defaults
    initializeCalibrationFromNVS();
    loadCalibrationFromNVS();
    
    // Step 2: Check LittleFS and sync if needed
    syncCalibrationFromLittleFS();
    
    // Step 3: Save current state to LittleFS
    if (!saveCalibrationToLittleFS()) {
      // Could not save to probe_cal.txt
    }
  }

  initialized = true;

  return probe_count > 0;  // Success if at least one probe detected
}

void ProbeManager::end() {
  if (initialized) {
    probe_count = 0;
    initialized = false;
  }
}

bool ProbeManager::scanAndDetectProbes() {
  // Scan for ADS1110 ADC on Bus 0 (addresses 0x48-0x4B)
  
  for (uint8_t addr = 0x48; addr <= 0x4B; addr++) {
    if (I2CManager::getInstance().ping(addr, I2C_BUS_DISPLAY)) {
      
      if (probe_count < MAX_PROBES) {
        if (initializeADS1110(addr, I2C_BUS_DISPLAY)) {
          probe_count++;
        }
      }
    }
  }

  // Try to integrate AHT10 temperature sensor
  if (AHT10Manager::getInstance().isInitialized() && probe_count < MAX_PROBES) {
    if (initializeAHT10()) {
      probe_count++;
    }
  }

  // Try to get temperature from MS11-control slave (if implemented)
  if (SlaveController::getInstance().ping() && probe_count < MAX_PROBES) {
    if (initializeMS11ControlTemp()) {
      probe_count++;
    }
  }

  return probe_count > 0;
}

bool ProbeManager::initializeADS1110(uint8_t address, uint8_t bus) {
  if (probe_count >= MAX_PROBES) return false;

  ProbeData& probe = probes[probe_count];
  probe.type = ProbeType::ADS1110;
  probe.i2c_address = address;
  probe.bus_number = bus;
  probe.temperature = 0.0f;
  probe.humidity = 0.0f;
  probe.initialized = true;
  probe.healthy = true;
  probe.temp_offset = PROBE_CAL_ADS1110_OFFSET;  // From config.h
  probe.temp_scale = 1.0f;
  probe.last_read_ms = millis();
  
  probe.name = "ADS1110 ADC (0x" + String(address, HEX) + ")";
  return true;
}



bool ProbeManager::initializeAHT10() {
  if (probe_count >= MAX_PROBES) return false;

  ProbeData& probe = probes[probe_count];
  probe.type = ProbeType::AHT10;
  probe.i2c_address = TEMP_SENSOR_ADDRESS;
  probe.bus_number = I2C_BUS_DISPLAY;
  probe.temperature = AHT10Manager::getInstance().getTemperature();
  probe.humidity = AHT10Manager::getInstance().getHumidity();
  probe.initialized = true;
  probe.healthy = AHT10Manager::getInstance().isHealthy();
  probe.temp_offset = PROBE_CAL_AHT10_OFFSET;    // From config.h
  probe.temp_scale = 1.0f;
  probe.last_read_ms = AHT10Manager::getInstance().getLastReadTime();
  
  probe.name = "AHT10 Temperature & Humidity Sensor (0x38)";
  return true;
}

bool ProbeManager::initializeMS11ControlTemp() {
  if (probe_count >= MAX_PROBES) return false;

  ProbeData& probe = probes[probe_count];
  probe.type = ProbeType::MS11_CONTROL_TEMP;
  probe.i2c_address = SLAVE_I2C_ADDR;
  probe.bus_number = SLAVE_I2C_BUS;
  probe.temperature = 0.0f;
  probe.humidity = 0.0f;
  probe.initialized = true;
  probe.healthy = true;  // Will be updately when read attempts start
  probe.temp_offset = PROBE_CAL_MS11_OFFSET;     // From config.h
  probe.temp_scale = 1.0f;
  probe.last_read_ms = 0;
  
  probe.name = "MS11-control Temperature (0x30) - DS Sensor";
  return true;
}

bool ProbeManager::readAllProbes() {
  bool any_success = false;

  for (uint8_t i = 0; i < probe_count; i++) {
    if (readProbe(i)) {
      any_success = true;
    }
  }

  return any_success;
}

bool ProbeManager::readProbe(uint8_t index) {
  if (index >= probe_count) {
    lastError = "Probe index out of range";
    return false;
  }

  ProbeData& probe = probes[index];

  if (!probe.initialized) {
    return false;
  }

  bool success = false;

  switch (probe.type) {
    case ProbeType::ADS1110:
      success = readADS1110(probe);
      break;
    case ProbeType::AHT10:
      success = readAHT10(probe);
      break;
    case ProbeType::MS11_CONTROL_TEMP:
      success = readMS11ControlTemp(probe);
      break;
    default:
      success = false;
      break;
  }

  if (success) {
    probe.last_read_ms = millis();
    probe.healthy = true;
  } else {
    probe.healthy = false;
  }

  return success;
}

bool ProbeManager::readADS1110(ProbeData& probe) {
  // TODO: Implement ADS1110 ADC reading
  // - Read raw ADC value from ADS1110 at probe.i2c_address
  // - Convert to temperature based on sensor calibration
  // - Store in probe.temperature
  // - For now: stub implementation
  
  // ADS1110 read not yet implemented
  return false;
}



bool ProbeManager::readAHT10(ProbeData& probe) {
  // Read from AHT10Manager singleton
  if (!AHT10Manager::getInstance().readSensor()) {
    lastError = "AHT10 read failed";
    return false;
  }

  // Update probe data from AHT10Manager
  probe.temperature = AHT10Manager::getInstance().getTemperature();
  probe.humidity = AHT10Manager::getInstance().getHumidity();

  // Apply calibration
  probe.temperature = (probe.temperature * probe.temp_scale) + probe.temp_offset;

  return true;
}

bool ProbeManager::readMS11ControlTemp(ProbeData& probe) {
  // Read system temperature from MS11-control slave (DS temperature sensor)
  int16_t temp_raw = 0;
  
  if (!SlaveController::getInstance().readSystemTemp(temp_raw)) {
    lastError = "Failed to read MS11-control temperature";
    return false;
  }
  
  // Convert from DS18B20 fixed-point format (Q8.8)
  // High byte = integer graden, low byte = fractie (0.00390625 per LSB)
  // Divide by 256 to get float value
  float temp_f = (float)temp_raw / 256.0f;
  
  // Apply calibration
  probe.temperature = temp_f * probe.temp_scale + probe.temp_offset;
  
  return true;
}

ProbeData* ProbeManager::getProbe(uint8_t index) {
  if (index >= probe_count) {
    return nullptr;
  }
  return &probes[index];
}

ProbeData* ProbeManager::getProbeByAddress(uint8_t address) {
  for (uint8_t i = 0; i < probe_count; i++) {
    if (probes[i].i2c_address == address) {
      return &probes[i];
    }
  }
  return nullptr;
}

ProbeData* ProbeManager::getProbeByType(ProbeType type) {
  for (uint8_t i = 0; i < probe_count; i++) {
    if (probes[i].type == type) {
      return &probes[i];
    }
  }
  return nullptr;
}

float ProbeManager::getTemperature(uint8_t index) {
  if (index >= probe_count) {
    return 0.0f;
  }
  return probes[index].temperature;
}

float ProbeManager::getHumidity(uint8_t index) {
  if (index >= probe_count) {
    return 0.0f;
  }
  return probes[index].humidity;
}

uint32_t ProbeManager::getLastReadTime(uint8_t index) {
  if (index >= probe_count) {
    return 0;
  }
  return probes[index].last_read_ms;
}

float ProbeManager::getAverageTemeprature(bool exclude_ms11) {
  if (probe_count == 0) {
    return 0.0f;
  }

  float sum = 0.0f;
  uint8_t count = 0;

  for (uint8_t i = 0; i < probe_count; i++) {
    if (exclude_ms11 && probes[i].type == ProbeType::MS11_CONTROL_TEMP) {
      continue;
    }
    if (probes[i].initialized && probes[i].healthy) {
      sum += probes[i].temperature;
      count++;
    }
  }

  return count > 0 ? sum / count : 0.0f;
}

bool ProbeManager::setProbeCalibration(uint8_t index, float offset, float scale) {
  if (index >= probe_count) {
    lastError = "Probe index out of range";
    return false;
  }

  probes[index].temp_offset = offset;
  probes[index].temp_scale = scale;
  return true;
}

bool ProbeManager::getProbeCalibration(uint8_t index, float& offset, float& scale) {
  if (index >= probe_count) {
    lastError = "Probe index out of range";
    return false;
  }

  offset = probes[index].temp_offset;
  scale = probes[index].temp_scale;
  return true;
}

bool ProbeManager::isHealthy() {
  for (uint8_t i = 0; i < probe_count; i++) {
    if (!probes[i].healthy) {
      return false;
    }
  }
  return true;
}

void ProbeManager::printProbeStatus() {
  // Debug status printing disabled
}

// ============================================================================
// CALIBRATION PERSISTENCE - NVS & LITTLEFS
// ============================================================================

bool ProbeManager::initializeCalibrationFromNVS() {
  // Initialize Preferences for NVS access
  Preferences prefs;
  prefs.begin("probe_cal", false);  // false = read/write mode
  
  
  // Check if NVS has any probe calibration
  bool has_nvs_data = prefs.isKey("initialized");
  
  if (!has_nvs_data) {
    // Get defaults from config.h for each probe type
    // These will be saved after probes are detected
    // For now, just mark as initialized
    prefs.putBool("initialized", true);
    prefs.end();
    
    return false;  // Signal that defaults were just saved
  }
  prefs.end();
  
  return loadCalibrationFromNVS();
}

bool ProbeManager::loadCalibrationFromNVS() {
  Preferences prefs;
  prefs.begin("probe_cal", true);  // true = read-only mode
  
  // Load calibration for each probe (stored as "probe_N_offset" and "probe_N_scale")
  for (uint8_t i = 0; i < probe_count; i++) {
    String offset_key = "probe_" + String(i) + "_offset";
    String scale_key = "probe_" + String(i) + "_scale";
    
    if (prefs.isKey(offset_key.c_str()) && prefs.isKey(scale_key.c_str())) {
      probes[i].temp_offset = prefs.getFloat(offset_key.c_str(), 0.0f);
      probes[i].temp_scale = prefs.getFloat(scale_key.c_str(), 1.0f);
    }
  }
  
  prefs.end();
  return true;
}

bool ProbeManager::saveCalibrationToNVS() {
  Preferences prefs;
  prefs.begin("probe_cal", false);  // false = read/write mode
  
  
  // Save calibration for each probe
  for (uint8_t i = 0; i < probe_count; i++) {
    String offset_key = "probe_" + String(i) + "_offset";
    String scale_key = "probe_" + String(i) + "_scale";
    String type_key = "probe_" + String(i) + "_type";
    
    prefs.putFloat(offset_key.c_str(), probes[i].temp_offset);
    prefs.putFloat(scale_key.c_str(), probes[i].temp_scale);
    prefs.putInt(type_key.c_str(), (int)probes[i].type);
  }
  
  prefs.putBool("initialized", true);
  prefs.putULong("last_save", millis());
  prefs.end();
  
  return true;
}

bool ProbeManager::syncCalibrationFromLittleFS() {
  
  if (!LittleFS.exists("/littlefs/probe_cal.txt")) {
    return false;
  }

  File file = LittleFS.open("/littlefs/probe_cal.txt", "r");
  if (!file) {
    return false;
  }

  // Parse JSON from file
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    return false;
  }

  // Check update_nvs flag
  if (!doc.containsKey("update_nvs")) {
    return false;
  }

  bool update_nvs = doc["update_nvs"].as<bool>();

  if (!update_nvs) {
    return true;  // File exists but no sync needed
  }

  // Sync: Load calibration from probe_cal.txt into memory
  
  if (doc.containsKey("probes") && doc["probes"].is<JsonArray>()) {
    JsonArray probes_array = doc["probes"];
    
    for (uint8_t i = 0; i < probes_array.size() && i < probe_count; i++) {
      JsonObject probe_obj = probes_array[i];
      
      if (probe_obj.containsKey("offset") && probe_obj.containsKey("scale")) {
        float offset = probe_obj["offset"].as<float>();
        float scale = probe_obj["scale"].as<float>();
        
        probes[i].temp_offset = offset;
        probes[i].temp_scale = scale;
      }
    }
  }

  // Write synced calibration back to NVS
  if (!saveCalibrationToNVS()) {
    return false;
  }

  // Set update_nvs=0 in probe_cal.txt to prevent re-syncing on next boot
  doc["update_nvs"] = false;
  File out_file = LittleFS.open("/littlefs/probe_cal.txt", "w");
  serializeJson(doc, out_file);
  out_file.close();

  return true;
}

bool ProbeManager::saveCalibrationToLittleFS() {
  
  // Create JSON document
  DynamicJsonDocument doc(2048);
  doc["version"] = "2026.2.12";
  doc["timestamp"] = millis();
  doc["update_nvs"] = false;  // Flag for next boot (set to 1 if manual edit)
  
  // Add all probes to JSON
  JsonArray probes_array = doc.createNestedArray("probes");
  
  for (uint8_t i = 0; i < probe_count; i++) {
    JsonObject probe_obj = probes_array.createNestedObject();
    probe_obj["index"] = i;
    probe_obj["name"] = probes[i].name;
    probe_obj["address"] = "0x" + String(probes[i].i2c_address, HEX);
    probe_obj["type"] = (int)probes[i].type;
    probe_obj["offset"] = serialized(String(probes[i].temp_offset, 3));
    probe_obj["scale"] = serialized(String(probes[i].temp_scale, 3));
    probe_obj["temperature"] = serialized(String(probes[i].temperature, 2));
    probe_obj["healthy"] = probes[i].healthy;
  }

  // Write to file
  File file = LittleFS.open("/littlefs/probe_cal.txt", "w");
  if (!file) {
    return false;
  }

  size_t bytes_written = serializeJson(doc, file);
  file.close();

  return true;
}

