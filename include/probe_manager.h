#ifndef PROBE_MANAGER_H
#define PROBE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "i2c_manager.h"

/**
 * Probe Manager - Consolidated Temperature Measurement System
 * 
 * Singleton pattern for consistent probe access across application
 * Aggregates temperature from multiple sources:
 * - ADS1110 ADC (analog temperature sensors) at 0x48-0x4B
 * - AHT10 Temperature & Humidity Sensor (0x38)
 * - MS11-control slave temperature (if implemented, via I2C Bus 0)
 * 
 * Hardware: Multi-probe support on dual I2C buses
 * - Bus 0 (GPIO8/9): ADS1110 ADC, optional existing sensors
 * - Bus 1 (GPIO5/6): Slave controller (MS11-control) for remote temperature
 * 
 * Features:
 * - Non-blocking temperature reads from all available sources
 * - Per-probe calibration support (offset/scale factors)
 * - Error handling and sensor health checks
 * - Last valid value caching for failed reads
 * - Timestamp tracking for data freshness
 */

// Probe type enumeration
enum class ProbeType {
  UNKNOWN = 0,
  ADS1110,           // 16-bit ADC (TI)
  AHT10,             // Temperature & Humidity (Aosong)
  MS11_CONTROL_TEMP, // Remote temperature from MS11-control slave
};

// Probe data structure
struct ProbeData {
  ProbeType type;
  uint8_t i2c_address;
  uint8_t bus_number;
  float temperature;        // Last read temperature (°C)
  float humidity;           // Humidity (if sensor supports, else 0)
  uint32_t last_read_ms;    // Last successful read timestamp
  bool initialized;
  bool healthy;
  String name;
  
  // Calibration factors
  float temp_offset;        // Offset: temp_actual = temp_raw + offset
  float temp_scale;         // Scale: temp_actual = temp_raw * scale
};

class ProbeManager {
public:
  // Singleton instance accessor
  static ProbeManager& getInstance() {
    static ProbeManager instance;
    return instance;
  }

  // Initialization
  bool begin();
  void end();

  // Probe discovery and management
  bool scanAndDetectProbes();   // Scan I2C buses and detect available temperature sensors
  uint8_t getProbeCount() const { return probe_count; }
  ProbeData* getProbe(uint8_t index);
  ProbeData* getProbeByAddress(uint8_t address);
  ProbeData* getProbeByType(ProbeType type);

  // Temperature readings
  bool readAllProbes();                    // Non-blocking read from all probes
  bool readProbe(uint8_t index);           // Read specific probe by index
  float getTemperature(uint8_t index);     // Get last temperature from probe
  float getHumidity(uint8_t index);        // Get humidity (if available)
  uint32_t getLastReadTime(uint8_t index); // Get last read timestamp

  // Averaged temperature
  float getAverageTemeprature(bool exclude_ms11 = false);  // Average of all probes (optional exclude MS11)

  // Per-probe calibration
  bool setProbeCalibration(uint8_t index, float offset, float scale);
  bool getProbeCalibration(uint8_t index, float& offset, float& scale);

  // Calibration persistence (NVS + LittleFS)
  bool initializeCalibrationFromNVS();    // Load calibration from NVS; if empty, save defaults
  bool syncCalibrationFromLittleFS();     // Load probe_cal.txt and sync NVS if update_nvs=1
  bool saveCalibrationToLittleFS();       // Write current probe data + calibration to probe_cal.txt
  bool saveCalibrationToNVS();            // Write all probe calibration to NVS
  bool loadCalibrationFromNVS();          // Load all probe calibration from NVS

  // Status and health
  bool isInitialized() const { return initialized; }
  bool isHealthy();                        // All probes responding
  String getLastError() const { return lastError; }

  // Debug/monitoring
  void printProbeStatus();                 // Serial output of all probes and readings

private:
  ProbeManager();  // Private constructor
  ~ProbeManager();
  
  // Delete copy constructors
  ProbeManager(const ProbeManager&) = delete;
  ProbeManager& operator=(const ProbeManager&) = delete;

  // Internal probe management
  bool initializeADS1110(uint8_t address, uint8_t bus);
  bool initializeAHT10();
  bool initializeMS11ControlTemp();

  bool readADS1110(ProbeData& probe);
  bool readAHT10(ProbeData& probe);
  bool readMS11ControlTemp(ProbeData& probe);

  // State tracking
  bool initialized = false;
  String lastError;
  
  // Probes storage (max 8 probes)
  static constexpr uint8_t MAX_PROBES = 8;
  ProbeData probes[MAX_PROBES];
  uint8_t probe_count = 0;

  // I2C addresses and buses
  static constexpr uint8_t ADC_ADDRESS_BASE = 0x48;  // 0x48-0x4B possible
  static constexpr uint8_t TEMP_SENSOR_ADDRESS = 0x38;
  // MS11-control: 0x30 on Bus 1 (temperature register TBD)
  static constexpr uint8_t SLAVE_TEMP_ADDRESS = 0x30;
};

#endif // PROBE_MANAGER_H
