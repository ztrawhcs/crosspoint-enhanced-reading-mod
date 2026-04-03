// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <string>

// SleepScreenCache tracks whether the current sleep screen BMP cache is valid.
// The cache is stored at /.crosspoint/sleep_cover_cache.bmp and is invalidated
// when the open book path, cover mode, or cover filter setting changes.
class SleepScreenCache {
  static SleepScreenCache instance;

  std::string cachedBookPath;
  uint8_t cachedCoverMode = 255;
  uint8_t cachedCoverFilter = 255;
  bool loaded = false;

  SleepScreenCache() = default;

 public:
  SleepScreenCache(const SleepScreenCache&) = delete;
  SleepScreenCache& operator=(const SleepScreenCache&) = delete;

  static SleepScreenCache& getInstance() { return instance; }
  static constexpr const char* CACHE_PATH = "/.crosspoint/sleep_cover_cache.bmp";

  // Returns true if the cache is valid for the current state
  bool isValid(const std::string& bookPath, uint8_t coverMode, uint8_t coverFilter) const;

  // Marks the cache as valid for the given state
  void markValid(const std::string& bookPath, uint8_t coverMode, uint8_t coverFilter);

  // Invalidates the cache
  void invalidate();
};

#define SLEEP_CACHE SleepScreenCache::getInstance()
