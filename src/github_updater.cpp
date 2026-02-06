#include "github_updater.h"
#include "config.h"
#include "settings.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

GitHubUpdater::GitHubUpdater(Preferences& prefs, SSD1306& disp) 
  : preferences(prefs), display(disp) {
  updateInfo.state = UPDATE_IDLE;
  updateInfo.downloadProgress = 0;
}

UpdateInfo& GitHubUpdater::getUpdateInfo() {
  return updateInfo;
}

bool GitHubUpdater::compareVersions(String remoteVer, String currentVer) {
  if (remoteVer.length() == 0 || currentVer.length() == 0) return false;
  
  // Strip prefixes for comparison: fw-, fs-
  String remote = remoteVer;
  String current = currentVer;
  
  // Remove version prefixes (fw- and fs- only, no v prefix expected)
  if (remote.startsWith("fw-")) remote = remote.substring(3);
  if (remote.startsWith("fs-")) remote = remote.substring(3);
  
  if (current.startsWith("fw-")) current = current.substring(3);
  if (current.startsWith("fs-")) current = current.substring(3);
  
  // Format: 2026.1.1.00 (dotted format only)
  int r0 = 0, r1 = 0, r2 = 0, r3 = 0;
  int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
  if (sscanf(remote.c_str(), "%d.%d.%d.%d", &r0, &r1, &r2, &r3) != 4) return false;
  if (sscanf(current.c_str(), "%d.%d.%d.%d", &c0, &c1, &c2, &c3) != 4) return false;

  if (r0 != c0) return r0 > c0;
  if (r1 != c1) return r1 > c1;
  if (r2 != c2) return r2 > c2;
  return r3 > c3;
}

void GitHubUpdater::saveUpdateInfo() {
  preferences.begin(NVS_NAMESPACE_OTA, false);
  preferences.putString("remoteVer", updateInfo.remoteVersion);
  preferences.putString("fwUrl", updateInfo.firmwareUrl);
  preferences.putString("fsUrl", updateInfo.littlefsUrl);
  preferences.putULong("lastCheck", updateInfo.lastCheck);
  preferences.putString("lastError", updateInfo.lastError);
  preferences.putBool("fwAvail", updateInfo.firmwareAvailable);
  preferences.putBool("lfsAvail", updateInfo.littlefsAvailable);
  preferences.end();
}

void GitHubUpdater::loadUpdateInfo() {
  preferences.begin(NVS_NAMESPACE_OTA, true);
  updateInfo.remoteVersion = preferences.getString("remoteVer", "");
  updateInfo.firmwareUrl = preferences.getString("fwUrl", "");
  updateInfo.littlefsUrl = preferences.getString("fsUrl", "");
  updateInfo.lastCheck = preferences.getULong("lastCheck", 0);
  updateInfo.lastError = preferences.getString("lastError", "");
  updateInfo.firmwareAvailable = preferences.getBool("fwAvail", false);
  updateInfo.littlefsAvailable = preferences.getBool("lfsAvail", false);
  preferences.end();
  updateInfo.state = UPDATE_IDLE;
  updateInfo.downloadProgress = 0;
}

bool GitHubUpdater::checkGitHubRelease(const String& updateUrl, const String& githubToken,
                                      const String& currentFwVer, const String& currentFsVer) {
  if (!updateUrl || updateUrl.length() == 0) {
    Serial.println("No update URL configured");
    return false;
  }
  
  updateInfo.state = UPDATE_CHECKING;
  updateInfo.lastError = "";
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  String apiUrl = updateUrl;
  if (!apiUrl.startsWith("http")) {
    Serial.println("Invalid update URL");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid URL format";
    return false;
  }
  
  Serial.println("Checking for updates at: " + apiUrl);
  
  http.begin(client, apiUrl);
  http.addHeader("User-Agent", "ESP32-OTA-Client");
  
  if (githubToken && githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
    Serial.println("Using GitHub token for authentication");
  }
  
  http.setTimeout(15000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("JSON parsing failed: " + String(error.c_str()));
      updateInfo.state = UPDATE_ERROR;
      updateInfo.lastError = "JSON parse error";
      http.end();
      return false;
    }
    
    String remoteVersion = doc["tag_name"].as<String>();
    JsonArray assets = doc["assets"];
    
    updateInfo.remoteVersion = remoteVersion;
    updateInfo.firmwareUrl = "";
    updateInfo.littlefsUrl = "";
    
    for (JsonObject asset : assets) {
      String name = asset["name"].as<String>();
      String downloadUrl = asset["url"].as<String>();
      
      if (name.startsWith("fw-") && name.endsWith(".bin")) {
        updateInfo.firmwareUrl = downloadUrl;
        Serial.println("Found firmware: " + downloadUrl);
      } else if (name.startsWith("fs-") && name.endsWith(".bin")) {
        updateInfo.littlefsUrl = downloadUrl;
        Serial.println("Found filesystem: " + downloadUrl);
      }
    }
    
    updateInfo.firmwareAvailable = !updateInfo.firmwareUrl.isEmpty() && 
                                    compareVersions(remoteVersion, currentFwVer);
    updateInfo.littlefsAvailable = !updateInfo.littlefsUrl.isEmpty() && 
                                    compareVersions(remoteVersion, currentFsVer);
    
    if (updateInfo.firmwareAvailable || updateInfo.littlefsAvailable) {
      updateInfo.state = UPDATE_AVAILABLE;
      Serial.println("Update available: " + remoteVersion);
      Serial.println("Current firmware: " + currentFwVer);
      Serial.println("Current filesystem: " + currentFsVer);
      if (updateInfo.firmwareAvailable) {
        Serial.println("→ Firmware update available");
      }
      if (updateInfo.littlefsAvailable) {
        Serial.println("→ Filesystem update available");
      }
    } else {
      updateInfo.state = UPDATE_IDLE;
      Serial.println("No update needed. Remote: " + remoteVersion + ", Current: " + currentFwVer);
    }
    
    updateInfo.lastCheck = millis();
    saveUpdateInfo();
    
    http.end();
    return true;
    
  } else {
    Serial.println("HTTP request failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "HTTP " + String(httpCode);
    http.end();
    return false;
  }
}

bool GitHubUpdater::downloadAndInstallFirmware(String url, const String& githubToken,
                                              String& currentFwVersion) {
  if (url.length() == 0) {
    Serial.println("No firmware URL available");
    return false;
  }
  
  Serial.println(">>> DISPLAYING 'Updating FW' on OLED <<<");
  display.normalDisplay();  // Ensure normal mode
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Updating FW");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 30, "Please Wait...");
  display.drawString(0, 45, "DO NOT POWER OFF");
  display.display();
  Serial.println(">>> OLED display() called <<<");
  
  updateInfo.state = UPDATE_DOWNLOADING;
  updateInfo.downloadProgress = 0;
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  Serial.println("Downloading firmware from: " + url);
  Serial.println("Token available: " + String(githubToken.length() > 0 ? "YES" : "NO"));
  
  http.begin(client, url);
  http.setTimeout(60000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  
  if (githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
  }
  http.addHeader("Accept", "application/octet-stream");
  
  int httpCode = http.GET();
  Serial.println("HTTP response code: " + String(httpCode));
  
  if (httpCode != 200 && httpCode != 302) {
    Serial.println("Download failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Download failed HTTP " + String(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid content length";
    http.end();
    return false;
  }
  
  Serial.printf("Firmware size: %d bytes\n", contentLength);
  
  updateInfo.state = UPDATE_INSTALLING;
  
  if (!Update.begin(contentLength, U_FLASH)) {
    Serial.println("Update.begin failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = Update.errorString();
    http.end();
    return false;
  }
  
  WiFiClient * stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    
    if (available) {
      int c = stream->readBytes(buff, min(available, (size_t)sizeof(buff)));
      
      if (c > 0) {
        if (Update.write(buff, c) != c) {
          Serial.println("Write failed");
          Update.abort();
          updateInfo.state = UPDATE_ERROR;
          updateInfo.lastError = "Write failed";
          http.end();
          return false;
        }
        written += c;
        updateInfo.downloadProgress = (written * 100) / contentLength;
        
        esp_task_wdt_reset();
        yield();
        
        if (written % 10240 == 0) {
          Serial.printf("Progress: %d%%\n", updateInfo.downloadProgress);
        }
      }
    }
    esp_task_wdt_reset();
    delay(1);
  }
  
  http.end();
  
  if (written != contentLength) {
    Serial.println("Written bytes mismatch");
    Update.abort();
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Size mismatch";
    return false;
  }
  
  if (!Update.end()) {
    Serial.println("Update.end failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = Update.errorString();
    return false;
  }
  
  if (!Update.isFinished()) {
    Serial.println("Update not finished");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Update incomplete";
    return false;
  }
  
  Serial.println("Firmware update successful!");
  updateInfo.state = UPDATE_SUCCESS;
  updateInfo.downloadProgress = 100;
  
  Serial.println(">>> DISPLAYING 'Update done / Rebooting...' on OLED <<<");
  display.clear();
  // Keep normal display - no invertDisplay() for OTA updates
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 15, "Update done");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "Rebooting...");
  display.display();
  Serial.println(">>> OLED display() called (normal white-on-black) <<<");
  
  String newVersion = updateInfo.remoteVersion;
  if (newVersion.startsWith("fw-")) {
    newVersion = newVersion.substring(3);
  }
  currentFwVersion = newVersion;
  settings.updateVersions();
  
  return true;
}

bool GitHubUpdater::downloadAndInstallLittleFS(String url, const String& githubToken,
                                              String& currentFsVersion) {
  if (url.length() == 0) {
    Serial.println("No LittleFS URL available");
    return false;
  }
  
  display.normalDisplay();  // Ensure normal mode
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Updating FS");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 30, "Please Wait...");
  display.drawString(0, 45, "DO NOT POWER OFF");
  display.display();
  
  updateInfo.state = UPDATE_DOWNLOADING;
  updateInfo.downloadProgress = 0;
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  Serial.println("=== DOWNLOAD DEBUG ===");
  Serial.println("LittleFS URL: " + url);
  Serial.println("Token (first 10 chars): " + (githubToken.length() > 0 ? githubToken.substring(0, min(10, (int)githubToken.length())) : "EMPTY"));
  Serial.println("=====================");
  
  http.begin(client, url);
  http.setTimeout(60000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  
  if (githubToken.length() > 0) {
    http.addHeader("Authorization", "token " + githubToken);
    Serial.println("Added Authorization header");
  }
  http.addHeader("Accept", "application/octet-stream");
  Serial.println("Added Accept header");
  
  int httpCode = http.GET();
  Serial.println("HTTP response code: " + String(httpCode));
  
  if (httpCode != 200 && httpCode != 302) {
    Serial.println("Download failed: " + String(httpCode));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS download failed HTTP " + String(httpCode);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "Invalid LFS content length";
    http.end();
    return false;
  }
  
  Serial.printf("LittleFS size: %d bytes\n", contentLength);
  
  updateInfo.state = UPDATE_INSTALLING;
  
  #ifdef U_LITTLEFS
  if (!Update.begin(contentLength, U_LITTLEFS)) {
  #else
  if (!Update.begin(contentLength, U_SPIFFS)) {
  #endif
    Serial.println("LittleFS Update.begin failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS " + String(Update.errorString());
    http.end();
    return false;
  }
  
  WiFiClient * stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    
    if (available) {
      int c = stream->readBytes(buff, min(available, (size_t)sizeof(buff)));
      
      if (c > 0) {
        if (Update.write(buff, c) != c) {
          Serial.println("LittleFS Write failed");
          Update.abort();
          updateInfo.state = UPDATE_ERROR;
          updateInfo.lastError = "LFS write failed";
          http.end();
          return false;
        }
        written += c;
        updateInfo.downloadProgress = (written * 100) / contentLength;
        
        esp_task_wdt_reset();
        yield();
        
        if (written % 10240 == 0) {
          Serial.printf("LittleFS Progress: %d%%\n", updateInfo.downloadProgress);
        }
      }
    }
    esp_task_wdt_reset();
    delay(1);
  }
  
  http.end();
  
  if (written != contentLength) {
    Serial.println("LittleFS Written bytes mismatch");
    Update.abort();
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS size mismatch";
    return false;
  }
  
  if (!Update.end()) {
    Serial.println("LittleFS Update.end failed: " + String(Update.errorString()));
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS " + String(Update.errorString());
    return false;
  }
  
  if (!Update.isFinished()) {
    Serial.println("LittleFS Update not finished");
    updateInfo.state = UPDATE_ERROR;
    updateInfo.lastError = "LFS update incomplete";
    return false;
  }
  
  Serial.println("LittleFS update successful!");
  updateInfo.state = UPDATE_SUCCESS;
  updateInfo.downloadProgress = 100;
  
  Serial.println(">>> DISPLAYING 'Update done / Rebooting...' on OLED (LittleFS) <<<");
  display.clear();
  // Keep normal display - no invertDisplay() for OTA updates
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 15, "Update done");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "Rebooting...");
  display.display();
  Serial.println(">>> OLED display() called (normal white-on-black) <<<");
  
  String newVersion = updateInfo.remoteVersion;
  if (newVersion.startsWith("fw-")) {
    newVersion = newVersion.substring(3);
  }
  currentFsVersion = newVersion;
  settings.updateVersions();
  
  return true;
}

// =============================================================================
// API HANDLER METHODS
// =============================================================================

String GitHubUpdater::handleStatusRequest(const String& currentFwVer, const String& currentFsVer,
                                         bool updatesEnabled, bool debugEnabled, bool hasToken) {
  JsonDocument doc;
  
  doc["currentFirmwareVersion"] = currentFwVer;
  doc["currentFilesystemVersion"] = currentFsVer;
  doc["updatesEnabled"] = updatesEnabled;
  doc["debugEnabled"] = debugEnabled;
  doc["hasGithubToken"] = hasToken;
  doc["remoteVersion"] = updateInfo.remoteVersion;
  doc["state"] = updateInfo.state;
  doc["firmwareAvailable"] = updateInfo.firmwareAvailable;
  doc["littlefsAvailable"] = updateInfo.littlefsAvailable;
  doc["availableFirmwareVersion"] = updateInfo.remoteVersion;
  doc["availableFilesystemVersion"] = updateInfo.remoteVersion;
  doc["lastCheck"] = updateInfo.lastCheck;
  doc["lastError"] = updateInfo.lastError;
  doc["downloadProgress"] = updateInfo.downloadProgress;
  
  String response;
  serializeJson(doc, response);
  return response;
}

String GitHubUpdater::handleCheckRequest(const String& updateUrl, const String& githubToken,
                                        const String& currentFwVer, const String& currentFsVer) {
  Serial.println("=== UPDATE CHECK REQUESTED ===");
  bool success = checkGitHubRelease(updateUrl, githubToken, currentFwVer, currentFsVer);
  
  JsonDocument doc;
  doc["success"] = success;
  doc["message"] = success ? "Check completed" : "Check failed";
  doc["updateAvailable"] = updateInfo.firmwareAvailable || updateInfo.littlefsAvailable;
  
  String response;
  serializeJson(doc, response);
  return response;
}

String GitHubUpdater::handleInstallRequest(const String& type, const String& githubToken,
                                          String& currentFwVer, String& currentFsVer,
                                          bool& shouldReboot) {
  Serial.println("=== UPDATE INSTALL REQUESTED ===");
  Serial.println("Install type: " + type);
  Serial.println("Firmware available: " + String(updateInfo.firmwareAvailable));
  Serial.println("LittleFS available: " + String(updateInfo.littlefsAvailable));
  
  bool success = false;
  String message = "";
  
  if (type == "firmware" && updateInfo.firmwareAvailable) {
    Serial.println("Starting firmware download...");
    success = downloadAndInstallFirmware(updateInfo.firmwareUrl, githubToken, currentFwVer);
    message = success ? "Firmware installed" : "Firmware install failed";
  } else if (type == "littlefs" && updateInfo.littlefsAvailable) {
    Serial.println("Starting LittleFS download...");
    success = downloadAndInstallLittleFS(updateInfo.littlefsUrl, githubToken, currentFsVer);
    message = success ? "LittleFS installed" : "LittleFS install failed";
  } else if (type == "both") {
    if (updateInfo.firmwareAvailable) {
      success = downloadAndInstallFirmware(updateInfo.firmwareUrl, githubToken, currentFwVer);
      if (!success) {
        message = "Firmware install failed";
      }
    }
    if (success && updateInfo.littlefsAvailable) {
      success = downloadAndInstallLittleFS(updateInfo.littlefsUrl, githubToken, currentFsVer);
      message = success ? "Both installed" : "LittleFS install failed";
    } else if (success) {
      message = "Firmware installed";
    }
  } else {
    message = "No updates available";
  }
  
  shouldReboot = success;
  
  JsonDocument doc;
  doc["success"] = success;
  doc["message"] = message;
  doc["rebootRequired"] = success;
  
  String response;
  serializeJson(doc, response);
  return response;
}

String GitHubUpdater::handleReinstallRequest(const String& type, const String& githubToken,
                                            String& currentFwVer, String& currentFsVer,
                                            bool debugEnabled, bool& shouldReboot) {
  Serial.println("=== REINSTALL REQUESTED ===");
  
  if (!debugEnabled) {
    shouldReboot = false;
    return "{\"success\":false,\"message\":\"Debug mode required\"}";
  }
  
  Serial.println("Reinstall type: " + type);
  
  bool success = false;
  String message = "";
  
  if (type == "firmware" && !updateInfo.firmwareUrl.isEmpty()) {
    success = downloadAndInstallFirmware(updateInfo.firmwareUrl, githubToken, currentFwVer);
    message = success ? "Firmware update successful!" : "Firmware update failed";
  } else if (type == "littlefs" && !updateInfo.littlefsUrl.isEmpty()) {
    success = downloadAndInstallLittleFS(updateInfo.littlefsUrl, githubToken, currentFsVer);
    message = success ? "Filesystem update successful!" : "Filesystem update failed";
  } else if (type == "both") {
    if (!updateInfo.firmwareUrl.isEmpty()) {
      success = downloadAndInstallFirmware(updateInfo.firmwareUrl, githubToken, currentFwVer);
      if (!success) {
        message = "Firmware update failed";
      }
    }
    if (success && !updateInfo.littlefsUrl.isEmpty()) {
      success = downloadAndInstallLittleFS(updateInfo.littlefsUrl, githubToken, currentFsVer);
      message = success ? "Firmware update successful!" : "Filesystem update failed";
    } else if (success) {
      message = "Firmware update successful!";
    }
  } else {
    message = "No update URLs available. Check for updates first.";
  }
  
  shouldReboot = success;
  
  JsonDocument doc;
  doc["success"] = success;
  doc["message"] = message;
  doc["rebootRequired"] = success;
  
  String response;
  serializeJson(doc, response);
  return response;
}
