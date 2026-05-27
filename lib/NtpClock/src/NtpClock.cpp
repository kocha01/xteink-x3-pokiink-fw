#include "NtpClock.h"

#include <Arduino.h>
#include <Logging.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

namespace {

constexpr int kValidYearOffset = 120;  // 2020

void logLocalTime(const char* tag, const char* prefix, time_t now) {
  struct tm timeInfo;
  localtime_r(&now, &timeInfo);
  LOG_DBG(tag, "%s%04d-%02d-%02d %02d:%02d:%02d", prefix, timeInfo.tm_year + 1900, timeInfo.tm_mon + 1,
          timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
}

}  // namespace

NtpClock& NtpClock::getInstance() {
  static NtpClock instance;
  return instance;
}

const char* NtpClock::getTimezoneStringForIndex(uint8_t index) {
  // POSIX TZ strings use the opposite sign convention from UTC:
  // e.g. UTC+7 (Bangkok) is "ICT-7" in POSIX.
  static const char* tzStrings[] = {
      "PST8",     // 0: UTC-8 Pacific
      "EST5",     // 1: UTC-5 Eastern
      "GMT0",     // 2: UTC+0 GMT
      "CET-1",    // 3: UTC+1 CET
      "MSK-3",    // 4: UTC+3 Moscow
      "ICT-7",    // 5: UTC+7 Bangkok (default)
      "SGT-8",    // 6: UTC+8 Singapore
      "JST-9",    // 7: UTC+9 Tokyo
      "AEST-10",  // 8: UTC+10 Sydney
  };
  constexpr uint8_t defaultIndex = 5;  // Bangkok
  if (index >= sizeof(tzStrings) / sizeof(tzStrings[0])) {
    index = defaultIndex;
  }
  return tzStrings[index];
}

void NtpClock::applyTimezoneByIndex(uint8_t index) {
  setTimezone(getTimezoneStringForIndex(index));
}

void NtpClock::begin(uint8_t timezoneIndex) {
  applyTimezoneByIndex(timezoneIndex);
  if (synced_ || rtcRestoreAttempted_) {
    return;
  }
  rtcRestoreAttempted_ = true;
  restoreFromRtc();
}

void NtpClock::setTimezone(const char* tz) {
  setenv("TZ", tz, 1);
  tzset();
  LOG_DBG("NTP", "Timezone set to %s", tz);
}

bool NtpClock::restoreFromRtc() {
#if defined(CROSSPOINT_BOARD_X3)
  if (!rtc_.begin()) {
    LOG_DBG("NTP", "DS3231 not detected on the X3 I2C bus");
    return false;
  }
  if (!rtc_.hasValidTime()) {
    LOG_DBG("NTP", "DS3231 detected but its time is not valid yet");
    return false;
  }

  time_t rtcTime = 0;
  if (!rtc_.readUnixTime(rtcTime)) {
    LOG_ERR("NTP", "DS3231 is present but its time could not be read");
    return false;
  }

  const timeval tv{.tv_sec = rtcTime, .tv_usec = 0};
  if (settimeofday(&tv, nullptr) != 0) {
    LOG_ERR("NTP", "Failed to apply DS3231 time to the system clock");
    return false;
  }

  synced_ = true;
  networkSynced_ = false;
  logLocalTime("NTP", "Restored time from DS3231: ", rtcTime);
  return true;
#else
  return false;
#endif
}

void NtpClock::persistSystemTimeToRtc() {
#if defined(CROSSPOINT_BOARD_X3)
  time_t now = 0;
  time(&now);
  if (now <= 0) {
    return;
  }
  if (!rtc_.begin()) {
    LOG_ERR("NTP", "Cannot persist time because DS3231 is not responding");
    return;
  }
  if (rtc_.writeUnixTime(now)) {
    logLocalTime("NTP", "Persisted time to DS3231: ", now);
  } else {
    LOG_ERR("NTP", "Failed to persist system time to DS3231");
  }
#endif
}

bool NtpClock::syncOnce(unsigned long timeoutMs, bool forceResync) {
  if (networkSynced_ && !forceResync) {
    return true;  // Already completed an SNTP sync this boot
  }

  if (!sntpStarted_) {
    LOG_DBG("NTP", "Starting SNTP sync...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_setservername(2, "time.google.com");
    esp_sntp_init();
    sntpStarted_ = true;
  }

  // Wait for time to be set
  const unsigned long start = millis();
  time_t now = 0;
  struct tm timeInfo;

  while (millis() - start < timeoutMs) {
    time(&now);
    localtime_r(&now, &timeInfo);
    // Year > 2020 means we have a valid time
    if (timeInfo.tm_year > kValidYearOffset) {
      synced_ = true;
      networkSynced_ = true;
      LOG_DBG("NTP", "Time synced: %04d-%02d-%02d %02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1,
              timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      persistSystemTimeToRtc();
      return true;
    }
    delay(100);
  }

  LOG_ERR("NTP", "SNTP sync timeout after %lu ms", timeoutMs);
  return false;
}

void NtpClock::stop() {
  if (sntpStarted_) {
    esp_sntp_stop();
    sntpStarted_ = false;
    LOG_DBG("NTP", "SNTP stopped");
  }
}

bool NtpClock::getTime(struct tm& timeInfo) const {
  if (!synced_) return false;

  time_t now;
  time(&now);
  localtime_r(&now, &timeInfo);
  return timeInfo.tm_year > kValidYearOffset;  // Sanity check
}

const char* NtpClock::getTimeString() {
  struct tm timeInfo;
  if (getTime(timeInfo)) {
    snprintf(timeBuf_, sizeof(timeBuf_), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  } else {
    snprintf(timeBuf_, sizeof(timeBuf_), "--:--");
  }
  return timeBuf_;
}

const char* NtpClock::getDateString() {
  struct tm timeInfo;
  if (getTime(timeInfo)) {
    snprintf(dateBuf_, sizeof(dateBuf_), "%02d/%02d/%04d",
             timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900);
  } else {
    snprintf(dateBuf_, sizeof(dateBuf_), "--/--/----");
  }
  return dateBuf_;
}
