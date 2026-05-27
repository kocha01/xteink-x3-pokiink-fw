#pragma once

#include <cstdint>
#include <ctime>

#if defined(CROSSPOINT_BOARD_X3)
#include <BoardConfig.h>
#include <DS3231Rtc.h>
#endif

/// Lightweight NTP time synchronization for the ESP32-C3.
/// Uses the ESP-IDF SNTP component under the hood and, on X3 hardware,
/// restores/persists UTC time through an external DS3231 RTC.
class NtpClock {
 public:
  /// Singleton accessor.
  static NtpClock& getInstance();

  /// Apply timezone and, when available, restore system time from the external RTC.
  void begin(uint8_t timezoneIndex);

  /// Start SNTP client and synchronise once (blocks up to timeoutMs).
  /// Safe to call repeatedly — subsequent calls are no-ops until the
  /// next syncOnce(true) forced resync.
  /// Returns true when the system clock has been set.
  bool syncOnce(unsigned long timeoutMs = 10000, bool forceResync = false);

  /// Stop the SNTP client (call before WiFi disconnect to save power).
  void stop();

  /// True after the clock has a valid time source (RTC restore or NTP sync).
  bool isSynced() const { return synced_; }

  /// True after the current boot has completed at least one SNTP sync.
  bool hasNetworkSync() const { return networkSynced_; }

  /// Get current local time.  Returns false if time has never been synced.
  bool getTime(struct tm& timeInfo) const;

  /// Convenience: formatted time string "HH:MM".
  /// Returns "--:--" when not synced.
  const char* getTimeString();

  /// Convenience: formatted date string "DD/MM/YYYY".
  /// Returns "--/--/----" when not synced.
  const char* getDateString();

  /// Set timezone offset (default: +7 for Bangkok).
  /// Uses POSIX TZ notation, e.g. "ICT-7" for UTC+7.
  void setTimezone(const char* tz);

  /// Apply timezone from a settings index (0–8, matching CrossPointSettings::TIMEZONE enum).
  /// Falls back to UTC+7 Bangkok if index is out of range.
  void applyTimezoneByIndex(uint8_t index);

  /// Get the POSIX TZ string for a timezone index.
  static const char* getTimezoneStringForIndex(uint8_t index);

 private:
  NtpClock() = default;

  bool restoreFromRtc();
  void persistSystemTimeToRtc();

  bool synced_ = false;
  bool networkSynced_ = false;
  bool sntpStarted_ = false;
  bool rtcRestoreAttempted_ = false;
  char timeBuf_[6] = "--:--";
  char dateBuf_[11] = "--/--/----";

#if defined(CROSSPOINT_BOARD_X3)
  DS3231Rtc rtc_{BoardConfig::Pins::kI2cSda, BoardConfig::Pins::kI2cScl};
#endif
};

// Convenience macro
#define NTP_CLOCK NtpClock::getInstance()
