# ESP32-Base Modularization Plan

## Doel
ESP32-Base ombouwen naar een volledig modulaire baseline waar modules eenvoudig aan/uit gezet kunnen worden voor verschillende projecten.

## Feature Flag Systeem

### Geïmplementeerd in config.h
```cpp
#define FEATURE_OLED_DISPLAY
#define FEATURE_GPIO_VIEWER
#define FEATURE_FILE_MANAGER
#define FEATURE_I2C_SCANNER
#define FEATURE_OTA_UPDATES
#define FEATURE_GITHUB_UPDATES
```

Om een module uit te schakelen: comment de `#define` uit.

## Module Structuur

### Fase 1: Core Refactoring (HIGH PRIORITY)

#### 1.1 Settings Module ✅ DONE
- **Files**: `src/settings.cpp`, `include/settings.h`
- **Status**: Al geïmplementeerd
- **Functionaliteit**: NVS storage, load/save, version sync

#### 1.2 WiFi Manager Module
- **Files**: `src/modules/wifi_manager.cpp`, `include/modules/wifi_manager.h`
- **Functionaliteit**:
  - STA mode connection
  - AP mode (captive portal)
  - WiFi scan
  - Reconnect logic
- **Dependencies**: Settings
- **API**:
  ```cpp
  class WiFiManager {
    bool begin();
    bool isConnected();
    bool isAPMode();
    String getIP();
    void startAP();
    void connect();
  }
  ```

#### 1.3 Web Server Module
- **Files**: `src/modules/web_server.cpp`, `include/modules/web_server.h`
- **Functionaliteit**:
  - AsyncWebServer setup
  - Route registration
  - Template processor
  - Static file serving
- **Dependencies**: WiFi Manager
- **API**:
  ```cpp
  class WebServer {
    void begin();
    void registerRoute(const char* uri, WebRequestHandler handler);
    void serveStatic(const char* uri, const char* path);
  }
  ```

### Fase 2: Display & Debug Modules (MEDIUM PRIORITY)

#### 2.1 OLED Display Module
- **Files**: `src/modules/display.cpp`, `include/modules/display.h`
- **Feature Flag**: `FEATURE_OLED_DISPLAY`
- **Functionaliteit**:
  - Display initialization
  - Status messages
  - IP display
  - Version display
- **Dependencies**: None (hardware)
- **API**:
  ```cpp
  class DisplayManager {
    void begin();
    void showMessage(String msg);
    void showIP(String ip);
    void showVersion();
    void clear();
  }
  ```

#### 2.2 GPIO Viewer Module
- **Files**: `src/modules/gpio_viewer.cpp`, `include/modules/gpio_viewer.h`
- **Feature Flag**: `FEATURE_GPIO_VIEWER`
- **Functionaliteit**: Real-time GPIO monitoring
- **Dependencies**: Settings (enabled check)
- **Compile conditional**:
  ```cpp
  #ifdef FEATURE_GPIO_VIEWER
    #include <gpio_viewer.h>
  #endif
  ```

#### 2.3 I2C Scanner Module
- **Files**: `src/modules/i2c_scanner.cpp`, `include/modules/i2c_scanner.h`
- **Feature Flag**: `FEATURE_I2C_SCANNER`
- **Functionaliteit**:
  - I2C bus scanning
  - Device identification
  - Register dump
  - Metrics
- **Dependencies**: Web Server (routes)
- **Routes**: `/i2c`, `/api/i2c/scan`, `/api/i2c/registers`

### Fase 3: File System Modules (MEDIUM PRIORITY)

#### 3.1 File Manager Module
- **Files**: `src/modules/file_manager.cpp`, `include/modules/file_manager.h`
- **Feature Flag**: `FEATURE_FILE_MANAGER`
- **Functionaliteit**:
  - File listing
  - Upload/download
  - Delete
  - View/edit text files
- **Dependencies**: Web Server (routes)
- **Routes**: `/files`, `/api/files`, `/api/file`, `/api/upload`

### Fase 4: Update Modules (HIGH PRIORITY)

#### 4.1 OTA Manager Module
- **Files**: `src/modules/ota_manager.cpp`, `include/modules/ota_manager.h`
- **Feature Flag**: `FEATURE_OTA_UPDATES`
- **Functionaliteit**:
  - ArduinoOTA setup
  - Local network updates
  - Progress callback
- **Dependencies**: Settings (enabled check)

#### 4.2 GitHub Updater Module
- **Files**: `src/modules/github_updater.cpp`, `include/modules/github_updater.h`
- **Feature Flag**: `FEATURE_GITHUB_UPDATES`
- **Functionaliteit**:
  - GitHub API polling
  - Version comparison
  - Firmware download
  - LittleFS download
  - Background checks
- **Dependencies**: Settings, Web Server (routes)
- **Routes**: `/update`, `/api/update/status`, `/api/update/check`, `/api/update/install`

### Fase 5: Future Modules (LOW PRIORITY)

#### 5.1 MQTT Client Module
- **Feature Flag**: `FEATURE_MQTT_CLIENT`
- **Functionaliteit**: MQTT pub/sub
- **Dependencies**: WiFi Manager

#### 5.2 NTP Time Module
- **Feature Flag**: `FEATURE_NTP_TIME`
- **Functionaliteit**: Time synchronization
- **Dependencies**: WiFi Manager

#### 5.3 mDNS Module
- **Feature Flag**: `FEATURE_MDNS`
- **Functionaliteit**: esp32.local discovery
- **Dependencies**: WiFi Manager

## Directory Structure (Target)

```
ESP32-Base/
├── src/
│   ├── main.cpp                          # Core setup/loop
│   ├── settings.cpp                      # ✅ Already modular
│   └── modules/
│       ├── wifi_manager.cpp
│       ├── web_server.cpp
│       ├── display.cpp
│       ├── gpio_viewer.cpp
│       ├── i2c_scanner.cpp
│       ├── file_manager.cpp
│       ├── ota_manager.cpp
│       └── github_updater.cpp
├── include/
│   ├── config.h                          # ✅ Feature flags added
│   ├── settings.h                        # ✅ Already modular
│   └── modules/
│       ├── wifi_manager.h
│       ├── web_server.h
│       ├── display.h
│       ├── gpio_viewer.h
│       ├── i2c_scanner.h
│       ├── file_manager.h
│       ├── ota_manager.h
│       └── github_updater.h
├── data/
│   ├── index.html                        # Always included
│   ├── settings.html                     # Always included
│   ├── wifimanager.html                  # Always included
│   ├── confirm.html                      # Always included
│   ├── files.html                        # FEATURE_FILE_MANAGER
│   ├── i2c.html                          # FEATURE_I2C_SCANNER
│   ├── update.html                       # FEATURE_GITHUB_UPDATES
│   └── style.css                         # Always included
└── docs/
    ├── MODULARIZATION_PLAN.md           # This file
    ├── MODULE_USAGE.md                   # How to use modules
    └── API_REFERENCE.md                  # Module API documentation
```

## Implementation Strategy

### Step 1: WiFi Manager Module (Week 1)
1. Create `wifi_manager.h/cpp`
2. Extract WiFi logic from `main.cpp`
3. Implement class with clean API
4. Test AP and STA modes
5. Verify captive portal works

### Step 2: Web Server Module (Week 1)
1. Create `web_server.h/cpp`
2. Extract AsyncWebServer setup
3. Create route registration system
4. Test with existing routes
5. Verify template processing

### Step 3: Display Module (Week 2)
1. Create `display.h/cpp`
2. Wrap `#ifdef FEATURE_OLED_DISPLAY`
3. Extract OLED logic from `main.cpp`
4. Test enabled/disabled states
5. Verify no compile errors when disabled

### Step 4: Debug Modules (Week 2)
1. GPIO Viewer: wrap existing code
2. I2C Scanner: extract to module
3. File Manager: extract to module
4. Test each can be disabled independently

### Step 5: Update Modules (Week 3)
1. OTA Manager: extract ArduinoOTA setup
2. GitHub Updater: extract update logic
3. Test update workflows
4. Verify rollback mechanisms

### Step 6: Main.cpp Cleanup (Week 3)
1. Refactor to use module APIs
2. Remove inline code
3. Keep only setup() and loop()
4. Document module initialization order

## Testing Checklist

For each module:
- [ ] Compiles with feature enabled
- [ ] Compiles with feature disabled
- [ ] No unused dependencies when disabled
- [ ] Flash size reduction measurable when disabled
- [ ] Functionality works when enabled
- [ ] No side effects when disabled
- [ ] Documentation updated

## Expected Benefits

### Code Quality
- **Separation of Concerns**: Each module has single responsibility
- **Testability**: Modules can be unit tested
- **Maintainability**: Changes localized to modules
- **Readability**: main.cpp becomes simple orchestration

### Flexibility
- **Easy customization**: Comment out unneeded features
- **Reduced flash usage**: Only compile what's needed
- **Faster compile times**: Incremental builds
- **Project templates**: Quick start for new projects

### Reusability
- **Module library**: Reuse across projects
- **Consistent API**: Predictable interfaces
- **Documentation**: Clear usage examples
- **Version independent**: Modules work standalone

## Migration Risks & Mitigation

### Risk 1: Breaking Existing Code
- **Mitigation**: Incremental migration, keep git branches
- **Rollback**: Each step is a separate commit

### Risk 2: Increased Complexity
- **Mitigation**: Clear documentation, simple APIs
- **Training**: Example projects showing usage

### Risk 3: Circular Dependencies
- **Mitigation**: Dependency diagram, forward declarations
- **Design**: Keep dependencies one-way where possible

### Risk 4: Flash/RAM Overhead
- **Mitigation**: Measure before/after, optimize if needed
- **Monitoring**: Track memory usage per module

## Success Metrics

- [ ] main.cpp < 500 lines (currently ~1500)
- [ ] Each module < 300 lines
- [ ] All features can be disabled
- [ ] Flash savings: ~200KB when all optional features disabled
- [ ] Compile time: < 30% increase (acceptable for modular design)
- [ ] Documentation: 100% API coverage

## Timeline

- **Week 1**: WiFi Manager + Web Server modules
- **Week 2**: Display + Debug modules
- **Week 3**: Update modules + Main.cpp cleanup
- **Week 4**: Testing + Documentation + Polish

**Total**: ~4 weeks for complete modularization

## Next Steps

1. ✅ Add feature flags to config.h (DONE)
2. Create MODULE_USAGE.md documentation
3. Start WiFi Manager module implementation
4. Set up testing framework

---

**Status**: Phase 0 - Feature flags implemented
**Next**: Create WiFi Manager module
**Target**: Fully modular baseline by end of February 2026
