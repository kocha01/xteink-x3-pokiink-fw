#include "XtCloudActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
std::string truncateMiddle(std::string text, const size_t maxLen) {
  if (text.size() <= maxLen || maxLen < 8) {
    return text;
  }
  const size_t keep = (maxLen - 3) / 2;
  return text.substr(0, keep) + "..." + text.substr(text.size() - keep);
}
}  // namespace

void XtCloudActivity::onEnter() {
  Activity::onEnter();

  connectedSsid.clear();
  connectedIP.clear();
  failureDetail.clear();
  currentItemName.clear();
  preserveWifiOnExit = false;
  currentTaskIndex = 0;
  downloadedCount = 0;
  currentDownloaded = 0;
  currentTotal = 0;
  lastRenderPercentage = UNINITIALIZED_PERCENTAGE;
  taskList = {};
  requestUpdate();

  if (WiFi.status() == WL_CONNECTED) {
    connectedSsid = WiFi.SSID().c_str();
    connectedIP = WiFi.localIP().toString().c_str();
    beginCloudSync();
    return;
  }

  LOG_DBG("XTCLOUD", "Turning on WiFi for XT Cloud");
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             const auto& wifi = std::get<WifiResult>(result.data);
                             connectedSsid = wifi.ssid;
                             connectedIP = wifi.ip;
                           }
                           onWifiSelectionComplete(!result.isCancelled);
                         });
}

void XtCloudActivity::onExit() {
  Activity::onExit();

  if (preserveWifiOnExit) {
    LOG_DBG("XTCLOUD", "Preserving WiFi state while handing off to XTEINK pairing");
    return;
  }

  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

void XtCloudActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    finish();
    return;
  }

  beginCloudSync();
}

void XtCloudActivity::beginCloudSync() {
  {
    RenderLock lock(*this);
    state = CHECKING_TASKS;
    failureDetail.clear();
    currentItemName.clear();
    currentTaskIndex = 0;
    downloadedCount = 0;
    currentDownloaded = 0;
    currentTotal = 0;
    lastRenderPercentage = UNINITIALIZED_PERCENTAGE;
  }
  requestUpdateAndWait();

  const auto fetchResult = client.fetchTasks(taskList);
  if (fetchResult == XtCloudClient::NO_TASKS) {
    {
      RenderLock lock(*this);
      state = NO_TASKS;
      failureDetail = client.getLastDetail();
    }
    requestUpdate();
    return;
  }

  if (fetchResult == XtCloudClient::BIND_REQUIRED) {
    finishToPairing();
    return;
  }

  if (fetchResult != XtCloudClient::OK) {
    {
      RenderLock lock(*this);
      state = FAILED;
      failureDetail = client.getLastDetail();
    }
    requestUpdate();
    return;
  }

  {
    RenderLock lock(*this);
    state = DOWNLOADING;
    currentTaskIndex = 0;
    downloadedCount = 0;
    currentItemName = taskList.tasks.empty() ? "" : taskList.tasks.front().displayName;
  }
  requestUpdateAndWait();

  for (size_t i = 0; i < taskList.tasks.size(); i++) {
    currentTaskIndex = i;
    currentItemName = taskList.tasks[i].displayName;
    currentDownloaded = 0;
    currentTotal = 0;
    lastRenderPercentage = UNINITIALIZED_PERCENTAGE;
    requestUpdate();

    std::string savedPath;
    const auto result = client.downloadTask(taskList.tasks[i], savedPath, [this](size_t downloaded, size_t total) {
      onDownloadProgress(downloaded, total);
    });

    if (result != XtCloudClient::OK) {
      {
        RenderLock lock(*this);
        state = FAILED;
        failureDetail = client.getLastDetail();
      }
      requestUpdate();
      return;
    }

    downloadedCount = i + 1;
    currentDownloaded = currentTotal;
    requestUpdateAndWait();
  }

  {
    RenderLock lock(*this);
    state = FINISHED;
  }
  requestUpdate();
}

void XtCloudActivity::finishToPairing() {
  preserveWifiOnExit = true;
  setResult(XtCloudResult{true});
  finish();
}

float XtCloudActivity::getOverallProgress() const {
  if (taskList.tasks.empty()) {
    return 0.0f;
  }

  const float completed = static_cast<float>(downloadedCount);
  float currentFraction = 0.0f;
  if (currentTotal > 0) {
    currentFraction = static_cast<float>(currentDownloaded) / static_cast<float>(currentTotal);
  }
  return std::min(1.0f, (completed + currentFraction) / static_cast<float>(taskList.tasks.size()));
}

void XtCloudActivity::onDownloadProgress(const size_t downloaded, const size_t total) {
  currentDownloaded = downloaded;
  currentTotal = total;

  const unsigned int percentage = static_cast<unsigned int>(getOverallProgress() * 100.0f);
  if (lastRenderPercentage == UNINITIALIZED_PERCENTAGE || percentage / 2 != lastRenderPercentage / 2 ||
      (total > 0 && downloaded >= total)) {
    lastRenderPercentage = percentage;
    requestUpdate();
  }
}

void XtCloudActivity::loop() {
  if (state == FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      beginCloudSync();
    }
    return;
  }

  if (state == NO_TASKS) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      finishToPairing();
    }
    return;
  }

  if (state == BIND_REQUIRED || state == FINISHED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      finish();
    }
  }
}

void XtCloudActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_XT_CLOUD), nullptr);

  if (!connectedSsid.empty()) {
    const std::string connectionLine = truncateMiddle(connectedSsid + "  " + connectedIP, 34);
    GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                      connectionLine.c_str());
  }

  const int contentTop =
      metrics.topPadding + metrics.headerHeight + (!connectedSsid.empty() ? metrics.tabBarHeight : 0) +
      metrics.verticalSpacing * 2;
  const int centerY = (contentTop + pageHeight - metrics.buttonHintsHeight) / 2;

  if (state == CHECKING_TASKS) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_CHECKING_CLOUD_TASKS), true, EpdFontFamily::BOLD);
  } else if (state == DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop, tr(STR_DOWNLOADING), true, EpdFontFamily::BOLD);

    int y = contentTop + lineHeight + metrics.verticalSpacing;
    const int progressPercent = static_cast<int>(getOverallProgress() * 100.0f);
    GUI.drawProgressBar(
        renderer,
        Rect{metrics.contentSidePadding, y, pageWidth - metrics.contentSidePadding * 2, metrics.progressBarHeight},
        progressPercent, 100);

    y += metrics.progressBarHeight + metrics.verticalSpacing;
    renderer.drawCenteredText(
        UI_10_FONT_ID, y,
        (std::to_string(currentTaskIndex + 1) + "/" + std::to_string(taskList.tasks.size())).c_str());

    y += lineHeight + metrics.verticalSpacing;
    renderer.drawCenteredText(SMALL_FONT_ID, y, truncateMiddle(currentItemName, 34).c_str(), true);

    y += renderer.getLineHeight(SMALL_FONT_ID) + 4;
    if (currentTotal > 0) {
      renderer.drawCenteredText(
          SMALL_FONT_ID, y,
          (std::to_string(currentDownloaded) + " / " + std::to_string(currentTotal)).c_str(), true);
    } else if (currentDownloaded > 0) {
      renderer.drawCenteredText(SMALL_FONT_ID, y, (std::to_string(currentDownloaded) + " bytes").c_str(), true);
    }
  } else if (state == NO_TASKS) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight / 2, tr(STR_NO_CLOUD_TASKS), true,
                              EpdFontFamily::BOLD);
    if (!failureDetail.empty()) {
      renderer.drawCenteredText(SMALL_FONT_ID, centerY + lineHeight, truncateMiddle(failureDetail, 34).c_str(), true);
    }
  } else if (state == BIND_REQUIRED) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight / 2, tr(STR_BIND_APP_FIRST), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, centerY + lineHeight, tr(STR_SCAN_IN_XTEINK_APP), true);
  } else if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight / 2, tr(STR_CLOUD_SYNC_FAILED), true,
                              EpdFontFamily::BOLD);
    if (!failureDetail.empty()) {
      renderer.drawCenteredText(SMALL_FONT_ID, centerY + lineHeight, truncateMiddle(failureDetail, 34).c_str(), true);
    }
  } else if (state == FINISHED) {
    renderer.drawCenteredText(UI_10_FONT_ID, centerY - lineHeight / 2, tr(STR_CLOUD_SYNC_COMPLETE), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(
        SMALL_FONT_ID, centerY + lineHeight,
        (std::to_string(downloadedCount) + "/" + std::to_string(taskList.tasks.size()) + " files").c_str(), true);
  }

  if (state == FAILED) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == NO_TASKS) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONNECT), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == BIND_REQUIRED || state == FINISHED) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
