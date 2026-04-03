// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "SleepScreenCache.h"

SleepScreenCache SleepScreenCache::instance;

bool SleepScreenCache::isValid(const std::string& bookPath, uint8_t coverMode, uint8_t coverFilter) const {
  if (!loaded) return false;
  return cachedBookPath == bookPath && cachedCoverMode == coverMode && cachedCoverFilter == coverFilter;
}

void SleepScreenCache::markValid(const std::string& bookPath, uint8_t coverMode, uint8_t coverFilter) {
  cachedBookPath = bookPath;
  cachedCoverMode = coverMode;
  cachedCoverFilter = coverFilter;
  loaded = true;
}

void SleepScreenCache::invalidate() {
  loaded = false;
  cachedBookPath = "";
}
