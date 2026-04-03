// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct WifiResult {
  bool connected = false;
  std::string ssid;
  std::string ip;
};

struct MenuResult {
  int action = -1;
  uint8_t orientation = 0;
  uint8_t pageTurnOption = 0;
};

struct ChapterResult {
  int spineIndex = 0;
};

struct PercentResult {
  int percent = 0;
};

struct PageResult {
  uint32_t page = 0;
};

struct SyncResult {
  int spineIndex = 0;
  int page = 0;
};

struct ActivityResult {
  bool isCancelled = false;

  explicit ActivityResult() = default;
};

using ActivityResultHandler = std::function<void(const ActivityResult&)>;
