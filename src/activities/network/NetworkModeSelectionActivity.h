#pragma once

#include <functional>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

enum class NetworkMode { JOIN_NETWORK, CONNECT_CALIBRE, CREATE_HOTSPOT, XTEINK_APP, XT_CLOUD };

/**
 * NetworkModeSelectionActivity presents the user with a choice:
 * - "Join a Network" - Connect to an existing WiFi network (STA mode)
 * - "Connect to Calibre" - Use Calibre wireless device transfers
 * - "Create Hotspot" - Create an Access Point that others can connect to (AP mode)
 *
 * XTEINK App Connection / XT Cloud remain implemented in code, but are
 * intentionally hidden from the device UI for now until the flow is stable.
 *
 * The onModeSelected callback is called with the user's choice.
 * The onCancel callback is called if the user presses back.
 */
class NetworkModeSelectionActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  int selectedIndex = 0;

 public:
  explicit NetworkModeSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NetworkModeSelection", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  void onModeSelected(NetworkMode mode);
  void onCancel();
};
