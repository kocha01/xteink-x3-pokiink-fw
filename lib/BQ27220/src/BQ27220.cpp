#include "BQ27220.h"

#include <Logging.h>

BQ27220::BQ27220(uint8_t sdaPin, uint8_t sclPin)
    : sdaPin_(sdaPin), sclPin_(sclPin) {}

bool BQ27220::begin() {
  Wire.begin(sdaPin_, sclPin_);
  Wire.setClock(100000);  // BQ27220 supports up to 400kHz, use 100kHz for reliability

  // Probe: try reading the voltage register
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(REG_VOLTAGE);
  const uint8_t err = Wire.endTransmission(false);
  if (err != 0) {
    LOG_ERR("BQ27220", "I2C probe failed (err=%u), fuel gauge not found at 0x%02X", err, I2C_ADDR);
    available_ = false;
    return false;
  }

  const uint8_t n = Wire.requestFrom(I2C_ADDR, static_cast<uint8_t>(2));
  if (n != 2) {
    LOG_ERR("BQ27220", "I2C read failed (got %u bytes), fuel gauge not responding", n);
    available_ = false;
    return false;
  }
  // Consume the bytes
  Wire.read();
  Wire.read();

  available_ = true;
  const uint16_t mv = getVoltage();
  const uint16_t soc = getStateOfCharge();
  LOG_DBG("BQ27220", "Fuel gauge detected: %u mV, %u%% SoC", mv, soc);
  return true;
}

uint16_t BQ27220::readWord(uint8_t reg) {
  if (!available_) return 0;

  Wire.beginTransmission(I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    LOG_ERR("BQ27220", "I2C write reg 0x%02X failed", reg);
    return 0;
  }

  const uint8_t n = Wire.requestFrom(I2C_ADDR, static_cast<uint8_t>(2));
  if (n != 2) {
    LOG_ERR("BQ27220", "I2C read reg 0x%02X failed (got %u bytes)", reg, n);
    return 0;
  }

  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  return static_cast<uint16_t>(hi << 8 | lo);
}

uint16_t BQ27220::getStateOfCharge() {
  return readWord(REG_SOC);
}

uint16_t BQ27220::getVoltage() {
  return readWord(REG_VOLTAGE);
}

int16_t BQ27220::getAverageCurrent() {
  return static_cast<int16_t>(readWord(REG_AVG_CURRENT));
}

uint16_t BQ27220::getRemainingCapacity() {
  return readWord(REG_REM_CAPACITY);
}

uint16_t BQ27220::getFullChargeCapacity() {
  return readWord(REG_FULL_CHG_CAP);
}

uint16_t BQ27220::getInternalTemperature() {
  return readWord(REG_INT_TEMP);
}

float BQ27220::getTemperatureCelsius() {
  const uint16_t raw = getInternalTemperature();
  // BQ27220 returns temperature in 0.1 K units
  return static_cast<float>(raw) / 10.0f - 273.15f;
}

uint16_t BQ27220::getStateOfHealth() {
  return readWord(REG_SOH);
}
