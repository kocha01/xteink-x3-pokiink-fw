#include "DS3231Rtc.h"

#include <Logging.h>

#include <limits>

namespace {

constexpr int kSecondsPerDay = 24 * 60 * 60;

}  // namespace

DS3231Rtc::DS3231Rtc(uint8_t sdaPin, uint8_t sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin) {}

bool DS3231Rtc::begin() {
  if (!begun_) {
    Wire.begin(sdaPin_, sclPin_);
    Wire.setClock(100000);
    begun_ = true;
  }

  Wire.beginTransmission(kAddress);
  available_ = (Wire.endTransmission() == 0);
  return available_;
}

bool DS3231Rtc::hasValidTime() {
  uint8_t status = 0;
  return readRegister(kStatusRegister, status) && (status & kOscillatorStopFlag) == 0;
}

bool DS3231Rtc::readUnixTime(time_t& unixTimeUtc) {
  if (!hasValidTime()) {
    return false;
  }

  uint8_t data[7] = {};
  if (!readRegisters(kTimeRegister, data, sizeof(data))) {
    return false;
  }

  const int seconds = fromBcd(data[0] & 0x7F);
  const int minutes = fromBcd(data[1] & 0x7F);

  int hours = 0;
  if ((data[2] & 0x40) != 0) {
    hours = fromBcd(data[2] & 0x1F) % 12;
    if ((data[2] & 0x20) != 0) {
      hours += 12;
    }
  } else {
    hours = fromBcd(data[2] & 0x3F);
  }

  const unsigned day = fromBcd(data[4] & 0x3F);
  const unsigned month = fromBcd(data[5] & 0x1F);
  int year = 2000 + fromBcd(data[6]);
  if ((data[5] & 0x80) != 0) {
    year += 100;
  }

  if (seconds > 59 || minutes > 59 || hours > 23 || day < 1 || day > 31 || month < 1 || month > 12 || year < 2021) {
    LOG_ERR("RTC", "DS3231 returned invalid datetime %04d-%02u-%02u %02d:%02d:%02d", year, month, day, hours, minutes,
            seconds);
    return false;
  }

  const int64_t days = daysFromCivil(year, month, day);
  const int64_t epoch = days * kSecondsPerDay + hours * 3600 + minutes * 60 + seconds;
  if (epoch < 0 || epoch > std::numeric_limits<time_t>::max()) {
    LOG_ERR("RTC", "DS3231 epoch %lld is out of range", static_cast<long long>(epoch));
    return false;
  }

  unixTimeUtc = static_cast<time_t>(epoch);
  return true;
}

bool DS3231Rtc::writeUnixTime(time_t unixTimeUtc) {
  if (!begin()) {
    return false;
  }
  if (unixTimeUtc < 0) {
    return false;
  }

  int64_t epoch = static_cast<int64_t>(unixTimeUtc);
  int64_t days = epoch / kSecondsPerDay;
  int secondsOfDay = static_cast<int>(epoch % kSecondsPerDay);
  if (secondsOfDay < 0) {
    secondsOfDay += kSecondsPerDay;
    --days;
  }

  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  civilFromDays(days, year, month, day);
  if (year < 2000 || year > 2099) {
    LOG_ERR("RTC", "Refusing to write out-of-range DS3231 year %d", year);
    return false;
  }

  const int hours = secondsOfDay / 3600;
  const int minutes = (secondsOfDay % 3600) / 60;
  const int seconds = secondsOfDay % 60;
  int weekday = static_cast<int>((days + 4) % 7);  // 1970-01-01 was a Thursday
  if (weekday < 0) {
    weekday += 7;
  }

  const uint8_t data[7] = {
      toBcd(static_cast<uint8_t>(seconds)),
      toBcd(static_cast<uint8_t>(minutes)),
      toBcd(static_cast<uint8_t>(hours)),
      toBcd(static_cast<uint8_t>(weekday + 1)),
      toBcd(static_cast<uint8_t>(day)),
      toBcd(static_cast<uint8_t>(month)),
      toBcd(static_cast<uint8_t>(year - 2000)),
  };

  if (!writeRegisters(kTimeRegister, data, sizeof(data))) {
    return false;
  }

  uint8_t status = 0;
  if (readRegister(kStatusRegister, status)) {
    status &= static_cast<uint8_t>(~kOscillatorStopFlag);
    writeRegister(kStatusRegister, status);
  }
  return true;
}

bool DS3231Rtc::readRegister(uint8_t reg, uint8_t& value) {
  return readRegisters(reg, &value, 1);
}

bool DS3231Rtc::writeRegister(uint8_t reg, uint8_t value) {
  return writeRegisters(reg, &value, 1);
}

bool DS3231Rtc::readRegisters(uint8_t reg, uint8_t* data, size_t len) {
  if (!begin()) {
    return false;
  }

  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t requested = static_cast<uint8_t>(len);
  const uint8_t received = Wire.requestFrom(kAddress, requested);
  if (received != requested) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

bool DS3231Rtc::writeRegisters(uint8_t reg, const uint8_t* data, size_t len) {
  if (!begin()) {
    return false;
  }

  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  for (size_t i = 0; i < len; ++i) {
    Wire.write(data[i]);
  }
  return Wire.endTransmission() == 0;
}

uint8_t DS3231Rtc::toBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

uint8_t DS3231Rtc::fromBcd(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4) * 10) + (value & 0x0F));
}

int64_t DS3231Rtc::daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

void DS3231Rtc::civilFromDays(int64_t days, int& year, unsigned& month, unsigned& day) {
  days += 719468;
  const int era = static_cast<int>((days >= 0 ? days : days - 146096) / 146097);
  const unsigned doe = static_cast<unsigned>(days - static_cast<int64_t>(era) * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  year = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp + (mp < 10 ? 3 : -9);
  year += month <= 2;
}
