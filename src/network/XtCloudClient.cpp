#include "XtCloudClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_mac.h>

#include <algorithm>
#include <array>
#include <cctype>

#include "HttpDownloader.h"
#include "XteinkPairingInfo.h"
#include "util/UrlUtils.h"
#include <HalStorage.h>

namespace {
constexpr std::array<const char*, 2> kTaskApiBases = {
    "http://api-prod.xteink.com/api/v1",
    "http://api-prod.xteink.cc/api/v1",
};

constexpr std::array<const char*, 5> kAllowedSaveRoots = {
    "/Pushed Books/",
    "/Pushed Images/",
    "/Pushed Fonts/",
    "/XTCache/",
    "/sleep/",
};

bool endsWithInsensitive(const std::string& value, const char* suffix) {
  const size_t suffixLen = strlen(suffix);
  if (value.size() < suffixLen) {
    return false;
  }

  const size_t offset = value.size() - suffixLen;
  for (size_t i = 0; i < suffixLen; i++) {
    const unsigned char lhs = static_cast<unsigned char>(value[offset + i]);
    const unsigned char rhs = static_cast<unsigned char>(suffix[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }
  return true;
}

std::string readMessage(const JsonDocument& doc) {
  if (doc["message"].is<std::string>()) {
    return doc["message"].as<std::string>();
  }
  if (doc["msg"].is<std::string>()) {
    return doc["msg"].as<std::string>();
  }
  if (doc["error"].is<std::string>()) {
    return doc["error"].as<std::string>();
  }
  if (doc["data"]["message"].is<std::string>()) {
    return doc["data"]["message"].as<std::string>();
  }
  return "";
}
}  // namespace

std::string XtCloudClient::getDeviceId() {
  uint8_t mac[6] = {};
  readXteinkPairingMac(mac);
  return formatXteinkDeviceId(mac);
}

XtCloudClient::Error XtCloudClient::fetchTasks(TaskList& outTasks) {
  outTasks = {};
  lastDetail.clear();

  if (WiFi.status() != WL_CONNECTED) {
    lastDetail = "Wi-Fi not connected";
    return WIFI_NOT_CONNECTED;
  }

  const std::string deviceId = getDeviceId();
  Error lastError = HTTP_ERROR;

  for (const char* baseUrl : kTaskApiBases) {
    const std::string taskUrl = std::string(baseUrl) + "/device/tasks?limit=4&device_id=" + deviceId;

    std::string response;
    if (!HttpDownloader::fetchUrl(taskUrl, response)) {
      lastError = HTTP_ERROR;
      lastDetail = std::string("Fetch failed: ") + baseUrl;
      continue;
    }

    JsonDocument doc;
    const auto error = deserializeJson(doc, response);
    if (error) {
      LOG_ERR("XTCLOUD", "Task JSON parse failed from %s: %s", baseUrl, error.c_str());
      lastError = JSON_PARSE_ERROR;
      lastDetail = error.c_str();
      continue;
    }

    TaskList parsedTasks;
    parsedTasks.sourceBaseUrl = baseUrl;
    parsedTasks.serverMessage = readMessage(doc);

    if (doc["total"].is<int>()) {
      parsedTasks.total = doc["total"].as<int>();
    }
    if (doc["total_done"].is<int>()) {
      parsedTasks.totalDone = doc["total_done"].as<int>();
    }
    if (doc["total_pending"].is<int>()) {
      parsedTasks.totalPending = doc["total_pending"].as<int>();
    }

    if (doc["code"].is<int>() && doc["code"].as<int>() != 0 && !parsedTasks.serverMessage.empty()) {
      lastDetail = parsedTasks.serverMessage;
      return containsInsensitive(parsedTasks.serverMessage, "bind") ? BIND_REQUIRED : INVALID_RESPONSE;
    }

    JsonArrayConst taskArray;
    if (doc["tasks"].is<JsonArrayConst>()) {
      taskArray = doc["tasks"].as<JsonArrayConst>();
    } else if (doc["data"]["tasks"].is<JsonArrayConst>()) {
      taskArray = doc["data"]["tasks"].as<JsonArrayConst>();
    } else if (doc["data"].is<JsonArrayConst>()) {
      taskArray = doc["data"].as<JsonArrayConst>();
    } else if (doc.is<JsonArrayConst>()) {
      taskArray = doc.as<JsonArrayConst>();
    }

    if (taskArray.isNull()) {
      if (!parsedTasks.serverMessage.empty()) {
        lastDetail = parsedTasks.serverMessage;
        return containsInsensitive(parsedTasks.serverMessage, "bind") ? BIND_REQUIRED : INVALID_RESPONSE;
      }

      lastError = INVALID_RESPONSE;
      lastDetail = "No tasks array";
      continue;
    }

    for (JsonObjectConst taskObj : taskArray) {
      const std::string rawFileUrl =
          taskObj["file_url"].is<std::string>() ? taskObj["file_url"].as<std::string>() : "";
      if (rawFileUrl.empty()) {
        continue;
      }

      Task task;
      task.taskId = taskObj["task_id"].is<std::string>() ? taskObj["task_id"].as<std::string>() : "";
      if (task.taskId.empty() && taskObj["task_id"].is<int>()) {
        task.taskId = std::to_string(taskObj["task_id"].as<int>());
      }

      task.fileUrl = UrlUtils::buildUrl(baseUrl, rawFileUrl);
      const std::string rawSavePath =
          taskObj["save_path"].is<std::string>() ? taskObj["save_path"].as<std::string>() : "";
      task.savePath = normalizeSavePath(rawSavePath, task.fileUrl);
      task.displayName = basenameFromPathOrUrl(task.savePath.empty() ? task.fileUrl : task.savePath);
      parsedTasks.tasks.push_back(std::move(task));
    }

    if (parsedTasks.tasks.empty()) {
      outTasks = std::move(parsedTasks);
      lastDetail = outTasks.serverMessage;
      if (!lastDetail.empty() && containsInsensitive(lastDetail, "bind")) {
        return BIND_REQUIRED;
      }
      return NO_TASKS;
    }

    outTasks = std::move(parsedTasks);
    return OK;
  }

  return lastError;
}

XtCloudClient::Error XtCloudClient::downloadTask(const Task& task, std::string& savedPath, ProgressCallback progress) {
  lastDetail.clear();
  savedPath = normalizeSavePath(task.savePath, task.fileUrl);
  if (savedPath.empty()) {
    lastDetail = "Invalid save path";
    return FILE_ERROR;
  }

  if (!ensureParentDirectories(savedPath)) {
    lastDetail = "Failed to create parent directory";
    return FILE_ERROR;
  }

  const auto result = HttpDownloader::downloadToFile(task.fileUrl, savedPath, std::move(progress));
  switch (result) {
    case HttpDownloader::OK:
      LOG_DBG("XTCLOUD", "Downloaded %s -> %s", task.fileUrl.c_str(), savedPath.c_str());
      return OK;
    case HttpDownloader::FILE_ERROR:
      lastDetail = "Failed to write file";
      return FILE_ERROR;
    case HttpDownloader::HTTP_ERROR:
    case HttpDownloader::ABORTED:
    default:
      lastDetail = "HTTP download failed";
      return HTTP_ERROR;
  }
}

std::string XtCloudClient::basenameFromPathOrUrl(const std::string& value) {
  if (value.empty()) {
    return "";
  }

  std::string clean = value;
  const size_t queryPos = clean.find_first_of("?#");
  if (queryPos != std::string::npos) {
    clean.erase(queryPos);
  }

  const size_t slashPos = clean.find_last_of("/\\");
  if (slashPos == std::string::npos) {
    return clean;
  }
  return clean.substr(slashPos + 1);
}

bool XtCloudClient::containsInsensitive(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) {
    return true;
  }

  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                        [](const char lhs, const char rhs) {
                          return std::tolower(static_cast<unsigned char>(lhs)) ==
                                 std::tolower(static_cast<unsigned char>(rhs));
                        });
  return it != haystack.end();
}

std::string XtCloudClient::deriveSavePath(const std::string& fileUrl) {
  std::string fileName = basenameFromPathOrUrl(fileUrl);
  if (fileName.empty()) {
    fileName = "push.bin";
  }

  if (endsWithInsensitive(fileName, ".xth") || endsWithInsensitive(fileName, ".xtg") ||
      endsWithInsensitive(fileName, ".bmp") || endsWithInsensitive(fileName, ".png") ||
      endsWithInsensitive(fileName, ".jpg") || endsWithInsensitive(fileName, ".jpeg")) {
    return "/Pushed Images/" + fileName;
  }

  if (endsWithInsensitive(fileName, ".bin")) {
    return "/Pushed Fonts/" + fileName;
  }

  return "/Pushed Books/" + fileName;
}

bool XtCloudClient::ensureParentDirectories(const std::string& filePath) {
  const size_t slashPos = filePath.find_last_of('/');
  if (slashPos == std::string::npos || slashPos == 0) {
    return true;
  }
  const std::string parentPath = filePath.substr(0, slashPos);
  return Storage.ensureDirectoryExists(parentPath.c_str());
}

bool XtCloudClient::isAllowedSaveRoot(const std::string& path) {
  return std::any_of(kAllowedSaveRoots.begin(), kAllowedSaveRoots.end(),
                     [&path](const char* root) { return path.rfind(root, 0) == 0; });
}

std::string XtCloudClient::normalizeSavePath(const std::string& requestedPath, const std::string& fileUrl) {
  std::string normalized = requestedPath;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');

  if (normalized.empty() || normalized.back() == '/' || normalized.find("..") != std::string::npos) {
    normalized = deriveSavePath(fileUrl);
  }

  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }

  if (!isAllowedSaveRoot(normalized)) {
    normalized = deriveSavePath(fileUrl);
  }

  return normalized;
}
