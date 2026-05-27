#pragma once

#include <string>

#include "activities/Activity.h"
#include "network/XtCloudClient.h"

class XtCloudActivity final : public Activity {
  enum State {
    WIFI_SELECTION,
    CHECKING_TASKS,
    DOWNLOADING,
    NO_TASKS,
    BIND_REQUIRED,
    FAILED,
    FINISHED,
  };

  static constexpr unsigned int UNINITIALIZED_PERCENTAGE = 111;

  State state = WIFI_SELECTION;
  XtCloudClient client;
  XtCloudClient::TaskList taskList;

  std::string connectedSsid;
  std::string connectedIP;
  std::string failureDetail;
  std::string currentItemName;
  bool preserveWifiOnExit = false;
  size_t currentTaskIndex = 0;
  size_t downloadedCount = 0;
  size_t currentDownloaded = 0;
  size_t currentTotal = 0;
  unsigned int lastRenderPercentage = UNINITIALIZED_PERCENTAGE;

  void onWifiSelectionComplete(bool success);
  void beginCloudSync();
  void finishToPairing();
  float getOverallProgress() const;
  void onDownloadProgress(size_t downloaded, size_t total);

 public:
  explicit XtCloudActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("XtCloud", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == CHECKING_TASKS || state == DOWNLOADING; }
  bool skipLoopDelay() override { return state == CHECKING_TASKS || state == DOWNLOADING; }
};
