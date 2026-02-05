#ifndef GITHUB_UPDATER_H
#define GITHUB_UPDATER_H

#include <Arduino.h>
#include <Preferences.h>
#include <SSD1306.h>

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
  String remoteLittlefsVersion;
  String firmwareUrl;
  String littlefsUrl;
  bool firmwareAvailable;
  bool littlefsAvailable;
  unsigned long lastCheck;
  String lastError;
  int downloadProgress;
  bool filesystemUpdateDone;
};

class GitHubUpdater {
public:
  GitHubUpdater(Preferences& prefs, SSD1306& disp);
  
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

private:
  UpdateInfo updateInfo;
  Preferences& preferences;
  SSD1306& display;
};

#endif // GITHUB_UPDATER_H
