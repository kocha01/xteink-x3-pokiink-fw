#pragma once

#include <functional>
#include <string>
#include <vector>

class XtCloudClient {
 public:
  struct Task {
    std::string taskId;
    std::string fileUrl;
    std::string savePath;
    std::string displayName;
  };

  struct TaskList {
    int total = 0;
    int totalDone = 0;
    int totalPending = 0;
    std::string sourceBaseUrl;
    std::string serverMessage;
    std::vector<Task> tasks;
  };

  using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

  enum Error {
    OK = 0,
    WIFI_NOT_CONNECTED,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    INVALID_RESPONSE,
    NO_TASKS,
    FILE_ERROR,
    BIND_REQUIRED,
  };

  static std::string getDeviceId();

  Error fetchTasks(TaskList& outTasks);
  Error downloadTask(const Task& task, std::string& savedPath, ProgressCallback progress = nullptr);

  const std::string& getLastDetail() const { return lastDetail; }

 private:
  std::string lastDetail;

  static std::string basenameFromPathOrUrl(const std::string& value);
  static bool containsInsensitive(const std::string& haystack, const std::string& needle);
  static std::string deriveSavePath(const std::string& fileUrl);
  static bool ensureParentDirectories(const std::string& filePath);
  static bool isAllowedSaveRoot(const std::string& path);
  static std::string normalizeSavePath(const std::string& requestedPath, const std::string& fileUrl);
};
