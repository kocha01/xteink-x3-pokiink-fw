#include "HalPowerManager.h"

#include <BatteryMonitor.h>
#include <Logging.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cassert>

#include "HalGPIO.h"

HalPowerManager powerManager;  // Singleton instance

void HalPowerManager::begin() {
  if constexpr (BoardConfig::Features::kHasBatteryAdc) {
    pinMode(BoardConfig::Pins::kBatteryMonitorAdc, INPUT);
  }
#if defined(CROSSPOINT_BOARD_X3)
  // Release any GPIO holds set before deep sleep (SD power pin is held LOW during deep sleep
  // to keep the SD card powered off). Without this, SD init fails on wake.
  if constexpr (BoardConfig::Features::kHasSdPowerSwitch) {
    constexpr auto sdPowerPin = static_cast<gpio_num_t>(BoardConfig::Pins::kSdPowerControl);
    gpio_hold_dis(sdPowerPin);
    gpio_deep_sleep_hold_dis();
    // Power SD back on
    pinMode(BoardConfig::Pins::kSdPowerControl, OUTPUT);
    digitalWrite(BoardConfig::Pins::kSdPowerControl, CROSSPOINT_SD_POWER_ON_LEVEL);
    delay(CROSSPOINT_SD_POWER_SETTLE_MS);
  }
  fuelGaugeReady_ = fuelGauge_.begin();
  if (!fuelGaugeReady_) {
    LOG_ERR("PWR", "BQ27220 fuel gauge init failed — battery reads will return 0");
  }
#endif
  normalFreq = getCpuFrequencyMhz();
  modeMutex = xSemaphoreCreateMutex();
  assert(modeMutex != nullptr);
}

void HalPowerManager::setPowerSaving(bool enabled) {
  if (normalFreq <= 0) {
    return;  // invalid state
  }

  auto wifiMode = WiFi.getMode();
  if (wifiMode != WIFI_MODE_NULL) {
    // Wifi is active, force disabling power saving
    enabled = false;
  }

  // Note: We don't use mutex here to avoid too much overhead,
  // it's not very important if we read a slightly stale value for currentLockMode
  const LockMode mode = currentLockMode;

  if (mode == None && enabled && !isLowPower) {
    LOG_DBG("PWR", "Going to low-power mode");
    if (!setCpuFrequencyMhz(LOW_POWER_FREQ)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", LOW_POWER_FREQ);
      return;
    }
    isLowPower = true;

  } else if ((!enabled || mode != None) && isLowPower) {
    LOG_DBG("PWR", "Restoring normal CPU frequency");
    if (!setCpuFrequencyMhz(normalFreq)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", normalFreq);
      return;
    }
    isLowPower = false;
  }

  // Otherwise, no change needed
}

void HalPowerManager::startDeepSleep(HalGPIO& gpio) const {
  // Ensure that the power button has been released to avoid immediately turning back on if you're holding it
  while (gpio.isPowerButtonPhysicallyPressed()) {
    delay(50);
    gpio.update();
  }

  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    LOG_DBG("PWR", "Preparing X3 peripherals for deep sleep");
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    SPI.end();
    Wire.end();  // Tear down I2C to avoid leakage on SDA/SCL pins
    if constexpr (BoardConfig::Features::kHasSdPowerSwitch) {
      constexpr auto sdPowerPin = static_cast<gpio_num_t>(BoardConfig::Pins::kSdPowerControl);
      pinMode(BoardConfig::Pins::kSdPowerControl, OUTPUT);
      digitalWrite(BoardConfig::Pins::kSdPowerControl, LOW);
      delay(2);
      // Hold GPIO state during deep sleep so SD stays powered off
      gpio_hold_en(sdPowerPin);
      gpio_deep_sleep_hold_en();
    }
    // Use INPUT_PULLUP, not bare INPUT, for the wake pin.
    //
    // The POWER button is wired GPIO3 → GND through the switch.  With INPUT
    // (high-Z), the pin's idle state depends entirely on the external
    // pull-up resistor on the X3 PCB.  On units installed via Unlocker
    // WiFi MITM, the Xteink OEM bootloader stays in place and leaves the
    // pin's RTC-domain pull-up DISABLED when it hands off to our app —
    // different from the arduino-esp32 stage-2 bootloader (USB-flashed
    // case) which leaves the default RTC pull-up enabled.  Without the
    // RTC pull-up, leakage / coupling from adjacent pins occasionally
    // drags GPIO3 LOW for a few microseconds during deep sleep, which the
    // GPIO wakeup logic latches as a "button press" → device wakes by
    // itself even though the user touched nothing.
    //
    // gpio_set_pull_mode() reaches the RTC pull-up control directly, so
    // it survives deep sleep regardless of what the bootloader left
    // configured.  Belt-and-suspenders: keep pinMode(INPUT_PULLUP) for
    // the digital-domain config in case any other init clobbers it.
    pinMode(BoardConfig::Pins::kPowerButton, INPUT_PULLUP);
    gpio_set_pull_mode(static_cast<gpio_num_t>(BoardConfig::Pins::kPowerButton),
                       GPIO_PULLUP_ONLY);
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  // Arm the wakeup trigger *after* the button is released
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BoardConfig::Pins::kPowerButton, ESP_GPIO_WAKEUP_GPIO_LOW);
  // Enter Deep Sleep
  esp_deep_sleep_start();
}

bool HalPowerManager::startLightSleep(HalGPIO& gpio) const {
  while (gpio.isPowerButtonPhysicallyPressed()) {
    delay(20);
    gpio.update();
  }

  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    LOG_DBG("PWR", "Preparing X3 peripherals for light sleep");
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    SPI.end();
    Wire.end();
    if constexpr (BoardConfig::Features::kHasSdPowerSwitch) {
      pinMode(BoardConfig::Pins::kSdPowerControl, OUTPUT);
      digitalWrite(BoardConfig::Pins::kSdPowerControl, LOW);
      delay(2);
    }
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  gpio_wakeup_enable(static_cast<gpio_num_t>(BoardConfig::Pins::kPowerButton), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  LOG_DBG("PWR", "Entering light sleep");
  const esp_err_t err = esp_light_sleep_start();

  gpio_wakeup_disable(static_cast<gpio_num_t>(BoardConfig::Pins::kPowerButton));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

  if (err != ESP_OK) {
    LOG_ERR("PWR", "Light sleep failed: %d", static_cast<int>(err));
    return false;
  }

  LOG_DBG("PWR", "Exited light sleep");

  // Re-initialize peripherals after light sleep resume
  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    if constexpr (BoardConfig::Features::kHasSdPowerSwitch) {
      // Power SD card back on
      pinMode(BoardConfig::Pins::kSdPowerControl, OUTPUT);
      digitalWrite(BoardConfig::Pins::kSdPowerControl, CROSSPOINT_SD_POWER_ON_LEVEL);
      delay(CROSSPOINT_SD_POWER_SETTLE_MS);
    }
    // SPI and I2C will be re-initialized by display.begin() and fuel gauge access
  }

  return true;
}

uint16_t HalPowerManager::getBatteryPercentage() const {
  static_assert(BoardConfig::Features::kHasBatteryAdc || BoardConfig::Features::kHasFuelGauge,
                "Board must provide a battery telemetry source.");

#if defined(CROSSPOINT_BOARD_X3)
  if (fuelGaugeReady_) {
    uint16_t soc = const_cast<BQ27220&>(fuelGauge_).getStateOfCharge();
    if (soc > 100) soc = 100;
    return soc;
  }
  return 0;  // Fuel gauge not responding
#else
  static const BatteryMonitor battery = BatteryMonitor(BoardConfig::Pins::kBatteryMonitorAdc);
  return battery.readPercentage();
#endif
}

uint16_t HalPowerManager::getBatteryVoltage() const {
#if defined(CROSSPOINT_BOARD_X3)
  if (fuelGaugeReady_) {
    return const_cast<BQ27220&>(fuelGauge_).getVoltage();
  }
  return 0;
#else
  static const BatteryMonitor battery = BatteryMonitor(BoardConfig::Pins::kBatteryMonitorAdc);
  return battery.readMillivolts();
#endif
}

bool HalPowerManager::hasFuelGauge() const {
#if defined(CROSSPOINT_BOARD_X3)
  return fuelGaugeReady_;
#else
  return false;
#endif
}

HalPowerManager::Lock::Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  // Current limitation: only one lock at a time
  if (powerManager.currentLockMode != None) {
    LOG_ERR("PWR", "Lock already held, ignore");
    valid = false;
  } else {
    powerManager.currentLockMode = NormalSpeed;
    valid = true;
  }
  xSemaphoreGive(powerManager.modeMutex);
  if (valid) {
    // Immediately restore normal CPU frequency if currently in low-power mode
    powerManager.setPowerSaving(false);
  }
}

HalPowerManager::Lock::~Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  if (valid) {
    powerManager.currentLockMode = None;
  }
  xSemaphoreGive(powerManager.modeMutex);
}
