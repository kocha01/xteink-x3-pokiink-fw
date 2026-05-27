#include "XteinkAppConnectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <NimBLEDevice.h>
#include <esp32-hal-bt-mem.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/XteinkPairingInfo.h"
#include "util/QrUtils.h"

namespace {
constexpr unsigned long kBleStartDelayMs = 1500;
// Stock XTEINK firmware exposes the Nordic UART UUIDs in its app image.
constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // App writes to device.
constexpr const char* NUS_TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // Device notifies app.

XteinkAppConnectionActivity* activeActivity = nullptr;

std::string summarizePayload(const std::string& payload) {
  if (payload.empty()) {
    return "(empty)";
  }

  std::string summary;
  summary.reserve(std::min<size_t>(payload.size(), 32) * 3);
  for (size_t i = 0; i < payload.size() && i < 32; i++) {
    const unsigned char ch = static_cast<unsigned char>(payload[i]);
    if (ch >= 0x20 && ch <= 0x7E) {
      summary.push_back(static_cast<char>(ch));
    } else {
      char hex[5];
      snprintf(hex, sizeof(hex), "\\x%02X", ch);
      summary += hex;
    }
  }
  if (payload.size() > 32) {
    summary += "...";
  }
  return summary;
}

class XteinkBleServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override {
    if (activeActivity) {
      activeActivity->onBleConnected();
    }
  }

  void onDisconnect(NimBLEServer*) override {
    if (activeActivity) {
      activeActivity->onBleDisconnected();
    }
    if (activeActivity && activeActivity->shouldRestartAdvertising()) {
      NimBLEDevice::startAdvertising();
    }
  }
};

class XteinkBleWriteCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic) override {
    if (!activeActivity) {
      return;
    }
    activeActivity->onBleWrite(characteristic->getValue());
  }
};

XteinkBleServerCallbacks serverCallbacks;
XteinkBleWriteCallbacks writeCallbacks;
}  // namespace

void XteinkAppConnectionActivity::onEnter() {
  Activity::onEnter();
  activeActivity = this;
  preparePairingInfo();
  bleStartQueued = true;
  bleConnected = false;
  payloadReceived = false;
  lastPayloadSummary.clear();
  qrRendered = false;
  exitInputArmed = false;
  requestUpdate();
}

void XteinkAppConnectionActivity::onExit() {
  stopBle();
  activeActivity = nullptr;
  Activity::onExit();
}

void XteinkAppConnectionActivity::loop() {
  const bool exitButtonPressed =
      mappedInput.isPressed(MappedInputManager::Button::Back) ||
      mappedInput.isPressed(MappedInputManager::Button::Confirm);
  const bool exitButtonReleased =
      mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm);

  if (!exitInputArmed) {
    // Consume the Confirm release that opened this sub-activity in the parent menu.
    exitInputArmed = !exitButtonPressed && !exitButtonReleased;
  } else if (exitButtonReleased) {
    finish();
    return;
  }

  if (bleStartQueued && qrRendered && millis() - qrRenderedMs > kBleStartDelayMs) {
    bleStartQueued = false;
    startBle();
  }

  if (redrawRequested) {
    redrawRequested = false;
    requestUpdate();
  }
}

void XteinkAppConnectionActivity::preparePairingInfo() {
  const XteinkPairingInfo info = buildXteinkPairingInfo();
  deviceId = info.deviceId;
  bleName = info.bleName;
  pairingUrl = info.pairingUrl;

  LOG_DBG("XTBLE", "Official-style QR payload: %s", info.pairingJson.c_str());
}

void XteinkAppConnectionActivity::startBle() {
  if (bleStarted) {
    return;
  }

  LOG_DBG("XTBLE", "Starting BLE app pairing: name=%s id=%s url=%s", bleName.c_str(), deviceId.c_str(),
          pairingUrl.c_str());

  NimBLEDevice::init(bleName);
  // The phone is nearby during pairing; low TX power avoids brownout after
  // the e-ink panel has just performed a refresh.
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);

  NimBLEService* service = server->createService(NUS_SERVICE_UUID);
  notifyCharacteristic = service->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic* writeCharacteristic =
      service->createCharacteristic(NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  writeCharacteristic->setCallbacks(&writeCallbacks);

  notifyCharacteristic->setValue("READY");
  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(NUS_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  bleStarted = true;
}

void XteinkAppConnectionActivity::stopBle() {
  if (!bleStarted) {
    return;
  }

  bleStartQueued = false;
  bleStarted = false;
  bleConnected = false;
  notifyCharacteristic = nullptr;

  LOG_DBG("XTBLE", "Stopping BLE app pairing");
  NimBLEDevice::getAdvertising()->stop();
  NimBLEDevice::deinit(true);
}

void XteinkAppConnectionActivity::markForRedraw() { redrawRequested = true; }

void XteinkAppConnectionActivity::onBleConnected() {
  LOG_DBG("XTBLE", "BLE client connected");
  bleConnected = true;
  markForRedraw();
}

void XteinkAppConnectionActivity::onBleDisconnected() {
  LOG_DBG("XTBLE", "BLE client disconnected");
  bleConnected = false;
  markForRedraw();
}

void XteinkAppConnectionActivity::onBleWrite(const std::string& payload) {
  payloadReceived = true;
  lastPayloadSummary = summarizePayload(payload);
  LOG_DBG("XTBLE", "RX %u bytes: %s", static_cast<unsigned>(payload.size()), lastPayloadSummary.c_str());

  if (notifyCharacteristic) {
    notifyCharacteristic->setValue("OK");
    notifyCharacteristic->notify();
  }
  markForRedraw();
}

void XteinkAppConnectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int height10 = renderer.getLineHeight(UI_10_FONT_ID);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_XTEINK_APP_CONNECTION), nullptr);

  int startY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  renderer.drawCenteredText(UI_10_FONT_ID, startY, tr(STR_SCAN_IN_XTEINK_APP), true, EpdFontFamily::BOLD);
  startY += height10 + metrics.verticalSpacing;

  const int qrSize = std::min(240, pageWidth - metrics.contentSidePadding * 2);
  const Rect qrBounds((pageWidth - qrSize) / 2, startY, qrSize, qrSize);
  QrUtils::drawQrCode(renderer, qrBounds, pairingUrl);
  startY += qrSize + metrics.verticalSpacing;

  const char* status = tr(STR_BLE_WAITING);
  if (payloadReceived) {
    status = tr(STR_BLE_RECEIVED);
  } else if (bleConnected) {
    status = tr(STR_BLE_CONNECTED);
  }

  renderer.drawCenteredText(UI_10_FONT_ID, startY, status, true);
  startY += height10 + 4;

  const std::string nameLine = std::string(tr(STR_BLE_DEVICE_NAME)) + " " + bleName;
  renderer.drawCenteredText(SMALL_FONT_ID, startY, nameLine.c_str(), true);
  startY += renderer.getLineHeight(SMALL_FONT_ID) + 2;

  const std::string idLine = std::string(tr(STR_DEVICE_ID)) + " " + deviceId;
  renderer.drawCenteredText(SMALL_FONT_ID, startY, idLine.c_str(), true);
  startY += renderer.getLineHeight(SMALL_FONT_ID) + 2;

  if (payloadReceived) {
    const std::string payloadLine = std::string("RX: ") + lastPayloadSummary;
    renderer.drawCenteredText(SMALL_FONT_ID, startY, payloadLine.c_str(), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_EXIT), tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
  if (!qrRendered) {
    qrRendered = true;
    qrRenderedMs = millis();
  }
}
