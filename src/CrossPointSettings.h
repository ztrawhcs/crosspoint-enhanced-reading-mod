#pragma once
#include <cstdint>
#include <iosfwd>

// Forward declarations
class FsFile;

class CrossPointSettings {
 private:
  CrossPointSettings() = default;
  static CrossPointSettings instance;

 public:
  CrossPointSettings(const CrossPointSettings&) = delete;
  CrossPointSettings& operator=(const CrossPointSettings&) = delete;

  enum SLEEP_SCREEN_MODE {
    DARK = 0,
    LIGHT = 1,
    CUSTOM = 2,
    COVER = 3,
    BLANK = 4,
    COVER_CUSTOM = 5,
    SLEEP_SCREEN_MODE_COUNT
  };
  enum SLEEP_SCREEN_COVER_MODE { FIT = 0, CROP = 1, SLEEP_SCREEN_COVER_MODE_COUNT };
  enum SLEEP_SCREEN_COVER_FILTER {
    NO_FILTER = 0,
    BLACK_AND_WHITE = 1,
    INVERTED_BLACK_AND_WHITE = 2,
    SLEEP_SCREEN_COVER_FILTER_COUNT
  };
  enum STATUS_BAR_MODE {
    NONE = 0,
    NO_PROGRESS = 1,
    FULL = 2,
    BOOK_PROGRESS_BAR = 3,
    ONLY_BOOK_PROGRESS_BAR = 4,
    CHAPTER_PROGRESS_BAR = 5,
    STATUS_BAR_MODE_COUNT
  };
  enum ORIENTATION { PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3, ORIENTATION_COUNT };
  enum FRONT_BUTTON_LAYOUT {
    BACK_CONFIRM_LEFT_RIGHT = 0,
    LEFT_RIGHT_BACK_CONFIRM = 1,
    LEFT_BACK_CONFIRM_RIGHT = 2,
    BACK_CONFIRM_RIGHT_LEFT = 3,
    FRONT_BUTTON_LAYOUT_COUNT
  };
  enum FRONT_BUTTON_HARDWARE {
    FRONT_HW_BACK = 0,
    FRONT_HW_CONFIRM = 1,
    FRONT_HW_LEFT = 2,
    FRONT_HW_RIGHT = 3,
    FRONT_BUTTON_HARDWARE_COUNT
  };
  enum SIDE_BUTTON_LAYOUT { PREV_NEXT = 0, NEXT_PREV = 1, SIDE_BUTTON_LAYOUT_COUNT };
  enum BUTTON_MOD_MODE { MOD_OFF = 0, MOD_SIMPLE = 1, MOD_FULL = 2, BUTTON_MOD_MODE_COUNT };
  enum FONT_FAMILY { BOOKERLY = 0, NOTOSANS = 1, OPENDYSLEXIC = 2, FONT_FAMILY_COUNT };
  enum FONT_SIZE { SMALL = 0, MEDIUM = 1, LARGE = 2, EXTRA_LARGE = 3, FONT_SIZE_COUNT };
  enum LINE_COMPRESSION { TIGHT = 0, NORMAL = 1, WIDE = 2, LINE_COMPRESSION_COUNT };
  enum PARAGRAPH_ALIGNMENT {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
    BOOK_STYLE = 4,
    PARAGRAPH_ALIGNMENT_COUNT
  };
  enum SLEEP_TIMEOUT {
    SLEEP_1_MIN = 0,
    SLEEP_5_MIN = 1,
    SLEEP_10_MIN = 2,
    SLEEP_15_MIN = 3,
    SLEEP_30_MIN = 4,
    SLEEP_TIMEOUT_COUNT
  };
  enum REFRESH_FREQUENCY {
    REFRESH_1 = 0,
    REFRESH_5 = 1,
    REFRESH_10 = 2,
    REFRESH_15 = 3,
    REFRESH_30 = 4,
    REFRESH_FREQUENCY_COUNT
  };
  enum SHORT_PWRBTN { IGNORE = 0, SLEEP = 1, PAGE_TURN = 2, SHORT_PWRBTN_COUNT };
  enum HIDE_BATTERY_PERCENTAGE { HIDE_NEVER = 0, HIDE_READER = 1, HIDE_ALWAYS = 2, HIDE_BATTERY_PERCENTAGE_COUNT };
  // UI Theme
  enum UI_THEME { CLASSIC = 0, LYRA = 1, LYRA_3_COVERS = 2, LYRA_CUSTOM = 3 };

  enum DAILY_GOAL_TARGET {
    GOAL_15_MIN = 0,
    GOAL_30_MIN = 1,
    GOAL_45_MIN = 2,
    GOAL_60_MIN = 3,
    DAILY_GOAL_TARGET_COUNT
  };

  enum DATE_FORMAT {
    DATE_DD_MM_YYYY = 0,
    DATE_MM_DD_YYYY = 1,
    DATE_YYYY_MM_DD = 2,
    DATE_FORMAT_COUNT
  };

  enum SLEEP_IMAGE_ORDER {
    SLEEP_ORDER_SEQUENTIAL = 0,
    SLEEP_ORDER_RANDOM = 1,
    SLEEP_IMAGE_ORDER_COUNT
  };

  enum SHORTCUT_LOCATION {
    SHORTCUT_HOME = 0,
    SHORTCUT_APPS = 1
  };

  uint8_t sleepScreen = DARK;
  uint8_t sleepScreenCoverMode = FIT;
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  uint8_t statusBar = FULL;
  uint8_t extraParagraphSpacing = 1;
  uint8_t textAntiAliasing = 1;
  uint8_t shortPwrBtn = IGNORE;
  uint8_t orientation = PORTRAIT;
  uint8_t frontButtonLayout = BACK_CONFIRM_LEFT_RIGHT;
  uint8_t sideButtonLayout = PREV_NEXT;
  uint8_t frontButtonBack = FRONT_HW_BACK;
  uint8_t frontButtonConfirm = FRONT_HW_CONFIRM;
  uint8_t frontButtonLeft = FRONT_HW_LEFT;
  uint8_t frontButtonRight = FRONT_HW_RIGHT;
  uint8_t fontFamily = BOOKERLY;
  uint8_t fontSize = MEDIUM;
  uint8_t lineSpacing = NORMAL;
  uint8_t paragraphAlignment = JUSTIFIED;
  uint8_t sleepTimeout = SLEEP_10_MIN;
  uint8_t refreshFrequency = REFRESH_15;
  uint8_t hyphenationEnabled = 0;
  uint8_t screenMargin = 5;
  char opdsServerUrl[128] = "";
  char opdsUsername[64] = "";
  char opdsPassword[64] = "";
  uint8_t hideBatteryPercentage = HIDE_NEVER;
  uint8_t longPressChapterSkip = 1;
  uint8_t uiTheme = LYRA;
  uint8_t fadingFix = 0;
  uint8_t embeddedStyle = 1;
  uint8_t buttonModMode = MOD_FULL;
  uint8_t forceBoldText = 0;
  uint8_t swapPortraitControls = 0;
  uint8_t swapLandscapeControls = 0;
  uint8_t highlightModeEnabled = 1;  // --- HIGHLIGHT MODE --- enable/disable highlight feature

  // Stats and time settings (added for cpr-vcodex feature merge)
  uint8_t displayDay = 0;
  uint8_t autoSyncDay = 0;
  uint8_t timeZonePreset = 0;
  uint8_t dateFormat = 0;
  uint8_t dailyGoalTarget = GOAL_30_MIN;
  uint8_t showStatsAfterReading = 0;
  uint8_t achievementsEnabled = 1;
  uint8_t achievementPopups = 1;

  // Sleep folder settings
  char sleepDirectory[128] = "";
  uint8_t sleepImageOrder = SLEEP_ORDER_SEQUENTIAL;

  // Shortcut location/order/visibility settings (added for cpr-vcodex merge)
  uint8_t browseFilesShortcut = SHORTCUT_HOME;
  uint8_t browseFilesShortcutOrder = 0;
  uint8_t browseFilesShortcutVisible = 1;
  uint8_t statsShortcut = SHORTCUT_HOME;
  uint8_t statsShortcutOrder = 1;
  uint8_t statsShortcutVisible = 0;
  uint8_t syncDayShortcut = SHORTCUT_HOME;
  uint8_t syncDayShortcutOrder = 2;
  uint8_t syncDayShortcutVisible = 1;
  uint8_t settingsShortcut = SHORTCUT_HOME;
  uint8_t settingsShortcutOrder = 3;
  uint8_t settingsShortcutVisible = 1;
  uint8_t readingStatsShortcut = SHORTCUT_APPS;
  uint8_t readingStatsShortcutOrder = 0;
  uint8_t readingStatsShortcutVisible = 1;
  uint8_t readingHeatmapShortcut = SHORTCUT_APPS;
  uint8_t readingHeatmapShortcutOrder = 1;
  uint8_t readingHeatmapShortcutVisible = 1;
  uint8_t achievementsShortcut = SHORTCUT_APPS;
  uint8_t achievementsShortcutOrder = 2;
  uint8_t achievementsShortcutVisible = 1;
  // If Found contact text (shown on the If Found screen)
  char ifFoundText[128] = "";

  uint8_t ifFoundShortcut = SHORTCUT_APPS;
  uint8_t ifFoundShortcutOrder = 3;
  uint8_t ifFoundShortcutVisible = 0;
  uint8_t readMeShortcut = SHORTCUT_APPS;
  uint8_t readMeShortcutOrder = 4;
  uint8_t readMeShortcutVisible = 0;
  uint8_t recentBooksShortcut = SHORTCUT_HOME;
  uint8_t recentBooksShortcutOrder = 4;
  uint8_t recentBooksShortcutVisible = 1;
  uint8_t bookmarksShortcut = SHORTCUT_APPS;
  uint8_t bookmarksShortcutOrder = 5;
  uint8_t bookmarksShortcutVisible = 0;
  uint8_t fileTransferShortcut = SHORTCUT_HOME;
  uint8_t fileTransferShortcutOrder = 5;
  uint8_t fileTransferShortcutVisible = 1;
  uint8_t sleepShortcut = SHORTCUT_APPS;
  uint8_t sleepShortcutOrder = 6;
  uint8_t sleepShortcutVisible = 0;

  ~CrossPointSettings() = default;

  static CrossPointSettings& getInstance() { return instance; }

  uint16_t getPowerButtonDuration() const {
    return (shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) ? 10 : 400;
  }
  int getReaderFontId() const;
  uint64_t getDailyGoalMs() const;

  // If count_only is true, returns the number of settings items that would be written.
  uint8_t writeSettings(FsFile& file, bool count_only = false) const;

  bool saveToFile() const;
  bool loadFromFile();
  float getReaderLineCompression() const;
  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

#define SETTINGS CrossPointSettings::getInstance()
