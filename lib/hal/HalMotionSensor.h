#pragma once

#include <cstdint>

class HalMotionSensor {
 public:
  enum class PageTurnDirection : uint8_t { None, Previous, Next };
  enum class UiDirection : uint8_t { None, Left, Right, Up, Down };

  bool begin();
  bool isAvailable() const { return available_; }
  const char* chipName() const { return available_ ? "QMI8658" : "none"; }
  uint8_t address() const { return address_; }

  PageTurnDirection pollPageTurn(uint8_t mode, uint8_t orientation);
  UiDirection pollUiDirection(uint8_t orientation);
  void resetGestureState();
  bool consumeActivity();

 private:
  struct MotionSample {
    float accelX = 0.0f;
    float accelY = 0.0f;
    float gyroX = 0.0f;
    float gyroY = 0.0f;
  };

  bool probeAddress(uint8_t address);
  bool readReg(uint8_t reg, uint8_t& value);
  bool writeReg(uint8_t reg, uint8_t value);
  bool readMotionSample(MotionSample& sample);
  void noteCommFailure(const char* stage);
  void clearCommFailures();

  bool begun_ = false;
  bool available_ = false;
  bool activityPending_ = false;
  uint8_t address_ = 0;
  uint8_t consecutiveCommFailures_ = 0;
  float gyroWindow_[25] = {};
  uint8_t gyroWindowIndex_ = 0;
  uint8_t warmupSamples_ = 0;
  bool tiltHasBaseline_ = false;
  bool tiltArmed_ = true;
  MotionSample tiltBaselineSample_;
  float tiltLastPrimaryDelta_ = 0.0f;
  float tiltLastFallbackDelta_ = 0.0f;
  int8_t tiltFlickDirection_ = 0;
  unsigned long lastPollMs_ = 0;
  unsigned long tiltLastSampleMs_ = 0;
  unsigned long tiltReadyAtMs_ = 0;
  unsigned long tiltLastGestureMs_ = 0;
  unsigned long tiltFlickReadyUntilMs_ = 0;
  bool uiHasBaseline_ = false;
  bool uiArmed_ = true;
  MotionSample uiBaselineSample_;
  float uiLastScreenDeltaX_ = 0.0f;
  float uiLastScreenDeltaY_ = 0.0f;
  unsigned long uiLastPollMs_ = 0;
  unsigned long uiLastSampleMs_ = 0;
  unsigned long uiReadyAtMs_ = 0;
  unsigned long uiLastGestureMs_ = 0;
  unsigned long commSuspendUntilMs_ = 0;
};

extern HalMotionSensor motionSensor;
