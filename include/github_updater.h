#ifndef GITHUB_UPDATER_H
#define GITHUB_UPDATER_H

#include <Arduino.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "display_manager.h"  // Use DisplayManager instead of direct SSD1306

// Update state enum
enum UpdateState {
  UPDATE_IDLE,
  UPDATE_CHECKING,
  UPDATE_AVAILABLE,
  UPDATE_DOWNLOADING,
  UPDATE_INSTALLING,
  UPDATE_SUCCESS,
  UPDATE_ERROR
};

// Update info structure
struct UpdateInfo {
  UpdateState state;
  String remoteVersion;
  String firmwareUrl;
  String littlefsUrl;
  bool firmwareAvailable;
  bool littlefsAvailable;
  unsigned long lastCheck;
  String lastError;
  int downloadProgress;
};

class GitHubUpdater {
public:
  GitHubUpdater(Preferences& prefs);
  
  // Update info access
  UpdateInfo& getUpdateInfo();
  
  // Version comparison
  bool compareVersions(String remoteVer, String currentVer);
  
  // NVS persistence
  void saveUpdateInfo();
  void loadUpdateInfo();
  
  // Update operations
  bool checkGitHubRelease(const String& updateUrl, const String& githubToken,
                         const String& currentFwVer, const String& currentFsVer);
  bool downloadAndInstallFirmware(String url, const String& githubToken,
                                 String& currentFwVersion);
  bool downloadAndInstallLittleFS(String url, const String& githubToken,
                                 String& currentFsVersion);
  
  // API handlers - return JSON response strings
  String handleStatusRequest(const String& currentFwVer, const String& currentFsVer,
                            bool updatesEnabled, bool debugEnabled, bool hasToken);
  String handleCheckRequest(const String& updateUrl, const String& githubToken,
                           const String& currentFwVer, const String& currentFsVer);
  String handleInstallRequest(const String& type, const String& githubToken,
                             String& currentFwVer, String& currentFsVer,
                             bool& shouldReboot);
  String handleReinstallRequest(const String& type, const String& githubToken,
                               String& currentFwVer, String& currentFsVer,
                               bool debugEnabled, bool& shouldReboot);

private:
  UpdateInfo updateInfo;
  Preferences& preferences;
};

#endif // GITHUB_UPDATER_H
