#ifndef TWI_BOOT_UPDATER_H
#define TWI_BOOT_UPDATER_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>

// Twiboot bootloader I2C address (slave)
#define TWIBOOT_I2C_ADDR 0x29

// Application I2C address (activates bootloader)
#define APP_I2C_ADDR 0x30

// I2C Command to enter bootloader mode
#define APP_BOOTLOADER_COMMAND 0x42  // ASCII 'B'

// Twiboot commands (from bootloader protocol)
enum TwiBootCommand {
  TWIBOOT_READ_VERSION = 0x01,
  TWIBOOT_READ_MEMORY = 0x02,
  TWIBOOT_WRITE_MEMORY = 0x03,
  TWIBOOT_READ_FLASH = 0x04,
  TWIBOOT_READ_EEPROM = 0x05,
  TWIBOOT_WRITE_FLASH = 0x06,
  TWIBOOT_WRITE_EEPROM = 0x07,
  TWIBOOT_READ_SIGNATURE = 0x08
};

// Bootloader response status
enum TwiBootStatus {
  BOOT_OK = 0x00,
  BOOT_ERROR = 0xFF
};

class TwiBootUpdater {
public:
  TwiBootUpdater();
  
  // Request app to enter bootloader mode
  bool requestBootloaderMode();
  
  // Query bootloader version
  bool queryBootloaderVersion(String& version);
  
  // Query chip signature
  bool queryChipSignature(uint8_t& sig0, uint8_t& sig1, uint8_t& sig2);
  
  // Upload Intel HEX firmware file
  bool uploadHexFile(const String& hexContent, void (*progressCallback)(int percent) = nullptr);
  
  // Write memory via bootloader (for testing)
  bool writeMemory(uint16_t address, const uint8_t* data, uint16_t length);
  
  // Read memory via bootloader (for testing)
  bool readMemory(uint16_t address, uint8_t* buffer, uint16_t length);
  
  // Get last error message
  String getLastError() { return lastError; }
  
private:
  String lastError;
  
  // Helper: Send I2C command to bootloader
  bool sendBootloaderCommand(TwiBootCommand cmd, const uint8_t* data = nullptr, 
                             uint16_t dataLen = 0, uint8_t* response = nullptr, 
                             uint16_t* responseLen = nullptr);
  
  // Helper: Parse Intel HEX line
  struct HexRecord {
    uint16_t address;
    uint8_t recordType;  // 0=data, 1=EOF, 2=extended segment, 4=extended linear
    std::vector<uint8_t> data;
  };
  
  bool parseHexLine(const String& line, HexRecord& record);
  
  // Helper: Calculate checksum for hex record
  uint8_t calculateHexChecksum(const String& line);
};

#endif // TWI_BOOT_UPDATER_H
