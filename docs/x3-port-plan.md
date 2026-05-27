# Halo 2 UI X3 Port Plan

This document captures the current state of the X4 -> X3 porting effort for Halo 2 UI.

## What Already Looks Reusable

- Main application flow in `src/main.cpp`
- Activity stack and most UI code under `src/activities`
- Renderer orientation logic in `lib/GfxRenderer`
- EPUB and TXT reader paths
- SD-backed storage model and OTA partition expectations

The app layer is already reasonably isolated from hardware through the HAL under `lib/hal`.

## What Is Still X4-Specific

- `open-x4-sdk/libs/display/EInkDisplay`
- `lib/hal/HalDisplay.*`
- `lib/hal/HalPowerManager.*`
- Image conversion defaults in `lib/JpegToBmpConverter` and `lib/PngToBmpConverter`
- XTC support in `lib/Xtc` and `src/activities/reader/XtcReaderActivity.cpp`

## Confirmed Or Likely X3 Hardware Differences

- Same SoC family: ESP32-C3
- Same display control pins: GPIO 21/4/5/6 and shared SPI on 8/7/10
- Same button input topology appears likely: ADC buttons on GPIO 1 and 2, power on GPIO 3
- SD card still uses shared SPI, but CS is GPIO 12 and power control is GPIO 13
- Battery telemetry uses a fuel gauge instead of the X4 ADC divider model
- The stock X3 display is a different panel/driver configuration from X4
- Reverse-engineered X3 portrait geometry is roughly 528x792 logical, which is close to Halo 2 UI's current 480x800 assumptions

## Work Done In This Pass

- Added `lib/hal/BoardConfig.h` as a central place for board-specific pins and feature flags
- Moved the current HAL pin usage to `BoardConfig` so future X3 work is localized
- Captured X3 preview values in code without changing the default X4 build target
- Added a dedicated `env:x3_preview` PlatformIO target for X3-only work
- Added a dedicated `env:x3_bringup` PlatformIO target that sidesteps the unfinished app-level keyboard work and
  focuses on serial logging, wake reason, button logs, and a basic display smoke test
- Added compile-time SD power-switch support so X3 can enable the card before `SdFat` init
- Relaxed wake reason logic for boards that do not expose the old X4 USB-detect path
- Split display geometry into physical framebuffer dimensions vs board logical portrait dimensions so UI sizing can
  move toward X3 before the final panel backend is ready
- Replaced the local `EInkDisplay` implementation with the X3-capable panel path from the community SDK and switched
  `HalDisplay` to configure X3 mode at runtime
- Updated `GfxRenderer` buffer chunking to handle the X3 framebuffer size (`792x528`, 52272 bytes)
- Added an X3-only keyboard activity implementation so `env:x3_preview` can build and accept text input without
  touching the unfinished `src/activities/util/KeyboardEntryActivity.cpp`
- Updated X3 startup to bring up USB serial even without an X4-style USB detect pin, which restores early boot logs
- Scaled the X3 UI font mapping up one step so the denser `792x528` panel does not make Halo 2 UI look undersized

## Recommended Porting Order

1. Add an X3 display backend and switch `HalDisplay` to select X4 vs X3 driver at compile time.
2. Replace the X4 battery ADC path with an X3 fuel gauge backend.
3. Add SD power control support for X3 before relying on sleep/resume.
4. Revisit USB wake and "USB connected" assumptions because X3 does not expose the same detect path.
5. Update image conversion target sizes from fixed `480x800` to board-derived dimensions.
6. Decide whether XTC support should be disabled on X3 initially or adapted to scale 480x800 content.
7. Build an `env:x3` PlatformIO target once steps 1-4 are ready.
8. Replace the temporary X3 battery stub with a real BQ27220 implementation over I2C.

## Recommended MVP Scope

- Bring up display
- Buttons
- SD storage
- Battery percentage
- EPUB reader
- TXT reader
- Sleep and wake

Defer these until after the MVP:

- XTC reader
- Grayscale tuning
- Shake-to-turn
- NFC-dependent features

## Current Verification Status

- Stock X3 firmware was backed up before flashing:
  `/Users/nakarinkochapond/Documents/Xteink X3 FW/device-backups/x3-stock-20260410-213023`
- `env:x3_bringup` builds, flashes, and completes real X3 refreshes with the correct backend geometry:
  `792x528`, panel match = yes
- `env:x3_preview` now builds successfully by excluding the unfinished keyboard source and compiling the X3-only X3
  keyboard activity
- `env:x3_preview` has been flashed to the real X3 and boots through the normal app flow:
  SD card detected -> settings loaded -> display initialized -> Boot activity -> Home activity
- Serial logging now works on X3 preview builds over `/dev/cu.usbmodem1101`
- Current flash usage for `env:x3_preview` is about 86.4% of the app partition (roughly 5.66 MB / 6.55 MB)
- A fresh `env:x3_preview` rebuild with the usable keyboard and larger X3 UI font mapping passes locally; reflashing is
  currently blocked only because the device is not enumerating on USB

## Remaining X3-Specific Issues

- The X3 preview keyboard is currently English-first; Thai entry screens temporarily fall back to the same Latin input
  path until the unfinished upstream keyboard work is completed
- Battery reporting is still on the old stub path; the real X3 fuel gauge backend is not implemented yet
- XTC and image conversion paths still need a pass to become truly board-derived instead of X4-biased
- The idle Home screen appears to trigger repeated fast refreshes on X3; this is likely an input/update-loop issue and
  should be investigated before calling the port stable
