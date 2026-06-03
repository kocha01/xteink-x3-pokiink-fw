#include <HalGPIO.h>
#include <Logging.h>
#include <SPI.h>
#include <esp_ota_ops.h>

void HalGPIO::begin() {
  if constexpr (BoardConfig::Features::kHasSdPowerSwitch) {
    pinMode(BoardConfig::Pins::kSdPowerControl, OUTPUT);
    digitalWrite(BoardConfig::Pins::kSdPowerControl, HIGH);
    delay(5);
  }

  inputMgr.begin();
  SPI.begin(BoardConfig::Pins::kEpdSclk, BoardConfig::Pins::kSpiMiso, BoardConfig::Pins::kEpdMosi,
            BoardConfig::Pins::kEpdCs);
  if constexpr (BoardConfig::Features::kHasUsbDetectPin) {
    pinMode(BoardConfig::Pins::kUsbDetect, INPUT);
  }
}

void HalGPIO::update() { inputMgr.update(); }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

bool HalGPIO::isPowerButtonPhysicallyPressed() const {
  return digitalRead(BoardConfig::Pins::kPowerButton) == LOW;
}

bool HalGPIO::isUsbConnected() const {
  if constexpr (!BoardConfig::Features::kHasUsbDetectPin) {
    return false;
  }

  // On X4, U0RXD/GPIO20 reads HIGH when USB is connected.
  return digitalRead(BoardConfig::Pins::kUsbDetect) == HIGH;
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();
  // Diagnostic log: surfaces the actual hardware wake source so users
  // experiencing spontaneous wakes can capture it from Serial Monitor and
  // we don't have to guess.  EXT0/EXT1/GPIO/TIMER/TOUCH/ULP cover the
  // entire ESP32-C3 sleep wakeup space.
  LOG_DBG("GPIO",
          "Wakeup diagnostic: wakeupCause=%d resetReason=%d usbConnected=%d",
          static_cast<int>(wakeupCause), static_cast<int>(resetReason), usbConnected ? 1 : 0);

  // First-boot-after-flash detection takes precedence over every reset-reason
  // heuristic below: a fresh OTA write leaves otadata in state=NEW (the
  // crosspointreader.com web flasher writes exactly that), which we can read
  // back regardless of how the chip's reset register lies about its source.
  //
  // Without this, esptool hard_reset on ESP32-C3 comes back as
  // ESP_RST_POWERON + USB-connected — indistinguishable from a "plug-in to
  // charge" event — and the AfterUSBPower branch in main.cpp puts the device
  // straight into deep sleep.  Every web-flash → deep-sleep → user panic.
  // (See docs/x3-backport-handoff.md Critical Bug 5.)
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running) {
    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &otaState) == ESP_OK) {
      if (otaState == ESP_OTA_IMG_NEW || otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        return WakeupReason::AfterFlash;
      }
    }
  }

  if (!BoardConfig::Features::kHasUsbDetectPin) {
    if (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP) {
      return WakeupReason::PowerButton;
    }
    if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN) {
      return WakeupReason::AfterFlash;
    }
    if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON) {
      return WakeupReason::PowerButton;
    }
    return WakeupReason::Other;
  }

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
