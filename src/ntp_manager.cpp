#include "ntp_manager.h"
#include "config.h"
#include "settings.h"
#include <time.h>
#include <sys/time.h>

// ============================================================================
// Timezone Parsing
// ============================================================================

int parseTimezoneOffset(const String& timezone) {
  int offset = 0;
  if (timezone.startsWith("UTC+")) {
    offset = timezone.substring(4).toInt();
  } else if (timezone.startsWith("UTC-")) {
    offset = -timezone.substring(4).toInt();
  }
  return offset;
}

// ============================================================================
// NTP Time Synchronization
// ============================================================================

void syncTimeIfEnabled(bool isBootSync) {
  if (!Settings::stringToBool(settings.ntpEnabled)) {
    Serial.println("NTP sync disabled");
    return;
  }

  Serial.println("Starting NTP sync...");
  configTime(NTP_GMT_OFFSET_SEC, NTP_DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

  const unsigned long start = millis();
  time_t now = 0;
  while (now < NTP_VALID_TIME && (millis() - start) < NTP_SYNC_TIMEOUT) {
    time(&now);
    delay(100);
  }

  if (now >= NTP_VALID_TIME) {
    Serial.println(String("NTP time synced: ") + ctime(&now));
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    settings.saveStoredDateIfNeeded(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    // Only save boot time on the first sync after boot (not on later settings updates)
    if (isBootSync) {
      int timezoneOffsetHours = parseTimezoneOffset(settings.timezone);
      
      // Save boot time if debug enabled (with timezone offset at time of sync)
      settings.saveBootTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timezoneOffsetHours);
    }
  } else {
    Serial.println("NTP sync timeout");
    Settings::StoredDate stored = settings.getStoredDate();
    if (stored.valid) {
      struct tm fallbackTime = {};
      fallbackTime.tm_year = stored.year - 1900;
      fallbackTime.tm_mon = stored.month - 1;
      fallbackTime.tm_mday = stored.day;
      fallbackTime.tm_hour = 0;
      fallbackTime.tm_min = 0;
      fallbackTime.tm_sec = 0;
      time_t fallback = mktime(&fallbackTime);
      if (fallback > 0) {
        struct timeval tv = { .tv_sec = fallback, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        Serial.println(String("NTP failed; using stored date: ") + stored.year + "-" + stored.month + "-" + stored.day);
      }
    }
  }
}
