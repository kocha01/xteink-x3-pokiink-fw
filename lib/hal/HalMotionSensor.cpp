#include "HalMotionSensor.h"

#include <Arduino.h>
#include <BoardConfig.h>
#include <Logging.h>
#include <Wire.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

HalMotionSensor motionSensor;

namespace {

constexpr uint8_t kQmi8658PrimaryAddr = 0x6B;
constexpr uint8_t kQmi8658FallbackAddr = 0x6A;
constexpr uint8_t kQmi8658WhoAmIReg = 0x00;
constexpr uint8_t kQmi8658WhoAmI = 0x05;
constexpr uint8_t kQmi8658Ctrl1Reg = 0x02;
constexpr uint8_t kQmi8658Ctrl2Reg = 0x03;
constexpr uint8_t kQmi8658Ctrl3Reg = 0x04;
constexpr uint8_t kQmi8658Ctrl7Reg = 0x08;
constexpr uint8_t kQmi8658Status0Reg = 0x2E;
constexpr uint8_t kQmi8658AccelXLowReg = 0x35;

constexpr uint8_t kMotionModeOff = 0;
constexpr uint8_t kMotionModeLrLight = 2;
constexpr uint8_t kMotionModeRightHeavy = 3;
constexpr uint8_t kMotionModeRightLight = 4;
constexpr uint8_t kMotionModeTiltLr = 5;

constexpr uint8_t kOrientationPortrait = 0;
constexpr uint8_t kOrientationLandscapeCw = 1;
constexpr uint8_t kOrientationInverted = 2;

constexpr unsigned long kPollIntervalMs = 15;
constexpr unsigned long kTiltSettleMs = 500;
constexpr unsigned long kTiltCooldownMs = 650;
constexpr unsigned long kTiltFlickWindowMs = 180;
constexpr unsigned long kUiSettleMs = 350;
constexpr unsigned long kUiCooldownMs = 500;
constexpr uint8_t kWindowSampleCount = 25;
constexpr uint8_t kWarmupSampleCount = kWindowSampleCount * 2;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint8_t kMaxCommFailures = 4;
constexpr unsigned long kCommCooldownMs = 1500;

// Stock X3 configures accel as +/-8g and gyro as +/-512dps. The stock gesture
// detector uses accel only for orientation and gyro swing for the page event.
constexpr int32_t kQmi8658LsbPerG = 4096;
constexpr float kGravityMps2 = 9.807f;
constexpr float kQmi8658GyroLsbPerDps = 64.0f;
constexpr float kHeavyTriggerDps = 700.0f;
constexpr float kLightTriggerDps = 390.0f;
constexpr float kDirectionCenterDps = -20.0f;
constexpr float kSideAccelMps2 = 5.0f;
constexpr float kTiltTriggerMps2 = 2.0f;
constexpr float kTiltReturnMps2 = 0.75f;
constexpr float kTiltFlickGyroDps = 300.0f;
constexpr float kTiltVelocityTriggerMps3 = 12.0f;
constexpr float kUiTriggerHorizontalMps2 = 2.0f;
constexpr float kUiTriggerVerticalMps2 = 2.6f;
constexpr float kUiReturnMps2 = 0.85f;
constexpr float kUiAxisSeparationMps2 = 0.45f;

int16_t readLe16(const uint8_t lo, const uint8_t hi) {
  return static_cast<int16_t>(static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
}

float rawAccelToMps2(const int16_t raw) {
  return (static_cast<float>(raw) * kGravityMps2) / static_cast<float>(kQmi8658LsbPerG);
}

float rawGyroToDps(const int16_t raw) {
  return static_cast<float>(raw) / kQmi8658GyroLsbPerDps;
}

bool isRightOnlyMode(const uint8_t mode) {
  return mode == kMotionModeRightHeavy || mode == kMotionModeRightLight;
}

bool isLightMode(const uint8_t mode) {
  return mode == kMotionModeLrLight || mode == kMotionModeRightLight;
}

bool isTiltMode(const uint8_t mode) {
  return mode == kMotionModeTiltLr;
}

void mapToScreenAxes(const float rawDx, const float rawDy, const uint8_t orientation, float& screenDx, float& screenDy) {
  switch (orientation) {
    case kOrientationPortrait:
      screenDx = rawDx;
      screenDy = rawDy;
      break;
    case kOrientationLandscapeCw:
      screenDx = -rawDy;
      screenDy = rawDx;
      break;
    case kOrientationInverted:
      screenDx = -rawDx;
      screenDy = -rawDy;
      break;
    default:
      screenDx = rawDy;
      screenDy = -rawDx;
      break;
  }
}

}  // namespace

bool HalMotionSensor::begin() {
#if defined(CROSSPOINT_BOARD_X3)
  const unsigned long now = millis();
  if (commSuspendUntilMs_ != 0 && now < commSuspendUntilMs_) {
    return false;
  }

  if (begun_) {
    return available_;
  }

  begun_ = true;
  available_ = false;
  address_ = 0;

  Wire.begin(BoardConfig::Pins::kI2cSda, BoardConfig::Pins::kI2cScl);
  Wire.setClock(400000);
  Wire.setTimeOut(kI2cTimeoutMs);

  if (!probeAddress(kQmi8658PrimaryAddr) && !probeAddress(kQmi8658FallbackAddr)) {
    LOG_ERR("IMU", "QMI8658 not detected at 0x%02X or 0x%02X", kQmi8658PrimaryAddr, kQmi8658FallbackAddr);
    return false;
  }

  // Matches stock X3 QMI8658 setup: CTRL1 auto-increment, accel +/-8g/ODR,
  // gyro +/-512dps/ODR, then enable accel + gyro in CTRL7.
  if (!writeReg(kQmi8658Ctrl1Reg, 0x60) || !writeReg(kQmi8658Ctrl2Reg, 0x26) ||
      !writeReg(kQmi8658Ctrl3Reg, 0x46) || !writeReg(kQmi8658Ctrl7Reg, 0x03)) {
    LOG_ERR("IMU", "QMI8658 init failed at 0x%02X", address_);
    available_ = false;
    return false;
  }

  available_ = true;
  clearCommFailures();
  resetGestureState();
  LOG_DBG("IMU", "QMI8658 ready at 0x%02X", address_);
  return true;
#else
  begun_ = true;
  available_ = false;
  return false;
#endif
}

bool HalMotionSensor::probeAddress(const uint8_t address) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  address_ = address;
  uint8_t whoAmI = 0;
  if (!readReg(kQmi8658WhoAmIReg, whoAmI) || whoAmI != kQmi8658WhoAmI) {
    LOG_ERR("IMU", "Unexpected WHO_AM_I at 0x%02X: 0x%02X", address, whoAmI);
    address_ = 0;
    return false;
  }

  return true;
}

bool HalMotionSensor::readReg(const uint8_t reg, uint8_t& value) {
  if (address_ == 0) {
    return false;
  }

  Wire.beginTransmission(address_);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    noteCommFailure("endTransmission(reg)");
    return false;
  }

  if (Wire.requestFrom(address_, static_cast<uint8_t>(1)) != 1) {
    noteCommFailure("requestFrom(reg)");
    return false;
  }

  value = Wire.read();
  return true;
}

bool HalMotionSensor::writeReg(const uint8_t reg, const uint8_t value) {
  if (address_ == 0) {
    return false;
  }

  Wire.beginTransmission(address_);
  Wire.write(reg);
  Wire.write(value);
  if (Wire.endTransmission() != 0) {
    noteCommFailure("writeReg");
    return false;
  }
  return true;
}

bool HalMotionSensor::readMotionSample(MotionSample& sample) {
  if (!available_) {
    return false;
  }

  uint8_t status = 0;
  if (!readReg(kQmi8658Status0Reg, status) || (status & 0x03) == 0) {
    return false;
  }

  Wire.beginTransmission(address_);
  Wire.write(kQmi8658AccelXLowReg);
  if (Wire.endTransmission(false) != 0) {
    noteCommFailure("endTransmission(sample)");
    return false;
  }

  uint8_t data[12] = {};
  if (Wire.requestFrom(address_, static_cast<uint8_t>(sizeof(data))) != sizeof(data)) {
    noteCommFailure("requestFrom(sample)");
    return false;
  }

  for (uint8_t& value : data) {
    value = Wire.read();
  }

  sample.accelX = rawAccelToMps2(readLe16(data[0], data[1]));
  sample.accelY = rawAccelToMps2(readLe16(data[2], data[3]));
  sample.gyroX = rawGyroToDps(readLe16(data[6], data[7]));
  sample.gyroY = rawGyroToDps(readLe16(data[8], data[9]));
  clearCommFailures();
  return true;
}

HalMotionSensor::PageTurnDirection HalMotionSensor::pollPageTurn(const uint8_t mode, const uint8_t orientation) {
  if (mode == kMotionModeOff) {
    return PageTurnDirection::None;
  }

  if (commSuspendUntilMs_ != 0 && millis() < commSuspendUntilMs_) {
    return PageTurnDirection::None;
  }

  if (!begin()) {
    return PageTurnDirection::None;
  }

  const unsigned long now = millis();
  if (now - lastPollMs_ < kPollIntervalMs) {
    return PageTurnDirection::None;
  }
  lastPollMs_ = now;

  MotionSample sample;
  if (!readMotionSample(sample)) {
    return PageTurnDirection::None;
  }

  float shakeRateDps = sample.gyroX;
  float sideAccelMps2 = sample.accelX;
  switch (orientation) {
    case kOrientationPortrait:
      shakeRateDps = -sample.gyroY;
      sideAccelMps2 = -sample.accelY;
      break;
    case kOrientationLandscapeCw:
      shakeRateDps = -sample.gyroX;
      sideAccelMps2 = -sample.accelX;
      break;
    case kOrientationInverted:
      shakeRateDps = sample.gyroY;
      sideAccelMps2 = sample.accelY;
      break;
    default:
      shakeRateDps = sample.gyroX;
      sideAccelMps2 = sample.accelX;
      break;
  }

  if (isTiltMode(mode)) {
    if (!tiltHasBaseline_) {
      tiltBaselineSample_ = sample;
      tiltHasBaseline_ = true;
      tiltLastPrimaryDelta_ = 0.0f;
      tiltLastFallbackDelta_ = 0.0f;
      tiltLastSampleMs_ = now;
      tiltFlickDirection_ = 0;
      tiltReadyAtMs_ = now + kTiltSettleMs;
      return PageTurnDirection::None;
    }

    const float rawDx = sample.accelX - tiltBaselineSample_.accelX;
    const float rawDy = sample.accelY - tiltBaselineSample_.accelY;

    // Prefer the expected left/right axis for the current screen orientation,
    // but fall back to the other horizontal axis if the real IMU mounting or a
    // flipped reading posture makes that axis move more.
    float primaryDelta = rawDx;
    float fallbackDelta = -rawDy;
    switch (orientation) {
      case kOrientationPortrait:
        primaryDelta = rawDx;
        fallbackDelta = -rawDy;
        break;
      case kOrientationLandscapeCw:
        primaryDelta = -rawDy;
        fallbackDelta = -rawDx;
        break;
      case kOrientationInverted:
        primaryDelta = -rawDx;
        fallbackDelta = rawDy;
        break;
      default:
        primaryDelta = rawDy;
        fallbackDelta = rawDx;
        break;
    }

    const float primaryAbs = std::fabs(primaryDelta);
    const float fallbackAbs = std::fabs(fallbackDelta);
    const bool useFallbackAxis = primaryAbs < kTiltTriggerMps2 && fallbackAbs >= kTiltTriggerMps2;
    float delta = useFallbackAxis ? fallbackDelta : primaryDelta;
    const float motionAbs = primaryAbs >= fallbackAbs ? primaryAbs : fallbackAbs;
    const float triggerAbs = useFallbackAxis ? fallbackAbs : primaryAbs;
    const float lastPrimaryAbs = std::fabs(tiltLastPrimaryDelta_);
    const float lastFallbackAbs = std::fabs(tiltLastFallbackDelta_);
    const float lastMotionAbs = lastPrimaryAbs >= lastFallbackAbs ? lastPrimaryAbs : lastFallbackAbs;
    float primaryVelocityMps3 = 0.0f;
    float fallbackVelocityMps3 = 0.0f;
    if (tiltLastSampleMs_ != 0 && now > tiltLastSampleMs_) {
      const float dtSec = static_cast<float>(now - tiltLastSampleMs_) / 1000.0f;
      primaryVelocityMps3 = (primaryDelta - tiltLastPrimaryDelta_) / dtSec;
      fallbackVelocityMps3 = (fallbackDelta - tiltLastFallbackDelta_) / dtSec;
    }

    tiltLastPrimaryDelta_ = primaryDelta;
    tiltLastFallbackDelta_ = fallbackDelta;
    tiltLastSampleMs_ = now;

    float gyroMotionDps = std::fabs(sample.gyroX);
    if (std::fabs(sample.gyroY) > gyroMotionDps) {
      gyroMotionDps = std::fabs(sample.gyroY);
    }

    const float primaryVelocityAbs = std::fabs(primaryVelocityMps3);
    const float fallbackVelocityAbs = std::fabs(fallbackVelocityMps3);
    const float tiltVelocityMps3 = primaryVelocityAbs >= fallbackVelocityAbs ? primaryVelocityMps3 : fallbackVelocityMps3;

    // Tilt page turns must start with a fast movement from below the trigger.
    // This keeps slow posture changes from becoming accidental page turns.
    if (lastMotionAbs < kTiltTriggerMps2 && gyroMotionDps >= kTiltFlickGyroDps &&
        std::fabs(tiltVelocityMps3) >= kTiltVelocityTriggerMps3) {
      tiltFlickReadyUntilMs_ = now + kTiltFlickWindowMs;
      tiltFlickDirection_ = tiltVelocityMps3 >= 0.0f ? 1 : -1;
    }

    if (motionAbs <= kTiltReturnMps2) {
      tiltArmed_ = true;
      tiltFlickReadyUntilMs_ = 0;
      tiltFlickDirection_ = 0;
      // Follow the user's natural holding angle only while close to neutral,
      // so lying down or one-handed reading does not permanently bias the trigger.
      tiltBaselineSample_.accelX = (tiltBaselineSample_.accelX * 7.0f + sample.accelX) / 8.0f;
      tiltBaselineSample_.accelY = (tiltBaselineSample_.accelY * 7.0f + sample.accelY) / 8.0f;
    }

    const bool hasRecentFlick = tiltFlickReadyUntilMs_ != 0 && now <= tiltFlickReadyUntilMs_;
    const int8_t tiltDirection = delta >= 0.0f ? 1 : -1;
    const bool flickDirectionMatches = tiltFlickDirection_ != 0 && tiltFlickDirection_ == tiltDirection;
    if (!tiltArmed_ || !hasRecentFlick || !flickDirectionMatches || now < tiltReadyAtMs_ ||
        now - tiltLastGestureMs_ < kTiltCooldownMs || triggerAbs < kTiltTriggerMps2) {
      return PageTurnDirection::None;
    }

    tiltArmed_ = false;
    tiltLastGestureMs_ = now;
    tiltFlickReadyUntilMs_ = 0;
    tiltFlickDirection_ = 0;
    activityPending_ = true;

    const PageTurnDirection direction = delta >= 0.0f ? PageTurnDirection::Next : PageTurnDirection::Previous;
    LOG_DBG("IMU", "Tilt velocity page turn: %s delta=%.2fmps2 velocity=%.1fmps3 gyro=%.1fdps x=%.2f y=%.2f",
            direction == PageTurnDirection::Next ? "next" : "prev", delta, tiltVelocityMps3, gyroMotionDps, rawDx,
            rawDy);
    return direction;
  }

  gyroWindow_[gyroWindowIndex_] = shakeRateDps;
  gyroWindowIndex_ = (gyroWindowIndex_ + 1) % kWindowSampleCount;
  if (warmupSamples_ < kWarmupSampleCount) {
    warmupSamples_++;
    return PageTurnDirection::None;
  }

  float minRate = gyroWindow_[0];
  float maxRate = gyroWindow_[0];
  for (uint8_t i = 1; i < kWindowSampleCount; i++) {
    if (gyroWindow_[i] < minRate) {
      minRate = gyroWindow_[i];
    }
    if (gyroWindow_[i] > maxRate) {
      maxRate = gyroWindow_[i];
    }
  }

  const float triggerDps = isLightMode(mode) ? kLightTriggerDps : kHeavyTriggerDps;
  if ((maxRate - minRate) <= triggerDps) {
    return PageTurnDirection::None;
  }

  activityPending_ = true;
  std::memset(gyroWindow_, 0, sizeof(gyroWindow_));
  gyroWindowIndex_ = 0;

  if (isRightOnlyMode(mode)) {
    LOG_DBG("IMU", "Stock-style shake page turn: next range=%.1fdps", maxRate - minRate);
    return PageTurnDirection::Next;
  }

  const bool negativeSwing = (minRate + maxRate) < kDirectionCenterDps;
  const bool sidePositive = sideAccelMps2 > kSideAccelMps2;
  const PageTurnDirection direction =
      (negativeSwing != sidePositive) ? PageTurnDirection::Next : PageTurnDirection::Previous;
  LOG_DBG("IMU", "Stock-style shake page turn: %s range=%.1fdps side=%.2fmps2",
          direction == PageTurnDirection::Next ? "next" : "prev", maxRate - minRate, sideAccelMps2);
  return direction;
}

HalMotionSensor::UiDirection HalMotionSensor::pollUiDirection(const uint8_t orientation) {
  if (commSuspendUntilMs_ != 0 && millis() < commSuspendUntilMs_) {
    return UiDirection::None;
  }

  if (!begin()) {
    return UiDirection::None;
  }

  const unsigned long now = millis();
  if (now - uiLastPollMs_ < kPollIntervalMs) {
    return UiDirection::None;
  }
  uiLastPollMs_ = now;

  MotionSample sample;
  if (!readMotionSample(sample)) {
    return UiDirection::None;
  }

  if (!uiHasBaseline_) {
    uiBaselineSample_ = sample;
    uiHasBaseline_ = true;
    uiArmed_ = true;
    uiLastScreenDeltaX_ = 0.0f;
    uiLastScreenDeltaY_ = 0.0f;
    uiLastSampleMs_ = now;
    uiReadyAtMs_ = now + kUiSettleMs;
    return UiDirection::None;
  }

  const float rawDx = sample.accelX - uiBaselineSample_.accelX;
  const float rawDy = sample.accelY - uiBaselineSample_.accelY;
  float screenDx = 0.0f;
  float screenDy = 0.0f;
  mapToScreenAxes(rawDx, rawDy, orientation, screenDx, screenDy);

  const float absX = std::fabs(screenDx);
  const float absY = std::fabs(screenDy);
  const float dominantAbs = absX >= absY ? absX : absY;
  const float secondaryAbs = absX >= absY ? absY : absX;

  uiLastScreenDeltaX_ = screenDx;
  uiLastScreenDeltaY_ = screenDy;
  uiLastSampleMs_ = now;

  if (dominantAbs <= kUiReturnMps2) {
    uiArmed_ = true;
    uiBaselineSample_.accelX = (uiBaselineSample_.accelX * 7.0f + sample.accelX) / 8.0f;
    uiBaselineSample_.accelY = (uiBaselineSample_.accelY * 7.0f + sample.accelY) / 8.0f;
    return UiDirection::None;
  }

  if (!uiArmed_ || now < uiReadyAtMs_ || now - uiLastGestureMs_ < kUiCooldownMs) {
    return UiDirection::None;
  }

  const bool horizontal = absX >= absY;
  const float trigger = horizontal ? kUiTriggerHorizontalMps2 : kUiTriggerVerticalMps2;
  if (dominantAbs < trigger || (dominantAbs - secondaryAbs) < kUiAxisSeparationMps2) {
    return UiDirection::None;
  }

  uiArmed_ = false;
  uiLastGestureMs_ = now;
  activityPending_ = true;

  const UiDirection direction =
      horizontal ? (screenDx >= 0.0f ? UiDirection::Right : UiDirection::Left)
                 : (screenDy >= 0.0f ? UiDirection::Down : UiDirection::Up);
  const char* directionName = "none";
  switch (direction) {
    case UiDirection::Left:
      directionName = "left";
      break;
    case UiDirection::Right:
      directionName = "right";
      break;
    case UiDirection::Up:
      directionName = "up";
      break;
    case UiDirection::Down:
      directionName = "down";
      break;
    default:
      break;
  }
  LOG_DBG("IMU", "Home tilt nav: %s dx=%.2f dy=%.2f", directionName, screenDx, screenDy);
  return direction;
}

void HalMotionSensor::resetGestureState() {
  std::memset(gyroWindow_, 0, sizeof(gyroWindow_));
  gyroWindowIndex_ = 0;
  warmupSamples_ = 0;
  tiltHasBaseline_ = false;
  tiltArmed_ = true;
  tiltBaselineSample_ = {};
  tiltLastPrimaryDelta_ = 0.0f;
  tiltLastFallbackDelta_ = 0.0f;
  tiltFlickDirection_ = 0;
  lastPollMs_ = 0;
  tiltLastSampleMs_ = 0;
  tiltReadyAtMs_ = 0;
  tiltLastGestureMs_ = 0;
  tiltFlickReadyUntilMs_ = 0;
  uiHasBaseline_ = false;
  uiArmed_ = true;
  uiBaselineSample_ = {};
  uiLastScreenDeltaX_ = 0.0f;
  uiLastScreenDeltaY_ = 0.0f;
  uiLastPollMs_ = 0;
  uiLastSampleMs_ = 0;
  uiReadyAtMs_ = 0;
  uiLastGestureMs_ = 0;
}

bool HalMotionSensor::consumeActivity() {
  const bool wasPending = activityPending_;
  activityPending_ = false;
  return wasPending;
}

void HalMotionSensor::noteCommFailure(const char* stage) {
  if (consecutiveCommFailures_ < 0xFF) {
    consecutiveCommFailures_++;
  }

  if (consecutiveCommFailures_ < kMaxCommFailures) {
    return;
  }

  LOG_ERR("IMU", "QMI8658 comm stalled at %s, suspending sensor for %lu ms", stage, kCommCooldownMs);
  available_ = false;
  begun_ = false;
  address_ = 0;
  commSuspendUntilMs_ = millis() + kCommCooldownMs;
  consecutiveCommFailures_ = 0;
  activityPending_ = false;
  resetGestureState();
  Wire.end();
}

void HalMotionSensor::clearCommFailures() {
  consecutiveCommFailures_ = 0;
  commSuspendUntilMs_ = 0;
}
