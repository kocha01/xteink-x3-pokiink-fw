#pragma once

#include <Wire.h>
#include <cstdint>

/// Driver for the Texas Instruments BQ27220 fuel gauge IC.
/// Communicates over I2C at address 0x55 (7-bit).
/// Used on the Xteink X3 e-reader board to monitor battery state.
class BQ27220 {
 public:
  /// Construct with I2C pins.  Call begin() before reading.
  BQ27220(uint8_t sdaPin, uint8_t sclPin);

  /// Initialize the I2C bus and verify chip communication.
  /// Returns true when the gauge responds.
  bool begin();

  /// State-of-Charge in percent (0-100).
  uint16_t getStateOfCharge();

  /// Battery voltage in millivolts.
  uint16_t getVoltage();

  /// Average current in milliamps (signed: positive = charging).
  int16_t getAverageCurrent();

  /// Remaining capacity in mAh.
  uint16_t getRemainingCapacity();

  /// Full-charge capacity in mAh.
  uint16_t getFullChargeCapacity();

  /// Internal temperature in 0.1 K units.
  uint16_t getInternalTemperature();

  /// Internal temperature converted to Celsius.
  float getTemperatureCelsius();

  /// State-of-Health percentage (0-100).
  uint16_t getStateOfHealth();

  /// True if the gauge was detected on the bus.
  bool isAvailable() const { return available_; }

 private:
  // Standard I2C address (7-bit)
  static constexpr uint8_t I2C_ADDR = 0x55;

  // BQ27220 Standard Commands (TI datasheet SLUSCH4 §12.1).  All commands
  // return little-endian 16-bit values.  The earlier register map in this
  // file was a half-port of the BQ27441's command layout and put SoC at
  // 0x1C — which on the BQ27220 is actually StandbyTimeToEmpty() in
  // minutes.  An idle / charging device reports 0xFFFF (65535) there
  // because "no measurable load" maps to infinite time-to-empty, and
  // HalPowerManager clamps anything >100 down to 100 — producing the
  // "battery stuck at 100%" symptom users have been reporting.
  enum Register : uint8_t {
    REG_CONTROL          = 0x00,
    REG_AT_RATE          = 0x02,
    REG_AT_RATE_TTE      = 0x04,
    REG_TEMPERATURE      = 0x06,
    REG_VOLTAGE          = 0x08,
    REG_BATTERY_STATUS   = 0x0A,
    REG_CURRENT          = 0x0C,
    REG_REM_CAPACITY     = 0x10,
    REG_FULL_CHG_CAP     = 0x12,
    REG_AVG_CURRENT      = 0x14,
    REG_TTE              = 0x16,
    REG_TTF              = 0x18,
    REG_STANDBY_CUR      = 0x1A,
    REG_STANDBY_TTE      = 0x1C,
    REG_MAX_LOAD_CUR     = 0x1E,
    REG_MAX_LOAD_TTE     = 0x20,
    REG_AVAIL_ENERGY     = 0x24,
    REG_AVG_POWER        = 0x26,
    REG_INT_TEMP         = 0x28,
    REG_CYCLE_COUNT      = 0x2A,
    REG_SOC              = 0x2C,
    REG_SOH              = 0x2E,
    REG_CHARGE_VOLTAGE   = 0x30,
    REG_CHARGE_CURRENT   = 0x32,
    REG_DESIGN_CAPACITY  = 0x3C,
  };

  /// Read a 16-bit little-endian register.
  uint16_t readWord(uint8_t reg);

  uint8_t sdaPin_;
  uint8_t sclPin_;
  bool available_ = false;
};
