#pragma once

#include <cstdint>

namespace BoardConfig {

enum class TargetBoard { X4, X3 };

#if defined(CROSSPOINT_BOARD_X3)
inline constexpr TargetBoard kTargetBoard = TargetBoard::X3;
inline constexpr char kBoardName[] = "Xteink X3";
#else
inline constexpr TargetBoard kTargetBoard = TargetBoard::X4;
inline constexpr char kBoardName[] = "Xteink X4";
#endif

namespace Pins {
#if defined(CROSSPOINT_BOARD_X3)
// Reverse-engineered from stock X3 firmware analysis.
inline constexpr uint8_t kEpdSclk = 8;
inline constexpr uint8_t kEpdMosi = 10;
inline constexpr uint8_t kEpdCs = 21;
inline constexpr uint8_t kEpdDc = 4;
inline constexpr uint8_t kEpdRst = 5;
inline constexpr uint8_t kEpdBusy = 6;
inline constexpr uint8_t kSpiMiso = 7;
inline constexpr uint8_t kSdCs = 12;
inline constexpr uint8_t kSdPowerControl = 13;
inline constexpr uint8_t kButtonAdcPrimary = 1;
inline constexpr uint8_t kButtonAdcSecondary = 2;
inline constexpr uint8_t kPowerButton = 3;
inline constexpr uint8_t kI2cScl = 0;
inline constexpr uint8_t kI2cSda = 20;
inline constexpr uint8_t kBatteryMonitorAdc = 0xFF;
inline constexpr uint8_t kUsbDetect = 0xFF;
#else
inline constexpr uint8_t kEpdSclk = 8;
inline constexpr uint8_t kEpdMosi = 10;
inline constexpr uint8_t kEpdCs = 21;
inline constexpr uint8_t kEpdDc = 4;
inline constexpr uint8_t kEpdRst = 5;
inline constexpr uint8_t kEpdBusy = 6;
inline constexpr uint8_t kSpiMiso = 7;
inline constexpr uint8_t kButtonAdcPrimary = 1;
inline constexpr uint8_t kButtonAdcSecondary = 2;
inline constexpr uint8_t kPowerButton = 3;
inline constexpr uint8_t kBatteryMonitorAdc = 0;
inline constexpr uint8_t kUsbDetect = 20;
#endif
}  // namespace Pins

namespace Features {
#if defined(CROSSPOINT_BOARD_X3)
inline constexpr bool kHasBatteryAdc = false;
inline constexpr bool kHasFuelGauge = true;
inline constexpr bool kHasUsbDetectPin = false;
inline constexpr bool kHasSdPowerSwitch = true;
#else
inline constexpr bool kHasBatteryAdc = true;
inline constexpr bool kHasFuelGauge = false;
inline constexpr bool kHasUsbDetectPin = true;
inline constexpr bool kHasSdPowerSwitch = false;
#endif
}  // namespace Features

namespace Display {
#if defined(CROSSPOINT_BOARD_X3)
inline constexpr uint16_t kLogicalPortraitWidth = 528;
inline constexpr uint16_t kLogicalPortraitHeight = 792;
inline constexpr uint16_t kPhysicalPanelWidth = 792;
inline constexpr uint16_t kPhysicalPanelHeight = 528;
#else
inline constexpr uint16_t kLogicalPortraitWidth = 480;
inline constexpr uint16_t kLogicalPortraitHeight = 800;
inline constexpr uint16_t kPhysicalPanelWidth = 800;
inline constexpr uint16_t kPhysicalPanelHeight = 480;
#endif
}  // namespace Display

}  // namespace BoardConfig
