#include "twi_boot_updater.h"

TwiBootUpdater::TwiBootUpdater() {
  lastError = "";
}

bool TwiBootUpdater::requestBootloaderMode() {
  // Send bootloader activation command to app at 0x30
  // This tells app to set EEPROM flag and reboot
  
  Serial.println("[TwiBootUpdater] Requesting bootloader mode...");
  
  Wire.beginTransmission(APP_I2C_ADDR);
  Wire.write(APP_BOOTLOADER_COMMAND);  // Send 0x42 ('B')
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    lastError = "Failed to send bootloader command to app (0x" + String(APP_I2C_ADDR, HEX) + ")";
    Serial.println("[TwiBootUpdater] ERROR: " + lastError);
    return false;
  }
  
  Serial.println("[TwiBootUpdater] Bootloader command sent. Waiting for app to reboot...");
  
  // Wait for app to reboot and bootloader to start
  delay(2000);
  
  // Verify bootloader is active by querying version
  String version;
  if (!queryBootloaderVersion(version)) {
    lastError = "Bootloader did not respond after reboot";
    Serial.println("[TwiBootUpdater] ERROR: " + lastError);
    return false;
  }
  
  Serial.println("[TwiBootUpdater] Bootloader active! Version: " + version);
  return true;
}

bool TwiBootUpdater::queryBootloaderVersion(String& version) {
  Serial.println("[TwiBootUpdater] Querying bootloader version...");
  
  uint8_t response[16];
  uint16_t responseLen = sizeof(response);
  
  if (!sendBootloaderCommand(TWIBOOT_READ_VERSION, nullptr, 0, response, &responseLen)) {
    lastError = "Failed to query bootloader version";
    return false;
  }
  
  if (responseLen < 4) {
    lastError = "Invalid version response length";
    return false;
  }
  
  // Response format: major, minor, features_low, features_high
  version = String(response[0]) + "." + String(response[1]);
  Serial.println("[TwiBootUpdater] Bootloader version: " + version);
  
  return true;
}

bool TwiBootUpdater::queryChipSignature(uint8_t& sig0, uint8_t& sig1, uint8_t& sig2) {
  Serial.println("[TwiBootUpdater] Querying chip signature...");
  
  uint8_t response[16];
  uint16_t responseLen = sizeof(response);
  
  if (!sendBootloaderCommand(TWIBOOT_READ_SIGNATURE, nullptr, 0, response, &responseLen)) {
    lastError = "Failed to query chip signature";
    return false;
  }
  
  if (responseLen < 3) {
    lastError = "Invalid signature response length";
    return false;
  }
  
  sig0 = response[0];
  sig1 = response[1];
  sig2 = response[2];
  
  Serial.printf("[TwiBootUpdater] Chip signature: %02X %02X %02X\n", sig0, sig1, sig2);
  
  return true;
}

bool TwiBootUpdater::uploadHexFile(const String& hexContent, void (*progressCallback)(int percent)) {
  Serial.println("[TwiBootUpdater] Starting hex file upload...");
  
  // Parse hex file line by line
  int lineCount = 0;
  int errorCount = 0;
  uint32_t totalBytes = 0;
  uint16_t baseAddress = 0x0000;  // Extended linear address
  bool inBootloaderSection = false;
  
  // Split hex content into lines
  int startIdx = 0;
  String currentLine;
  
  while (startIdx < hexContent.length()) {
    int endIdx = hexContent.indexOf('\n', startIdx);
    if (endIdx == -1) endIdx = hexContent.length();
    
    currentLine = hexContent.substring(startIdx, endIdx);
    currentLine.trim();
    startIdx = endIdx + 1;
    
    if (currentLine.length() == 0) continue;
    if (currentLine[0] != ':') continue;  // Skip non-hex lines
    
    lineCount++;
    
    HexRecord record;
    if (!parseHexLine(currentLine, record)) {
      Serial.println("[TwiBootUpdater] ERROR: Invalid hex line " + String(lineCount));
      errorCount++;
      continue;
    }
    
    // Handle record types
    if (record.recordType == 0x00) {
      // Data record
      uint32_t fullAddress = (baseAddress << 16) | record.address;
      
      // Check if address is in bootloader section (0x7C00 for ATmega328P)
      // Skip bootloader section (don't overwrite it)
      if (fullAddress >= 0x7C00 && fullAddress < 0x8000) {
        Serial.println("[TwiBootUpdater] Skipping bootloader section at 0x" + String(fullAddress, HEX));
        inBootloaderSection = true;
        continue;
      }
      
      // Write data in chunks (Twiboot max is 16 bytes per write)
      const int CHUNK_SIZE = 16;
      for (size_t i = 0; i < record.data.size(); i += CHUNK_SIZE) {
        size_t chunkLen = min((size_t)CHUNK_SIZE, record.data.size() - i);
        uint32_t chunkAddr = fullAddress + i;
        
        if (!writeMemory(chunkAddr, &record.data[i], chunkLen)) {
          Serial.println("[TwiBootUpdater] ERROR: Failed to write at 0x" + String(chunkAddr, HEX));
          errorCount++;
          break;
        }
        
        totalBytes += chunkLen;
        
        // Progress callback every 256 bytes
        if (progressCallback && (totalBytes % 256 == 0)) {
          progressCallback(totalBytes / 10);  // Rough estimation
        }
      }
      
    } else if (record.recordType == 0x04) {
      // Extended linear address record
      if (record.data.size() >= 2) {
        baseAddress = (record.data[0] << 8) | record.data[1];
        Serial.println("[TwiBootUpdater] Extended address: 0x" + String(baseAddress, HEX));
      }
      
    } else if (record.recordType == 0x01) {
      // End of file
      Serial.println("[TwiBootUpdater] EOF reached at line " + String(lineCount));
      break;
    }
  }
  
  if (errorCount > 0) {
    lastError = "Upload failed with " + String(errorCount) + " errors";
    return false;
  }
  
  Serial.println("[TwiBootUpdater] Upload complete! Wrote " + String(totalBytes) + " bytes");
  return true;
}

bool TwiBootUpdater::parseHexLine(const String& line, HexRecord& record) {
  if (line.length() < 11) return false;
  if (line[0] != ':') return false;
  
  // Parse: :LLAAAATTDD...CC
  // LL = byte count, AAAA = address, TT = record type, DD = data, CC = checksum
  
  record.data.clear();
  
  // Byte count
  uint8_t byteCount = strtol(line.substring(1, 3).c_str(), nullptr, 16);
  
  // Address
  record.address = strtol(line.substring(3, 7).c_str(), nullptr, 16);
  
  // Record type
  record.recordType = strtol(line.substring(7, 9).c_str(), nullptr, 16);
  
  // Data bytes
  for (int i = 0; i < byteCount; i++) {
    int pos = 9 + (i * 2);
    if (pos + 2 > line.length()) return false;
    
    uint8_t byte = strtol(line.substring(pos, pos + 2).c_str(), nullptr, 16);
    record.data.push_back(byte);
  }
  
  return true;
}

bool TwiBootUpdater::writeMemory(uint16_t address, const uint8_t* data, uint16_t length) {
  Serial.printf("[TwiBootUpdater] Writing %d bytes to 0x%04X\n", length, address);
  
  // Twiboot write command: [CMD] [ADDR_HIGH] [ADDR_LOW] [LEN] [DATA...]
  uint8_t cmdData[20];
  cmdData[0] = (address >> 8) & 0xFF;
  cmdData[1] = address & 0xFF;
  cmdData[2] = length & 0xFF;
  
  // Copy data
  for (int i = 0; i < length && i < 16; i++) {
    cmdData[3 + i] = data[i];
  }
  
  uint8_t response[1];
  uint16_t responseLen = 1;
  
  if (!sendBootloaderCommand(TWIBOOT_WRITE_FLASH, cmdData, 3 + length, response, &responseLen)) {
    lastError = "Write memory failed";
    return false;
  }
  
  if (responseLen < 1 || response[0] != BOOT_OK) {
    lastError = "Bootloader returned error";
    return false;
  }
  
  return true;
}

bool TwiBootUpdater::readMemory(uint16_t address, uint8_t* buffer, uint16_t length) {
  Serial.printf("[TwiBootUpdater] Reading %d bytes from 0x%04X\n", length, address);
  
  uint8_t cmdData[3];
  cmdData[0] = (address >> 8) & 0xFF;
  cmdData[1] = address & 0xFF;
  cmdData[2] = length & 0xFF;
  
  if (!sendBootloaderCommand(TWIBOOT_READ_FLASH, cmdData, 3, buffer, &length)) {
    lastError = "Read memory failed";
    return false;
  }
  
  return true;
}

bool TwiBootUpdater::sendBootloaderCommand(TwiBootCommand cmd, const uint8_t* data,
                                          uint16_t dataLen, uint8_t* response,
                                          uint16_t* responseLen) {
  // Send command to bootloader via I2C
  Wire.beginTransmission(TWIBOOT_I2C_ADDR);
  Wire.write((uint8_t)cmd);
  
  if (data && dataLen > 0) {
    Wire.write(data, dataLen);
  }
  
  uint8_t error = Wire.endTransmission(false);  // Keep connection for reading
  if (error != 0) {
    lastError = "I2C transmission failed";
    return false;
  }
  
  // Request response from bootloader
  if (response && responseLen && *responseLen > 0) {
    Wire.requestFrom(TWIBOOT_I2C_ADDR, *responseLen);
    
    int bytesRead = 0;
    while (Wire.available() && bytesRead < *responseLen) {
      response[bytesRead++] = Wire.read();
    }
    
    *responseLen = bytesRead;
  }
  
  return true;
}

uint8_t TwiBootUpdater::calculateHexChecksum(const String& line) {
  // Calculate checksum for Intel HEX format
  uint8_t sum = 0;
  
  // Skip the ':' at the beginning
  for (int i = 1; i < line.length() - 2; i += 2) {
    uint8_t byte = strtol(line.substring(i, i + 2).c_str(), nullptr, 16);
    sum += byte;
  }
  
  return (~sum + 1) & 0xFF;
}
