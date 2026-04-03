// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "JsonSettingsIO.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Stream.h>

#include <string>

#include "AchievementsStore.h"
#include "ReadingStatsStore.h"

namespace {

class HalFileStream : public Stream {
 public:
  explicit HalFileStream(FsFile& file) : file(file) {}

  int available() override { return file.available() + (peekedByte >= 0 ? 1 : 0); }

  int read() override {
    if (peekedByte >= 0) {
      const int byte = peekedByte;
      peekedByte = -1;
      return byte;
    }
    return file.read();
  }

  int peek() override {
    if (peekedByte < 0) {
      peekedByte = file.read();
    }
    return peekedByte;
  }

  void flush() override { file.flush(); }

  size_t write(uint8_t value) override { return file.write(value); }

 private:
  FsFile& file;
  int peekedByte = -1;
};

bool saveJsonDocumentToFile(const char* moduleName, const char* path, const JsonDocument& doc) {
  FsFile file;
  if (!Storage.openFileForWrite(moduleName, path, file)) {
    LOG_ERR(moduleName, "Could not open JSON file for write: %s", path);
    return false;
  }

  const size_t written = serializeJson(doc, file);
  file.flush();
  file.close();
  return written > 0;
}

bool loadJsonDocumentFromFile(const char* moduleName, const char* path, JsonDocument& doc) {
  FsFile file;
  if (!Storage.openFileForRead(moduleName, path, file)) {
    LOG_ERR(moduleName, "Could not open JSON file for read: %s", path);
    return false;
  }

  HalFileStream stream(file);
  auto error = deserializeJson(doc, stream);
  file.close();
  if (error) {
    LOG_ERR(moduleName, "JSON parse error: %s", error.c_str());
    return false;
  }
  return true;
}
}  // namespace

// ---- ReadingStatsStore ----

bool JsonSettingsIO::saveReadingStats(const ReadingStatsStore& store, const char* path) {
  JsonDocument doc;
  doc["formatVersion"] = 2;

  JsonArray days = doc["readingDays"].to<JsonArray>();
  for (const auto& day : store.getReadingDays()) {
    JsonObject dayObj = days.add<JsonObject>();
    dayObj["dayOrdinal"] = day.dayOrdinal;
    dayObj["readingMs"] = day.readingMs;
  }

  JsonArray legacyDays = doc["legacyReadingDays"].to<JsonArray>();
  for (const auto& day : store.legacyReadingDays) {
    JsonObject dayObj = legacyDays.add<JsonObject>();
    dayObj["dayOrdinal"] = day.dayOrdinal;
    dayObj["readingMs"] = day.readingMs;
  }

  JsonArray books = doc["books"].to<JsonArray>();
  for (const auto& book : store.getBooks()) {
    JsonObject obj = books.add<JsonObject>();
    obj["path"] = book.path;
    obj["title"] = book.title;
    obj["author"] = book.author;
    obj["coverBmpPath"] = book.coverBmpPath;
    obj["chapterTitle"] = book.chapterTitle;
    obj["totalReadingMs"] = book.totalReadingMs;
    obj["sessions"] = book.sessions;
    obj["lastSessionMs"] = book.lastSessionMs;
    obj["firstReadAt"] = book.firstReadAt;
    obj["lastReadAt"] = book.lastReadAt;
    obj["lastProgressPercent"] = book.lastProgressPercent;
    obj["chapterProgressPercent"] = book.chapterProgressPercent;
    obj["completed"] = book.completed;

    JsonArray bookDays = obj["readingDays"].to<JsonArray>();
    for (const auto& day : book.readingDays) {
      JsonObject dayObj = bookDays.add<JsonObject>();
      dayObj["dayOrdinal"] = day.dayOrdinal;
      dayObj["readingMs"] = day.readingMs;
    }
  }

  return saveJsonDocumentToFile("RST", path, doc);
}

bool JsonSettingsIO::loadReadingStats(ReadingStatsStore& store, const char* json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("RST", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.books.clear();
  store.legacyReadingDays.clear();
  store.readingDays.clear();

  const uint32_t formatVersion = doc["formatVersion"] | static_cast<uint32_t>(1);

  auto appendReadingDays = [](std::vector<ReadingDayStats>& destination, JsonArray source) {
    for (JsonVariant value : source) {
      ReadingDayStats day;
      if (value.is<JsonObject>()) {
        JsonObject obj = value.as<JsonObject>();
        day.dayOrdinal = obj["dayOrdinal"] | static_cast<uint32_t>(0);
        day.readingMs = obj["readingMs"] | static_cast<uint64_t>(0);
      } else {
        day.dayOrdinal = value | static_cast<uint32_t>(0);
        day.readingMs = 0;
      }
      if (day.dayOrdinal != 0) {
        destination.push_back(day);
      }
    }
  };

  appendReadingDays(store.readingDays, doc["readingDays"].as<JsonArray>());
  if (formatVersion >= 2) {
    appendReadingDays(store.legacyReadingDays, doc["legacyReadingDays"].as<JsonArray>());
  } else {
    store.legacyReadingDays = store.readingDays;
  }

  JsonArray books = doc["books"].as<JsonArray>();
  for (JsonObject obj : books) {
    ReadingBookStats book;
    book.path = obj["path"] | std::string("");
    if (book.path.empty()) {
      continue;
    }
    book.title = obj["title"] | std::string("");
    book.author = obj["author"] | std::string("");
    book.coverBmpPath = obj["coverBmpPath"] | std::string("");
    book.chapterTitle = obj["chapterTitle"] | std::string("");
    book.totalReadingMs = obj["totalReadingMs"] | static_cast<uint64_t>(0);
    book.sessions = obj["sessions"] | static_cast<uint32_t>(0);
    book.lastSessionMs = obj["lastSessionMs"] | static_cast<uint32_t>(0);
    book.firstReadAt = obj["firstReadAt"] | static_cast<uint32_t>(0);
    book.lastReadAt = obj["lastReadAt"] | static_cast<uint32_t>(0);
    book.lastProgressPercent = obj["lastProgressPercent"] | static_cast<uint8_t>(0);
    book.chapterProgressPercent = obj["chapterProgressPercent"] | static_cast<uint8_t>(0);
    book.completed = obj["completed"] | false;
    if (formatVersion >= 2) {
      appendReadingDays(book.readingDays, obj["readingDays"].as<JsonArray>());
    }
    store.books.push_back(std::move(book));
  }

  store.rebuildAggregatedReadingDays();
  LOG_DBG("RST", "Reading stats loaded (%d books)", static_cast<int>(store.books.size()));
  return true;
}

bool JsonSettingsIO::loadReadingStatsFromFile(ReadingStatsStore& store, const char* path) {
  JsonDocument doc;
  if (!loadJsonDocumentFromFile("RST", path, doc)) {
    return false;
  }

  store.books.clear();
  store.legacyReadingDays.clear();
  store.readingDays.clear();

  const uint32_t formatVersion = doc["formatVersion"] | static_cast<uint32_t>(1);

  auto appendReadingDays = [](std::vector<ReadingDayStats>& destination, JsonArray source) {
    for (JsonVariant value : source) {
      ReadingDayStats day;
      if (value.is<JsonObject>()) {
        JsonObject obj = value.as<JsonObject>();
        day.dayOrdinal = obj["dayOrdinal"] | static_cast<uint32_t>(0);
        day.readingMs = obj["readingMs"] | static_cast<uint64_t>(0);
      } else {
        day.dayOrdinal = value | static_cast<uint32_t>(0);
        day.readingMs = 0;
      }
      if (day.dayOrdinal != 0) {
        destination.push_back(day);
      }
    }
  };

  appendReadingDays(store.readingDays, doc["readingDays"].as<JsonArray>());
  if (formatVersion >= 2) {
    appendReadingDays(store.legacyReadingDays, doc["legacyReadingDays"].as<JsonArray>());
  } else {
    store.legacyReadingDays = store.readingDays;
  }

  JsonArray books = doc["books"].as<JsonArray>();
  for (JsonObject obj : books) {
    ReadingBookStats book;
    book.path = obj["path"] | std::string("");
    if (book.path.empty()) {
      continue;
    }
    book.title = obj["title"] | std::string("");
    book.author = obj["author"] | std::string("");
    book.coverBmpPath = obj["coverBmpPath"] | std::string("");
    book.chapterTitle = obj["chapterTitle"] | std::string("");
    book.totalReadingMs = obj["totalReadingMs"] | static_cast<uint64_t>(0);
    book.sessions = obj["sessions"] | static_cast<uint32_t>(0);
    book.lastSessionMs = obj["lastSessionMs"] | static_cast<uint32_t>(0);
    book.firstReadAt = obj["firstReadAt"] | static_cast<uint32_t>(0);
    book.lastReadAt = obj["lastReadAt"] | static_cast<uint32_t>(0);
    book.lastProgressPercent = obj["lastProgressPercent"] | static_cast<uint8_t>(0);
    book.chapterProgressPercent = obj["chapterProgressPercent"] | static_cast<uint8_t>(0);
    book.completed = obj["completed"] | false;
    if (formatVersion >= 2) {
      appendReadingDays(book.readingDays, obj["readingDays"].as<JsonArray>());
    }
    store.books.push_back(std::move(book));
  }

  store.rebuildAggregatedReadingDays();
  LOG_DBG("RST", "Reading stats loaded from file (%d books)", static_cast<int>(store.books.size()));
  return true;
}

// ---- AchievementsStore ----

bool JsonSettingsIO::saveAchievements(const AchievementsStore& store, const char* path) {
  JsonDocument doc;
  doc["formatVersion"] = 1;
  doc["accumulatedReadingMs"] = store.accumulatedReadingMs;
  doc["countedSessions"] = store.countedSessions;
  doc["totalBookmarksAdded"] = store.totalBookmarksAdded;
  doc["longestSessionMs"] = store.longestSessionMs;
  doc["goalDaysCount"] = store.goalDaysCount;
  doc["currentGoalStreak"] = store.currentGoalStreak;
  doc["maxGoalStreak"] = store.maxGoalStreak;
  doc["lastGoalDayOrdinal"] = store.lastGoalDayOrdinal;
  doc["resetDayOrdinal"] = store.resetDayOrdinal;
  doc["resetDayBaselineMs"] = store.resetDayBaselineMs;
  doc["lastProcessedSessionSerial"] = store.lastProcessedSessionSerial;

  JsonArray states = doc["states"].to<JsonArray>();
  for (const auto& state : store.states) {
    JsonObject obj = states.add<JsonObject>();
    obj["unlocked"] = state.unlocked;
    obj["unlockedAt"] = state.unlockedAt;
  }

  JsonArray startedBooks = doc["startedBooks"].to<JsonArray>();
  for (const auto& pathValue : store.startedBooks) {
    startedBooks.add(pathValue);
  }

  JsonArray finishedBooks = doc["finishedBooks"].to<JsonArray>();
  for (const auto& pathValue : store.finishedBooks) {
    finishedBooks.add(pathValue);
  }

  return saveJsonDocumentToFile("ACH", path, doc);
}

bool JsonSettingsIO::loadAchievements(AchievementsStore& store, const char* json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("ACH", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.states = {};
  store.startedBooks.clear();
  store.finishedBooks.clear();
  store.pendingUnlocks.clear();

  store.accumulatedReadingMs = doc["accumulatedReadingMs"] | static_cast<uint64_t>(0);
  store.countedSessions = doc["countedSessions"] | static_cast<uint32_t>(0);
  store.totalBookmarksAdded = doc["totalBookmarksAdded"] | static_cast<uint32_t>(0);
  store.longestSessionMs = doc["longestSessionMs"] | static_cast<uint32_t>(0);
  store.goalDaysCount = doc["goalDaysCount"] | static_cast<uint32_t>(0);
  store.currentGoalStreak = doc["currentGoalStreak"] | static_cast<uint32_t>(0);
  store.maxGoalStreak = doc["maxGoalStreak"] | static_cast<uint32_t>(0);
  store.lastGoalDayOrdinal = doc["lastGoalDayOrdinal"] | static_cast<uint32_t>(0);
  store.resetDayOrdinal = doc["resetDayOrdinal"] | static_cast<uint32_t>(0);
  store.resetDayBaselineMs = doc["resetDayBaselineMs"] | static_cast<uint64_t>(0);
  store.lastProcessedSessionSerial = doc["lastProcessedSessionSerial"] | static_cast<uint32_t>(0);

  JsonArray states = doc["states"].as<JsonArray>();
  size_t stateIndex = 0;
  for (JsonObject obj : states) {
    if (stateIndex >= store.states.size()) break;
    store.states[stateIndex].unlocked = obj["unlocked"] | false;
    store.states[stateIndex].unlockedAt = obj["unlockedAt"] | static_cast<uint32_t>(0);
    ++stateIndex;
  }

  for (JsonVariant value : doc["startedBooks"].as<JsonArray>()) {
    const std::string pathValue = value | std::string("");
    if (!pathValue.empty()) store.startedBooks.push_back(pathValue);
  }

  for (JsonVariant value : doc["finishedBooks"].as<JsonArray>()) {
    const std::string pathValue = value | std::string("");
    if (!pathValue.empty()) store.finishedBooks.push_back(pathValue);
  }

  return true;
}

bool JsonSettingsIO::loadAchievementsFromFile(AchievementsStore& store, const char* path) {
  JsonDocument doc;
  if (!loadJsonDocumentFromFile("ACH", path, doc)) {
    return false;
  }

  store.states = {};
  store.startedBooks.clear();
  store.finishedBooks.clear();
  store.pendingUnlocks.clear();

  store.accumulatedReadingMs = doc["accumulatedReadingMs"] | static_cast<uint64_t>(0);
  store.countedSessions = doc["countedSessions"] | static_cast<uint32_t>(0);
  store.totalBookmarksAdded = doc["totalBookmarksAdded"] | static_cast<uint32_t>(0);
  store.longestSessionMs = doc["longestSessionMs"] | static_cast<uint32_t>(0);
  store.goalDaysCount = doc["goalDaysCount"] | static_cast<uint32_t>(0);
  store.currentGoalStreak = doc["currentGoalStreak"] | static_cast<uint32_t>(0);
  store.maxGoalStreak = doc["maxGoalStreak"] | static_cast<uint32_t>(0);
  store.lastGoalDayOrdinal = doc["lastGoalDayOrdinal"] | static_cast<uint32_t>(0);
  store.resetDayOrdinal = doc["resetDayOrdinal"] | static_cast<uint32_t>(0);
  store.resetDayBaselineMs = doc["resetDayBaselineMs"] | static_cast<uint64_t>(0);
  store.lastProcessedSessionSerial = doc["lastProcessedSessionSerial"] | static_cast<uint32_t>(0);

  JsonArray states = doc["states"].as<JsonArray>();
  size_t stateIndex = 0;
  for (JsonObject obj : states) {
    if (stateIndex >= store.states.size()) break;
    store.states[stateIndex].unlocked = obj["unlocked"] | false;
    store.states[stateIndex].unlockedAt = obj["unlockedAt"] | static_cast<uint32_t>(0);
    ++stateIndex;
  }

  for (JsonVariant value : doc["startedBooks"].as<JsonArray>()) {
    const std::string pathValue = value | std::string("");
    if (!pathValue.empty()) store.startedBooks.push_back(pathValue);
  }

  for (JsonVariant value : doc["finishedBooks"].as<JsonArray>()) {
    const std::string pathValue = value | std::string("");
    if (!pathValue.empty()) store.finishedBooks.push_back(pathValue);
  }

  return true;
}
