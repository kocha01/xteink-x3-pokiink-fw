#pragma once

#include <Wire.h>
#include <cstddef>
#include <cstdint>
#include <ctime>

class DS3231Rtc {
 public:
  DS3231Rtc(uint8_t sdaPin, uint8_t sclPin);

  bool begin();
  bool isAvailable() const { return available_; }
  bool hasValidTime();
  bool readUnixTime(time_t& unixTimeUtc);
  bool writeUnixTime(time_t unixTimeUtc);

 private:
  static constexpr uint8_t kAddress = 0x68;
  static constexpr uint8_t kTimeRegister = 0x00;
  static constexpr uint8_t kStatusRegister = 0x0F;
  static constexpr uint8_t kOscillatorStopFlag = 0x80;

  bool readRegister(uint8_t reg, uint8_t& value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t* data, size_t len);
  bool writeRegisters(uint8_t reg, const uint8_t* data, size_t len);

  static uint8_t toBcd(uint8_t value);
  static uint8_t fromBcd(uint8_t value);
  static int64_t daysFromCivil(int year, unsigned month, unsigned day);
  static void civilFromDays(int64_t days, int& year, unsigned& month, unsigned& day);

  uint8_t sdaPin_;
  uint8_t sclPin_;
  bool begun_ = false;
  bool available_ = false;
};
