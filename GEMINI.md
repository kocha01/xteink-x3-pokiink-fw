# Project: CrossPoint Halo 2 UI Port for Xteink X3

## 🎯 Overview
Porting the **Halo 2 UI** (originally for Xteink X4) to the **Xteink X3** e-reader.
This project is a fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) with customizations for Thai language, improved typography, and UI enhancements.

## 🛠 Tech Stack
- **Framework:** PlatformIO
- **Microcontroller:** ESP32-C3 (Verify if X3 uses the same as X4)
- **Language:** C++
- **Key Files:** 
  - `platformio.ini`: Project configuration
  - `src/`: Source code
  - `lib/`: Libraries
  - `scripts/`: Build and utility scripts

## 📜 Development Rules & Guidelines
- **AI Coordination:** Refer to `AI_COORDINATION.md` for AI-assisted development protocols.
- **Style:** Adhere to `.clang-format`.
- **Porting Focus:** 
  - Adjust display drivers if X3 screen differs from X4.
  - Ensure UI elements scale correctly for X3 resolution.
  - Maintain Thai language support and typography improvements.

## 🚀 Quick Commands
- **Resume Session:** `gemini --resume`
- **Build:** `pio run`
- **Upload:** `pio run -t upload`
- **Monitor:** `pio run -t monitor`
