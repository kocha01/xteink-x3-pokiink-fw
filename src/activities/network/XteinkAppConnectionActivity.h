#pragma once

#include <string>

#include "activities/Activity.h"

class NimBLECharacteristic;

class XteinkAppConnectionActivity final : public Activity {
  bool bleStarted = false;
  bool bleStartQueued = false;
  bool bleConnected = false;
  bool payloadReceived = false;
  bool redrawRequested = false;
  bool qrRendered = false;
  bool exitInputArmed = false;
  unsigned long qrRenderedMs = 0;

  std::string deviceId;
  std::string bleName;
  std::string pairingUrl;
  std::string lastPayloadSummary;
  NimBLECharacteristic* notifyCharacteristic = nullptr;

  void preparePairingInfo();
  void startBle();
  void stopBle();
  void markForRedraw();

 public:
  void onBleConnected();
  void onBleDisconnected();
  void onBleWrite(const std::string& payload);
  bool shouldRestartAdvertising() const { return bleStarted; }

  explicit XteinkAppConnectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("XteinkAppConnection", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return bleStarted; }
};
