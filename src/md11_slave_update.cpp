#include "md11_slave_update.h"

MD11SlaveUpdate::MD11SlaveUpdate() {
  lastError = "";
}

bool MD11SlaveUpdate::requestBootloaderMode() {
  // Send bootloader activation command to app at 0x30
  // This tells app to set EEPROM flag and reboot
  
  Serial.println("[MD11SlaveUpdate] Requesting bootloader mode...");
  
  Wire.beginTransmission(APP_I2C_ADDR);
  Wire.write(APP_BOOTLOADER_COMMAND);  // Send 0x42 ('B')
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    lastError = "Failed to send bootloader command to app (0x" + String(APP_I2C_ADDR, HEX) + ")";
    Serial.println("[MD11SlaveUpdate] ERROR: " + lastError);
    return false;
  }
  
  Serial.println("[MD11SlaveUpdate] Bootloader command sent. Waiting for app to reboot...");
  
  // Wait for app to reboot and bootloader to start
  delay(2000);
  
  // Verify bootloader is active by querying version
  String version;
  if (!queryBootloaderVersion(version)) {
    lastError = "Bootloader did not respond after reboot";
    Serial.println("[MD11SlaveUpdate] ERROR: " + lastError);
    return false;
  }
  
  Serial.println("[MD11SlaveUpdate] Bootloader active! Version: " + version);
  return true;
}

bool MD11SlaveUpdate::queryBootloaderVersion(String& version) {
  Serial.println("[MD11SlaveUpdate] Querying bootloader version...");
  
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
  Serial.println("[MD11SlaveUpdate] Bootloader version: " + version);
  
  return true;
}

bool MD11SlaveUpdate::queryChipSignature(uint8_t& sig0, uint8_t& sig1, uint8_t& sig2) {
  Serial.println("[MD11SlaveUpdate] Querying chip signature...");
  
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
  
  Serial.printf("[MD11SlaveUpdate] Chip signature: %02X %02X %02X\n", sig0, sig1, sig2);
  
  return true;
}

bool MD11SlaveUpdate::uploadHexFile(const String& hexContent, void (*progressCallback)(int percent)) {
  Serial.println("[MD11SlaveUpdate] Starting hex file upload (page-based mode)...");
  
  // Page buffer (128 bytes for ATmega328P)
  const uint16_t PAGE_SIZE = 128;
  uint8_t pageBuffer[PAGE_SIZE];
  uint16_t currentPageBase = 0xFFFF;  // Current page start address
  bool pagePending = false;  // Page has data waiting to be flushed
  
  // Parse hex file line by line
  int lineCount = 0;
  int errorCount = 0;
  uint32_t totalBytes = 0;
  uint16_t baseAddress = 0x0000;  // Extended linear address
  
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
      Serial.println("[MD11SlaveUpdate] ERROR: Invalid hex line " + String(lineCount));
      errorCount++;
      continue;
    }
    
    // Handle record types
    if (record.recordType == 0x00) {
      // Data record
      uint16_t fullAddress = (baseAddress << 16) | record.address;
      
      // Check if address is in bootloader section (0x7C00 for ATmega328P)
      if (fullAddress >= 0x7C00 && fullAddress < 0x8000) {
        Serial.println("[MD11SlaveUpdate] Skipping bootloader section at 0x" + String(fullAddress, HEX));
        continue;
      }
      
      // Process each byte in the record
      for (size_t i = 0; i < record.data.size(); i++) {
        uint16_t byteAddr = fullAddress + i;
        uint16_t pageBase = byteAddr & ~(PAGE_SIZE - 1);  // Round down to page boundary
        uint16_t offsetInPage = byteAddr & (PAGE_SIZE - 1);
        
        // Check if we crossed into a new page
        if (pageBase != currentPageBase) {
          // Flush previous page if it has data
          if (pagePending && currentPageBase != 0xFFFF) {
            Serial.println("[MD11SlaveUpdate] Page boundary crossed, flushing page at 0x" + String(currentPageBase, HEX));
            if (!writeFlashPage(currentPageBase, pageBuffer, PAGE_SIZE)) {
              Serial.println("[MD11SlaveUpdate] ERROR: Failed to write page at 0x" + String(currentPageBase, HEX));
              errorCount++;
              break;
            }
          }
          
          // Start new page
          currentPageBase = pageBase;
          memset(pageBuffer, 0xFF, PAGE_SIZE);  // Initialize with 0xFF (erased flash)
          pagePending = false;
          Serial.println("[MD11SlaveUpdate] Starting new page at 0x" + String(currentPageBase, HEX));
        }
        
        // Add byte to page buffer
        pageBuffer[offsetInPage] = record.data[i];
        pagePending = true;
        totalBytes++;
        
        // Progress callback every 256 bytes
        if (progressCallback && (totalBytes % 256 == 0)) {
          progressCallback((totalBytes * 100) / 32768);  // Estimate 32KB typical sketch size
        }
      }
      
    } else if (record.recordType == 0x04) {
      // Extended linear address record
      if (record.data.size() >= 2) {
        baseAddress = (record.data[0] << 8) | record.data[1];
        Serial.println("[MD11SlaveUpdate] Extended address: 0x" + String(baseAddress, HEX));
      }
      
    } else if (record.recordType == 0x01) {
      // End of file
      Serial.println("[MD11SlaveUpdate] EOF reached at line " + String(lineCount));
      break;
    }
  }
  
  // Flush last page if pending
  if (pagePending && currentPageBase != 0xFFFF) {
    Serial.println("[MD11SlaveUpdate] Flushing last page at 0x" + String(currentPageBase, HEX));
    if (!writeFlashPage(currentPageBase, pageBuffer, PAGE_SIZE)) {
      Serial.println("[MD11SlaveUpdate] ERROR: Failed to write last page");
      errorCount++;
    }
  }
  
  if (errorCount > 0) {
    lastError = "Upload failed with " + String(errorCount) + " errors";
    return false;
  }
  
  Serial.println("[MD11SlaveUpdate] Upload complete! Wrote " + String(totalBytes) + " bytes");
  return true;
}

bool MD11SlaveUpdate::writeFlashPage(uint16_t pageAddress, const uint8_t* pageData, uint16_t pageSize) {
  Serial.printf("[MD11SlaveUpdate] Writing page at 0x%04X (%d bytes)\n", pageAddress, pageSize);
  
  const int CHUNK_SIZE = 16;  // 16 bytes per I2C transaction (safe for Arduino Wire library)
  const int MAX_RETRIES = 3;
  
  // Write page in 16-byte chunks
  // CRITICAL: Each chunk MUST be a separate I2C transaction with STOP condition!
  // The bootloader accumulates data from multiple STOP-terminated transactions.
  // Page write is triggered internally when page boundary is reached.
  for (int offset = 0; offset < pageSize; offset += CHUNK_SIZE) {
    int bytesThisChunk = min(CHUNK_SIZE, pageSize - offset);
    uint16_t chunkAddr = pageAddress + offset;
    
    bool chunkSent = false;
    for (int attempt = 1; attempt <= MAX_RETRIES && !chunkSent; attempt++) {
      Wire.beginTransmission(TWIBOOT_I2C_ADDR);
      // CRITICAL: Use 4-byte header protocol from original Gus Mueller code!
      // Reference: https://github.com/judasgutenberg/twiboot_for_arduino/blob/main/slaveupdate.cpp
      Wire.write(0x02);  // CMD_ACCESS_MEMORY
      Wire.write(0x01);  // MEMTYPE_FLASH
      Wire.write((chunkAddr >> 8) & 0xFF);  // Address high byte
      Wire.write(chunkAddr & 0xFF);         // Address low byte
      
      // Write chunk data
      for (int i = 0; i < bytesThisChunk; i++) {
        Wire.write(pageData[offset + i]);
      }
      
      delay(2);  // Small delay before transmission
      // CRITICAL: Send STOP after every chunk (bootloader expects this!)
      uint8_t err = Wire.endTransmission();  // Default = true, sends STOP
      delay(2);
      
      if (err == 0) {
        chunkSent = true;
      } else {
        Serial.printf("[MD11SlaveUpdate] ERROR: Chunk send failed at offset %d (attempt %d, error %d)\n", 
                     offset, attempt, err);
        delay(5 * attempt);  // Increasing delay on retries
      }
    }
    
    if (!chunkSent) {
      lastError = "Failed to send chunk at offset " + String(offset);
      return false;
    }
    
    delay(10);  // POST_CHUNK_DELAY - let bootloader process chunk
  }
  
  Serial.println("[MD11SlaveUpdate] Page written successfully");
  return true;
}

bool MD11SlaveUpdate::parseHexLine(const String& line, HexRecord& record) {
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

bool MD11SlaveUpdate::writeMemory(uint16_t address, const uint8_t* data, uint16_t length) {
  Serial.printf("[MD11SlaveUpdate] Writing %d bytes to 0x%04X\n", length, address);
  
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

bool MD11SlaveUpdate::readMemory(uint16_t address, uint8_t* buffer, uint16_t length) {
  Serial.printf("[MD11SlaveUpdate] Reading %d bytes from 0x%04X\n", length, address);
  
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

bool MD11SlaveUpdate::sendBootloaderCommand(TwiBootCommand cmd, const uint8_t* data,
                                          uint16_t dataLen, uint8_t* response,
                                          uint16_t* responseLen) {
  // Send command to bootloader via I2C
  Wire.beginTransmission(TWIBOOT_I2C_ADDR);
  Wire.write((uint8_t)cmd);
  
  if (data && dataLen > 0) {
    Wire.write(data, dataLen);
  }
  
  uint8_t error = Wire.endTransmission(true);  // Complete transmission
  if (error != 0) {
    lastError = "I2C transmission failed";
    return false;
  }
  
  // Small delay for bootloader to process command
  delay(5);
  
  // Request response from bootloader
  if (response && responseLen && *responseLen > 0) {
    Wire.requestFrom(TWIBOOT_I2C_ADDR, *responseLen);
    
    // Wait for data with timeout
    unsigned long start = millis();
    while (Wire.available() < *responseLen && (millis() - start) < 100) {
      delay(1);
    }
    
    int bytesRead = 0;
    while (Wire.available() && bytesRead < *responseLen) {
      response[bytesRead++] = Wire.read();
    }
    
    *responseLen = bytesRead;
  }
  
  return true;
}

uint8_t MD11SlaveUpdate::calculateHexChecksum(const String& line) {
  // Calculate checksum for Intel HEX format
  uint8_t sum = 0;
  
  // Skip the ':' at the beginning
  for (int i = 1; i < line.length() - 2; i += 2) {
    uint8_t byte = strtol(line.substring(i, i + 2).c_str(), nullptr, 16);
    sum += byte;
  }
  
  return (~sum + 1) & 0xFF;
}
