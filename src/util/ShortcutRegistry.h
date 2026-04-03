// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <I18nKeys.h>
#include <algorithm>
#include <functional>
#include <vector>

#include "CrossPointSettings.h"

enum class ShortcutId : uint8_t {
  BrowseFiles = 0,
  RecentBooks,
  Stats,
  SyncDay,
  ReadingHeatmap,
  Achievements,
  IfFound,
  ReadMe,
  FileTransfer,
  Sleep,
  Settings,
};

struct ShortcutEntry {
  ShortcutId id;
  uint8_t location;   // SHORTCUT_HOME or SHORTCUT_APPS
  uint8_t order;
  uint8_t visible;
  StrId labelId;
};

inline std::vector<ShortcutEntry> getAllShortcuts() {
  const auto& s = SETTINGS;
  return {
      {ShortcutId::BrowseFiles,     s.browseFilesShortcut,    s.browseFilesShortcutOrder,    s.browseFilesShortcutVisible,    StrId::STR_BROWSE_FILES},
      {ShortcutId::RecentBooks,     s.recentBooksShortcut,    s.recentBooksShortcutOrder,    s.recentBooksShortcutVisible,    StrId::STR_MENU_RECENT_BOOKS},
      {ShortcutId::Stats,           s.statsShortcut,          s.statsShortcutOrder,          s.statsShortcutVisible,          StrId::STR_READING_STATS},
      {ShortcutId::SyncDay,         s.syncDayShortcut,        s.syncDayShortcutOrder,        s.syncDayShortcutVisible,        StrId::STR_SYNC_DAY},
      {ShortcutId::ReadingHeatmap,  s.readingHeatmapShortcut, s.readingHeatmapShortcutOrder, s.readingHeatmapShortcutVisible, StrId::STR_READING_HEATMAP},
      {ShortcutId::Achievements,    s.achievementsShortcut,   s.achievementsShortcutOrder,   s.achievementsShortcutVisible,   StrId::STR_ACHIEVEMENTS},
      {ShortcutId::IfFound,         s.ifFoundShortcut,        s.ifFoundShortcutOrder,        s.ifFoundShortcutVisible,        StrId::STR_IF_FOUND},
      {ShortcutId::ReadMe,          s.readMeShortcut,         s.readMeShortcutOrder,         s.readMeShortcutVisible,         StrId::STR_README},
      {ShortcutId::FileTransfer,    s.fileTransferShortcut,   s.fileTransferShortcutOrder,   s.fileTransferShortcutVisible,   StrId::STR_FILE_TRANSFER},
      {ShortcutId::Sleep,           s.sleepShortcut,          s.sleepShortcutOrder,          s.sleepShortcutVisible,          StrId::STR_SLEEP},
      {ShortcutId::Settings,        s.settingsShortcut,       s.settingsShortcutOrder,       s.settingsShortcutVisible,       StrId::STR_SETTINGS_TITLE},
  };
}

// Returns visible shortcuts for a given location, sorted by order
inline std::vector<ShortcutEntry> getVisibleShortcuts(uint8_t location) {
  auto all = getAllShortcuts();
  std::vector<ShortcutEntry> result;
  for (const auto& entry : all) {
    if (entry.location == location && entry.visible) {
      result.push_back(entry);
    }
  }
  std::sort(result.begin(), result.end(), [](const ShortcutEntry& a, const ShortcutEntry& b) {
    return a.order < b.order;
  });
  return result;
}

// Returns all shortcuts for a given location, sorted by order (regardless of visibility)
inline std::vector<ShortcutEntry> getAllShortcutsForLocation(uint8_t location) {
  auto all = getAllShortcuts();
  std::vector<ShortcutEntry> result;
  for (const auto& entry : all) {
    if (entry.location == location) {
      result.push_back(entry);
    }
  }
  std::sort(result.begin(), result.end(), [](const ShortcutEntry& a, const ShortcutEntry& b) {
    return a.order < b.order;
  });
  return result;
}
