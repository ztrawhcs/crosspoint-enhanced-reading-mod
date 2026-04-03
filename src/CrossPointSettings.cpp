#include "CrossPointSettings.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>
#include <string>

#include "fontIds.h"

// Initialize the static instance
CrossPointSettings CrossPointSettings::instance;

void readAndValidate(FsFile& file, uint8_t& member, const uint8_t maxValue) {
  uint8_t tempValue;
  serialization::readPod(file, tempValue);
  if (tempValue < maxValue) {
    member = tempValue;
  }
}

namespace {
constexpr uint8_t SETTINGS_FILE_VERSION = 1;
// SETTINGS_COUNT is now calculated automatically in saveToFile
constexpr char SETTINGS_FILE[] = "/.crosspoint/settings.bin";

void validateFrontButtonMapping(CrossPointSettings& settings) {
  const uint8_t mapping[] = {settings.frontButtonBack, settings.frontButtonConfirm, settings.frontButtonLeft,
                             settings.frontButtonRight};
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = i + 1; j < 4; j++) {
      if (mapping[i] == mapping[j]) {
        settings.frontButtonBack = CrossPointSettings::FRONT_HW_BACK;
        settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_CONFIRM;
        settings.frontButtonLeft = CrossPointSettings::FRONT_HW_LEFT;
        settings.frontButtonRight = CrossPointSettings::FRONT_HW_RIGHT;
        return;
      }
    }
  }
}

void applyLegacyFrontButtonLayout(CrossPointSettings& settings) {
  switch (static_cast<CrossPointSettings::FRONT_BUTTON_LAYOUT>(settings.frontButtonLayout)) {
    case CrossPointSettings::LEFT_RIGHT_BACK_CONFIRM:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_RIGHT;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_CONFIRM;
      break;
    case CrossPointSettings::LEFT_BACK_CONFIRM_RIGHT:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_RIGHT;
      break;
    case CrossPointSettings::BACK_CONFIRM_RIGHT_LEFT:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_RIGHT;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_LEFT;
      break;
    case CrossPointSettings::BACK_CONFIRM_LEFT_RIGHT:
    default:
      settings.frontButtonBack = CrossPointSettings::FRONT_HW_BACK;
      settings.frontButtonConfirm = CrossPointSettings::FRONT_HW_CONFIRM;
      settings.frontButtonLeft = CrossPointSettings::FRONT_HW_LEFT;
      settings.frontButtonRight = CrossPointSettings::FRONT_HW_RIGHT;
      break;
  }
}
}  // namespace

class SettingsWriter {
 public:
  bool is_counting = false;
  uint8_t item_count = 0;
  template <typename T>

  void writeItem(FsFile& file, const T& value) {
    if (is_counting) {
      item_count++;
    } else {
      serialization::writePod(file, value);
    }
  }

  void writeItemString(FsFile& file, const char* value) {
    if (is_counting) {
      item_count++;
    } else {
      serialization::writeString(file, std::string(value));
    }
  }
};

uint8_t CrossPointSettings::writeSettings(FsFile& file, bool count_only) const {
  SettingsWriter writer;
  writer.is_counting = count_only;

  writer.writeItem(file, sleepScreen);
  writer.writeItem(file, extraParagraphSpacing);
  writer.writeItem(file, shortPwrBtn);
  writer.writeItem(file, statusBar);
  writer.writeItem(file, orientation);
  writer.writeItem(file, frontButtonLayout);  // legacy
  writer.writeItem(file, sideButtonLayout);
  writer.writeItem(file, fontFamily);
  writer.writeItem(file, fontSize);
  writer.writeItem(file, lineSpacing);
  writer.writeItem(file, paragraphAlignment);
  writer.writeItem(file, sleepTimeout);
  writer.writeItem(file, refreshFrequency);
  writer.writeItem(file, screenMargin);
  writer.writeItem(file, sleepScreenCoverMode);
  writer.writeItemString(file, opdsServerUrl);
  writer.writeItem(file, textAntiAliasing);
  writer.writeItem(file, hideBatteryPercentage);
  writer.writeItem(file, longPressChapterSkip);
  writer.writeItem(file, hyphenationEnabled);
  writer.writeItemString(file, opdsUsername);
  writer.writeItemString(file, opdsPassword);
  writer.writeItem(file, sleepScreenCoverFilter);
  writer.writeItem(file, uiTheme);
  writer.writeItem(file, frontButtonBack);
  writer.writeItem(file, frontButtonConfirm);
  writer.writeItem(file, frontButtonLeft);
  writer.writeItem(file, frontButtonRight);
  writer.writeItem(file, fadingFix);
  writer.writeItem(file, embeddedStyle);
  // New fields need to be added at end for backward compatibility
  writer.writeItem(file, buttonModMode);
  writer.writeItem(file, forceBoldText);
  writer.writeItem(file, swapPortraitControls);
  writer.writeItem(file, swapLandscapeControls);
  // Stats and time settings (added for cpr-vcodex merge)
  writer.writeItem(file, displayDay);
  writer.writeItem(file, autoSyncDay);
  writer.writeItem(file, timeZonePreset);
  writer.writeItem(file, dateFormat);
  writer.writeItem(file, dailyGoalTarget);
  writer.writeItem(file, showStatsAfterReading);
  writer.writeItem(file, achievementsEnabled);
  writer.writeItem(file, achievementPopups);
  writer.writeItemString(file, sleepDirectory);
  writer.writeItem(file, sleepImageOrder);
  // Shortcut location/order/visibility settings (added for cpr-vcodex merge)
  writer.writeItem(file, browseFilesShortcut);
  writer.writeItem(file, browseFilesShortcutOrder);
  writer.writeItem(file, browseFilesShortcutVisible);
  writer.writeItem(file, statsShortcut);
  writer.writeItem(file, statsShortcutOrder);
  writer.writeItem(file, statsShortcutVisible);
  writer.writeItem(file, syncDayShortcut);
  writer.writeItem(file, syncDayShortcutOrder);
  writer.writeItem(file, syncDayShortcutVisible);
  writer.writeItem(file, settingsShortcut);
  writer.writeItem(file, settingsShortcutOrder);
  writer.writeItem(file, settingsShortcutVisible);
  writer.writeItem(file, readingStatsShortcut);
  writer.writeItem(file, readingStatsShortcutOrder);
  writer.writeItem(file, readingStatsShortcutVisible);
  writer.writeItem(file, readingHeatmapShortcut);
  writer.writeItem(file, readingHeatmapShortcutOrder);
  writer.writeItem(file, readingHeatmapShortcutVisible);
  writer.writeItem(file, achievementsShortcut);
  writer.writeItem(file, achievementsShortcutOrder);
  writer.writeItem(file, achievementsShortcutVisible);
  writer.writeItem(file, ifFoundShortcut);
  writer.writeItem(file, ifFoundShortcutOrder);
  writer.writeItem(file, ifFoundShortcutVisible);
  writer.writeItem(file, readMeShortcut);
  writer.writeItem(file, readMeShortcutOrder);
  writer.writeItem(file, readMeShortcutVisible);
  writer.writeItem(file, recentBooksShortcut);
  writer.writeItem(file, recentBooksShortcutOrder);
  writer.writeItem(file, recentBooksShortcutVisible);
  writer.writeItem(file, bookmarksShortcut);
  writer.writeItem(file, bookmarksShortcutOrder);
  writer.writeItem(file, bookmarksShortcutVisible);
  writer.writeItem(file, fileTransferShortcut);
  writer.writeItem(file, fileTransferShortcutOrder);
  writer.writeItem(file, fileTransferShortcutVisible);
  writer.writeItem(file, sleepShortcut);
  writer.writeItem(file, sleepShortcutOrder);
  writer.writeItem(file, sleepShortcutVisible);

  return writer.item_count;
}

bool CrossPointSettings::saveToFile() const {
  Storage.mkdir("/.crosspoint");

  FsFile outputFile;
  if (!Storage.openFileForWrite("CPS", SETTINGS_FILE, outputFile)) {
    return false;
  }

  // First pass: count the items
  uint8_t item_count = writeSettings(outputFile, true);  // This will just count, not write

  // Write header
  serialization::writePod(outputFile, SETTINGS_FILE_VERSION);
  serialization::writePod(outputFile, static_cast<uint8_t>(item_count));
  // Second pass: actually write the settings
  writeSettings(outputFile);  // This will write the actual data

  outputFile.close();

  LOG_DBG("CPS", "Settings saved to file");
  return true;
}

bool CrossPointSettings::loadFromFile() {
  FsFile inputFile;
  if (!Storage.openFileForRead("CPS", SETTINGS_FILE, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != SETTINGS_FILE_VERSION) {
    LOG_ERR("CPS", "Deserialization failed: Unknown version %u", version);
    inputFile.close();
    return false;
  }

  uint8_t fileSettingsCount = 0;
  serialization::readPod(inputFile, fileSettingsCount);

  uint8_t settingsRead = 0;
  bool frontButtonMappingRead = false;
  do {
    readAndValidate(inputFile, sleepScreen, SLEEP_SCREEN_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, extraParagraphSpacing);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, shortPwrBtn, SHORT_PWRBTN_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, statusBar, STATUS_BAR_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, orientation, ORIENTATION_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonLayout, FRONT_BUTTON_LAYOUT_COUNT);  // legacy
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sideButtonLayout, SIDE_BUTTON_LAYOUT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, fontFamily, FONT_FAMILY_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, fontSize, FONT_SIZE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, lineSpacing, LINE_COMPRESSION_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, paragraphAlignment, PARAGRAPH_ALIGNMENT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepTimeout, SLEEP_TIMEOUT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, refreshFrequency, REFRESH_FREQUENCY_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, screenMargin);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepScreenCoverMode, SLEEP_SCREEN_COVER_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string urlStr;
      serialization::readString(inputFile, urlStr);
      strncpy(opdsServerUrl, urlStr.c_str(), sizeof(opdsServerUrl) - 1);
      opdsServerUrl[sizeof(opdsServerUrl) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, textAntiAliasing);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, hideBatteryPercentage, HIDE_BATTERY_PERCENTAGE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, longPressChapterSkip);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, hyphenationEnabled);
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string usernameStr;
      serialization::readString(inputFile, usernameStr);
      strncpy(opdsUsername, usernameStr.c_str(), sizeof(opdsUsername) - 1);
      opdsUsername[sizeof(opdsUsername) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string passwordStr;
      serialization::readString(inputFile, passwordStr);
      strncpy(opdsPassword, passwordStr.c_str(), sizeof(opdsPassword) - 1);
      opdsPassword[sizeof(opdsPassword) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepScreenCoverFilter, SLEEP_SCREEN_COVER_FILTER_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, uiTheme);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonBack, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonConfirm, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonLeft, FRONT_BUTTON_HARDWARE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, frontButtonRight, FRONT_BUTTON_HARDWARE_COUNT);
    frontButtonMappingRead = true;
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, fadingFix);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, embeddedStyle);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, buttonModMode, BUTTON_MOD_MODE_COUNT);
    if (++settingsRead >= fileSettingsCount) break;

    serialization::readPod(inputFile, forceBoldText);
    if (++settingsRead >= fileSettingsCount) break;

    serialization::readPod(inputFile, swapPortraitControls);
    if (++settingsRead >= fileSettingsCount) break;

    serialization::readPod(inputFile, swapLandscapeControls);
    if (++settingsRead >= fileSettingsCount) break;
    // Stats and time settings (cpr-vcodex merge)
    serialization::readPod(inputFile, displayDay);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, autoSyncDay);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, timeZonePreset, 255);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, dateFormat, DATE_FORMAT_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, dailyGoalTarget, DAILY_GOAL_TARGET_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, showStatsAfterReading);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, achievementsEnabled);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, achievementPopups);
    if (++settingsRead >= fileSettingsCount) break;
    {
      std::string sleepDirStr;
      serialization::readString(inputFile, sleepDirStr);
      strncpy(sleepDirectory, sleepDirStr.c_str(), sizeof(sleepDirectory) - 1);
      sleepDirectory[sizeof(sleepDirectory) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepImageOrder, SLEEP_IMAGE_ORDER_COUNT);
    if (++settingsRead >= fileSettingsCount) break;
    // Shortcut location/order/visibility settings (cpr-vcodex merge)
    readAndValidate(inputFile, browseFilesShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, browseFilesShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, browseFilesShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, statsShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, statsShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, statsShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, syncDayShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, syncDayShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, syncDayShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, settingsShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, settingsShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, settingsShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, readingStatsShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readingStatsShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readingStatsShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, readingHeatmapShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readingHeatmapShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readingHeatmapShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, achievementsShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, achievementsShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, achievementsShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, ifFoundShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, ifFoundShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, ifFoundShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, readMeShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readMeShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, readMeShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, recentBooksShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, recentBooksShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, recentBooksShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, bookmarksShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, bookmarksShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, bookmarksShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, fileTransferShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, fileTransferShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, fileTransferShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;
    readAndValidate(inputFile, sleepShortcut, 2);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, sleepShortcutOrder);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, sleepShortcutVisible);
    if (++settingsRead >= fileSettingsCount) break;

  } while (false);

  if (frontButtonMappingRead) {
    validateFrontButtonMapping(*this);
  } else {
    applyLegacyFrontButtonLayout(*this);
  }

  inputFile.close();
  LOG_DBG("CPS", "Settings loaded from file");
  return true;
}

float CrossPointSettings::getReaderLineCompression() const {
  switch (fontFamily) {
    case BOOKERLY:
    default:
      switch (lineSpacing) {
        case TIGHT:
          return 0.95f;
        case NORMAL:
        default:
          return 1.0f;
        case WIDE:
          return 1.1f;
      }
    case NOTOSANS:
      switch (lineSpacing) {
        case TIGHT:
          return 0.90f;
        case NORMAL:
        default:
          return 0.95f;
        case WIDE:
          return 1.0f;
      }
    case OPENDYSLEXIC:
      switch (lineSpacing) {
        case TIGHT:
          return 0.90f;
        case NORMAL:
        default:
          return 0.95f;
        case WIDE:
          return 1.0f;
      }
  }
}

unsigned long CrossPointSettings::getSleepTimeoutMs() const {
  switch (sleepTimeout) {
    case SLEEP_1_MIN:
      return 1UL * 60 * 1000;
    case SLEEP_5_MIN:
      return 5UL * 60 * 1000;
    case SLEEP_10_MIN:
    default:
      return 10UL * 60 * 1000;
    case SLEEP_15_MIN:
      return 15UL * 60 * 1000;
    case SLEEP_30_MIN:
      return 30UL * 60 * 1000;
  }
}

int CrossPointSettings::getRefreshFrequency() const {
  switch (refreshFrequency) {
    case REFRESH_1:
      return 1;
    case REFRESH_5:
      return 5;
    case REFRESH_10:
      return 10;
    case REFRESH_15:
    default:
      return 15;
    case REFRESH_30:
      return 30;
  }
}

int CrossPointSettings::getReaderFontId() const {
  switch (fontFamily) {
    case BOOKERLY:
    default:
      switch (fontSize) {
        case SMALL:
          return BOOKERLY_12_FONT_ID;
        case MEDIUM:
        default:
          return BOOKERLY_14_FONT_ID;
        case LARGE:
          return BOOKERLY_16_FONT_ID;
        case EXTRA_LARGE:
          return BOOKERLY_18_FONT_ID;
      }
    case NOTOSANS:
      switch (fontSize) {
        case SMALL:
          return NOTOSANS_12_FONT_ID;
        case MEDIUM:
        default:
          return NOTOSANS_14_FONT_ID;
        case LARGE:
          return NOTOSANS_16_FONT_ID;
        case EXTRA_LARGE:
          return NOTOSANS_18_FONT_ID;
      }
    case OPENDYSLEXIC:
      switch (fontSize) {
        case SMALL:
          return OPENDYSLEXIC_8_FONT_ID;
        case MEDIUM:
        default:
          return OPENDYSLEXIC_10_FONT_ID;
        case LARGE:
          return OPENDYSLEXIC_12_FONT_ID;
        case EXTRA_LARGE:
          return OPENDYSLEXIC_14_FONT_ID;
      }
  }
}

uint64_t CrossPointSettings::getDailyGoalMs() const {
  switch (static_cast<DAILY_GOAL_TARGET>(dailyGoalTarget)) {
    case GOAL_15_MIN:
      return 15ULL * 60ULL * 1000ULL;
    case GOAL_30_MIN:
    default:
      return 30ULL * 60ULL * 1000ULL;
    case GOAL_45_MIN:
      return 45ULL * 60ULL * 1000ULL;
    case GOAL_60_MIN:
      return 60ULL * 60ULL * 1000ULL;
  }
}
