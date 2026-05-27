#pragma once
#include "../Activity.h"

class AboutActivity final : public Activity {
 public:
  explicit AboutActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("About", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
