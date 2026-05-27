#pragma once

#include <HalDisplay.h>

#include <string>

#include "../Activity.h"
#include "MappedInputManager.h"

class XthViewerActivity final : public Activity {
 public:
  XthViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::string filePath;

  void showError(const char* message, HalDisplay::RefreshMode refreshMode);
};
