#include <Arduino.h>
#include <BoardConfig.h>
#include <HalDisplay.h>
#include <HalGPIO.h>

namespace {

HalGPIO gpio;
HalDisplay display;

const char* wakeReasonToString(const HalGPIO::WakeupReason reason) {
  switch (reason) {
    case HalGPIO::WakeupReason::PowerButton:
      return "PowerButton";
    case HalGPIO::WakeupReason::AfterFlash:
      return "AfterFlash";
    case HalGPIO::WakeupReason::AfterUSBPower:
      return "AfterUSBPower";
    case HalGPIO::WakeupReason::Other:
    default:
      return "Other";
  }
}

void logDisplayConfig() {
  Serial.printf("[X3] Board: %s\n", BoardConfig::kBoardName);
  Serial.printf("[X3] Wake reason: %s\n", wakeReasonToString(gpio.getWakeupReason()));
  Serial.printf("[X3] Backend framebuffer: %ux%u\n", display.getBackendWidth(), display.getBackendHeight());
  Serial.printf("[X3] Logical portrait: %ux%u\n", HalDisplay::LOGICAL_PORTRAIT_WIDTH,
                HalDisplay::LOGICAL_PORTRAIT_HEIGHT);
  Serial.printf("[X3] Expected board panel: %ux%u\n", HalDisplay::EXPECTED_PANEL_WIDTH,
                HalDisplay::EXPECTED_PANEL_HEIGHT);
  Serial.printf("[X3] Backend panel match: %s\n", display.backendMatchesBoardPanel() ? "yes" : "no");
  Serial.printf("[X3] SD power switch: %s\n", BoardConfig::Features::kHasSdPowerSwitch ? "yes" : "no");
}

void runDisplaySmokeTest() {
  Serial.println("[X3] Initializing display backend...");
  display.begin();

  Serial.println("[X3] Full refresh to black");
  display.clearScreen(0x00);
  display.displayBuffer(HalDisplay::FULL_REFRESH, false);
  delay(1200);

  Serial.println("[X3] Full refresh back to white");
  display.clearScreen(0xFF);
  display.displayBuffer(HalDisplay::FULL_REFRESH, false);

  if (!display.backendMatchesBoardPanel()) {
    Serial.println("[X3] WARNING: Running X3 with a fallback display backend geometry.");
    Serial.println("[X3] WARNING: This build is for pin and wake bring-up, not final panel tuning.");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("[X3] Halo 2 UI bring-up");

  gpio.begin();
  logDisplayConfig();
  runDisplaySmokeTest();

  Serial.println("[X3] Bring-up sequence complete. Looping for button activity logs.");
}

void loop() {
  gpio.update();

  if (gpio.wasAnyPressed()) {
    Serial.print("[X3] Button press:");
    for (uint8_t i = 0; i <= HalGPIO::BTN_POWER; ++i) {
      if (gpio.wasPressed(i)) {
        Serial.printf(" %u", i);
      }
    }
    Serial.println();
  }

  delay(20);
}
