import os
import re

def write_file(filepath, content):
    with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content.strip() + '\n')
    print(f"[*] Overwrote {filepath}")

def patch_block(filepath, start_marker, end_marker, new_content):
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()
    pattern = re.escape(start_marker) + r".*?" + re.escape(end_marker)
    replacement = new_content.strip() + "\n\n" + end_marker
    new_text = re.sub(pattern, replacement, text, flags=re.DOTALL)
    if new_text != text:
        with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
            f.write(new_text)
        print(f"[*] Patched {filepath}")

print("Applying perfectly formatted code...")

# --- 1. OVERWRITE SMALL FILES ---

write_file('src/CrossPointSettings.h', r'''#pragma once
#include <cstdint>
#include <iosfwd>

class CrossPointSettings {
 private:
  CrossPointSettings() = default;
  static CrossPointSettings instance;

 public:
  CrossPointSettings(const CrossPointSettings&) = delete;
  CrossPointSettings& operator=(const CrossPointSettings&) = delete;

  enum SLEEP_SCREEN_MODE { DARK = 0, LIGHT = 1, CUSTOM = 2, COVER = 3, BLANK = 4, COVER_CUSTOM = 5, SLEEP_SCREEN_MODE_COUNT };
  enum SLEEP_SCREEN_COVER_MODE { FIT = 0, CROP = 1, SLEEP_SCREEN_COVER_MODE_COUNT };
  enum SLEEP_SCREEN_COVER_FILTER { NO_FILTER = 0, BLACK_AND_WHITE = 1, INVERTED_BLACK_AND_WHITE = 2, SLEEP_SCREEN_COVER_FILTER_COUNT };
  enum STATUS_BAR_MODE { NONE = 0, NO_PROGRESS = 1, FULL = 2, BOOK_PROGRESS_BAR = 3, ONLY_BOOK_PROGRESS_BAR = 4, CHAPTER_PROGRESS_BAR = 5, STATUS_BAR_MODE_COUNT };
  enum ORIENTATION { PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3, ORIENTATION_COUNT };
  enum FRONT_BUTTON_LAYOUT { BACK_CONFIRM_LEFT_RIGHT = 0, LEFT_RIGHT_BACK_CONFIRM = 1, LEFT_BACK_CONFIRM_RIGHT = 2, BACK_CONFIRM_RIGHT_LEFT = 3, FRONT_BUTTON_LAYOUT_COUNT };
  enum FRONT_BUTTON_HARDWARE { FRONT_HW_BACK = 0, FRONT_HW_CONFIRM = 1, FRONT_HW_LEFT = 2, FRONT_HW_RIGHT = 3, FRONT_BUTTON_HARDWARE_COUNT };
  enum SIDE_BUTTON_LAYOUT { PREV_NEXT = 0, NEXT_PREV = 1, SIDE_BUTTON_LAYOUT_COUNT };
  enum BUTTON_MOD_MODE { MOD_OFF = 0, MOD_SIMPLE = 1, MOD_FULL = 2, BUTTON_MOD_MODE_COUNT };
  enum FONT_FAMILY { BOOKERLY = 0, NOTOSANS = 1, OPENDYSLEXIC = 2, FONT_FAMILY_COUNT };
  enum FONT_SIZE { SMALL = 0, MEDIUM = 1, LARGE = 2, EXTRA_LARGE = 3, FONT_SIZE_COUNT };
  enum LINE_COMPRESSION { TIGHT = 0, NORMAL = 1, WIDE = 2, LINE_COMPRESSION_COUNT };
  enum PARAGRAPH_ALIGNMENT { JUSTIFIED = 0, LEFT_ALIGN = 1, CENTER_ALIGN = 2, RIGHT_ALIGN = 3, BOOK_STYLE = 4, PARAGRAPH_ALIGNMENT_COUNT };
  enum SLEEP_TIMEOUT { SLEEP_1_MIN = 0, SLEEP_5_MIN = 1, SLEEP_10_MIN = 2, SLEEP_15_MIN = 3, SLEEP_30_MIN = 4, SLEEP_TIMEOUT_COUNT };
  enum REFRESH_FREQUENCY { REFRESH_1 = 0, REFRESH_5 = 1, REFRESH_10 = 2, REFRESH_15 = 3, REFRESH_30 = 4, REFRESH_FREQUENCY_COUNT };
  enum SHORT_PWRBTN { IGNORE = 0, SLEEP = 1, PAGE_TURN = 2, SHORT_PWRBTN_COUNT };
  enum HIDE_BATTERY_PERCENTAGE { HIDE_NEVER = 0, HIDE_READER = 1, HIDE_ALWAYS = 2, HIDE_BATTERY_PERCENTAGE_COUNT };
  enum UI_THEME { CLASSIC = 0, LYRA = 1 };

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
  char blePageTurnerMac[18] = "";
  uint8_t hideBatteryPercentage = HIDE_NEVER;
  uint8_t longPressChapterSkip = 1;
  uint8_t uiTheme = LYRA;
  uint8_t fadingFix = 0;
  uint8_t embeddedStyle = 1;
  uint8_t buttonModMode = MOD_FULL;
  uint8_t forceBoldText = 0;
  uint8_t swapPortraitControls = 0;

  ~CrossPointSettings() = default;

  static CrossPointSettings& getInstance() { return instance; }
  uint16_t getPowerButtonDuration() const { return (shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) ? 10 : 400; }
  int getReaderFontId() const;
  bool saveToFile() const;
  bool loadFromFile();
  float getReaderLineCompression() const;
  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

#define SETTINGS CrossPointSettings::getInstance()''')

write_file('src/CrossPointSettings.cpp', r'''#include "CrossPointSettings.h"

#include <HalStorage.h>
#include <HardwareSerial.h>
#include <Serialization.h>

#include <cstring>

#include "fontIds.h"

CrossPointSettings CrossPointSettings::instance;

void readAndValidate(FsFile& file, uint8_t& member, const uint8_t maxValue) {
  uint8_t tempValue;
  serialization::readPod(file, tempValue);
  if (tempValue < maxValue) member = tempValue;
}

namespace {
constexpr uint8_t SETTINGS_FILE_VERSION = 1;
constexpr uint8_t SETTINGS_COUNT = 34;
constexpr char SETTINGS_FILE[] = "/.crosspoint/settings.bin";

void validateFrontButtonMapping(CrossPointSettings& settings) {
  const uint8_t mapping[] = {settings.frontButtonBack, settings.frontButtonConfirm, settings.frontButtonLeft, settings.frontButtonRight};
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

bool CrossPointSettings::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  FsFile outputFile;
  if (!Storage.openFileForWrite("CPS", SETTINGS_FILE, outputFile)) return false;

  serialization::writePod(outputFile, SETTINGS_FILE_VERSION);
  serialization::writePod(outputFile, SETTINGS_COUNT);
  serialization::writePod(outputFile, sleepScreen);
  serialization::writePod(outputFile, extraParagraphSpacing);
  serialization::writePod(outputFile, shortPwrBtn);
  serialization::writePod(outputFile, statusBar);
  serialization::writePod(outputFile, orientation);
  serialization::writePod(outputFile, frontButtonLayout);
  serialization::writePod(outputFile, sideButtonLayout);
  serialization::writePod(outputFile, fontFamily);
  serialization::writePod(outputFile, fontSize);
  serialization::writePod(outputFile, lineSpacing);
  serialization::writePod(outputFile, paragraphAlignment);
  serialization::writePod(outputFile, sleepTimeout);
  serialization::writePod(outputFile, refreshFrequency);
  serialization::writePod(outputFile, screenMargin);
  serialization::writePod(outputFile, sleepScreenCoverMode);
  serialization::writeString(outputFile, std::string(opdsServerUrl));
  serialization::writePod(outputFile, textAntiAliasing);
  serialization::writePod(outputFile, hideBatteryPercentage);
  serialization::writePod(outputFile, longPressChapterSkip);
  serialization::writePod(outputFile, hyphenationEnabled);
  serialization::writeString(outputFile, std::string(opdsUsername));
  serialization::writeString(outputFile, std::string(opdsPassword));
  serialization::writePod(outputFile, sleepScreenCoverFilter);
  serialization::writePod(outputFile, uiTheme);
  serialization::writePod(outputFile, frontButtonBack);
  serialization::writePod(outputFile, frontButtonConfirm);
  serialization::writePod(outputFile, frontButtonLeft);
  serialization::writePod(outputFile, frontButtonRight);
  serialization::writePod(outputFile, fadingFix);
  serialization::writePod(outputFile, embeddedStyle);
  serialization::writePod(outputFile, buttonModMode);
  serialization::writeString(outputFile, std::string(blePageTurnerMac));
  serialization::writePod(outputFile, forceBoldText);
  serialization::writePod(outputFile, swapPortraitControls);

  outputFile.close();
  Serial.printf("[%lu] [CPS] Settings saved to file\n", millis());
  return true;
}

bool CrossPointSettings::loadFromFile() {
  FsFile inputFile;
  if (!Storage.openFileForRead("CPS", SETTINGS_FILE, inputFile)) return false;

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version != SETTINGS_FILE_VERSION) {
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
    readAndValidate(inputFile, frontButtonLayout, FRONT_BUTTON_LAYOUT_COUNT);
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
    {
      std::string macStr;
      serialization::readString(inputFile, macStr);
      strncpy(blePageTurnerMac, macStr.c_str(), sizeof(blePageTurnerMac) - 1);
      blePageTurnerMac[sizeof(blePageTurnerMac) - 1] = '\0';
    }
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, forceBoldText);
    if (++settingsRead >= fileSettingsCount) break;
    serialization::readPod(inputFile, swapPortraitControls);
    if (++settingsRead >= fileSettingsCount) break;
  } while (false);

  if (frontButtonMappingRead) validateFrontButtonMapping(*this);
  else applyLegacyFrontButtonLayout(*this);

  inputFile.close();
  return true;
}

float CrossPointSettings::getReaderLineCompression() const {
  switch (fontFamily) {
    case BOOKERLY:
    default:
      switch (lineSpacing) {
        case TIGHT: return 0.95f;
        case NORMAL: default: return 1.0f;
        case WIDE: return 1.1f;
      }
    case NOTOSANS:
      switch (lineSpacing) {
        case TIGHT: return 0.90f;
        case NORMAL: default: return 0.95f;
        case WIDE: return 1.0f;
      }
    case OPENDYSLEXIC:
      switch (lineSpacing) {
        case TIGHT: return 0.90f;
        case NORMAL: default: return 0.95f;
        case WIDE: return 1.0f;
      }
  }
}

unsigned long CrossPointSettings::getSleepTimeoutMs() const {
  switch (sleepTimeout) {
    case SLEEP_1_MIN: return 1UL * 60 * 1000;
    case SLEEP_5_MIN: return 5UL * 60 * 1000;
    case SLEEP_10_MIN: default: return 10UL * 60 * 1000;
    case SLEEP_15_MIN: return 15UL * 60 * 1000;
    case SLEEP_30_MIN: return 30UL * 60 * 1000;
  }
}

int CrossPointSettings::getRefreshFrequency() const {
  switch (refreshFrequency) {
    case REFRESH_1: return 1;
    case REFRESH_5: return 5;
    case REFRESH_10: return 10;
    case REFRESH_15: default: return 15;
    case REFRESH_30: return 30;
  }
}

int CrossPointSettings::getReaderFontId() const {
  switch (fontFamily) {
    case BOOKERLY:
    default:
      switch (fontSize) {
        case SMALL: return BOOKERLY_12_FONT_ID;
        case MEDIUM: default: return BOOKERLY_14_FONT_ID;
        case LARGE: return BOOKERLY_16_FONT_ID;
        case EXTRA_LARGE: return BOOKERLY_18_FONT_ID;
      }
    case NOTOSANS:
      switch (fontSize) {
        case SMALL: return NOTOSANS_12_FONT_ID;
        case MEDIUM: default: return NOTOSANS_14_FONT_ID;
        case LARGE: return NOTOSANS_16_FONT_ID;
        case EXTRA_LARGE: return NOTOSANS_18_FONT_ID;
      }
    case OPENDYSLEXIC:
      switch (fontSize) {
        case SMALL: return OPENDYSLEXIC_8_FONT_ID;
        case MEDIUM: default: return OPENDYSLEXIC_10_FONT_ID;
        case LARGE: return OPENDYSLEXIC_12_FONT_ID;
        case EXTRA_LARGE: return OPENDYSLEXIC_14_FONT_ID;
      }
  }
}''')

write_file('src/activities/reader/EpubReaderMenuActivity.h', r'''#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public ActivityWithSubactivity {
 public:
  enum class MenuAction { SELECT_CHAPTER, ROTATE_SCREEN, BUTTON_MOD_SETTINGS, SWAP_CONTROLS, GO_TO_PERCENT, GO_HOME, SYNC, DELETE_CACHE };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const uint8_t currentOrientation, const std::function<void(uint8_t)>& onBack,
                                  const std::function<void(MenuAction)>& onAction)
      : ActivityWithSubactivity("EpubReaderMenu", renderer, mappedInput),
        title(title), pendingOrientation(currentOrientation), currentPage(currentPage), totalPages(totalPages),
        bookProgressPercent(bookProgressPercent), onBack(onBack), onAction(onAction) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  struct MenuItem { MenuAction action; std::string label; };

  const std::vector<MenuItem> menuItems = {
      {MenuAction::SELECT_CHAPTER, "Go to Chapter"},
      {MenuAction::ROTATE_SCREEN, "Reading Orientation"},
      {MenuAction::BUTTON_MOD_SETTINGS, "Button Mods"},
      {MenuAction::SWAP_CONTROLS, "Portrait Controls"},
      {MenuAction::GO_TO_PERCENT, "Go to %"},
      {MenuAction::GO_HOME, "Go Home"},
      {MenuAction::SYNC, "Sync Progress"},
      {MenuAction::DELETE_CACHE, "Delete Book Cache"}};

  int selectedIndex = 0;
  bool updateRequired = false;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  ButtonNavigator buttonNavigator;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  const std::vector<const char*> orientationLabels = {"Portrait", "Landscape CW", "Inverted", "Landscape CCW"};
  const std::vector<const char*> buttonModLabels = {"Off", "Simple", "Full"};
  const std::vector<const char*> swapControlsLabels = {"Bottom=Format", "Bottom=Nav"};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;

  const std::function<void(uint8_t)> onBack;
  const std::function<void(MenuAction)> onAction;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();
};''')

write_file('src/activities/reader/EpubReaderMenuActivity.cpp', r'''#include "EpubReaderMenuActivity.h"

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void EpubReaderMenuActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();
  updateRequired = true;
  xTaskCreate(&EpubReaderMenuActivity::taskTrampoline, "EpubMenuTask", 4096, this, 1, &displayTaskHandle);
}

void EpubReaderMenuActivity::onExit() {
  ActivityWithSubactivity::onExit();
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void EpubReaderMenuActivity::taskTrampoline(void* param) {
  auto* self = static_cast<EpubReaderMenuActivity*>(param);
  self->displayTaskLoop();
}

void EpubReaderMenuActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void EpubReaderMenuActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
    updateRequired = true;
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
    updateRequired = true;
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto selectedAction = menuItems[selectedIndex].action;
    if (selectedAction == MenuAction::ROTATE_SCREEN) {
      pendingOrientation = (pendingOrientation + 1) % orientationLabels.size();
      updateRequired = true;
      return;
    }

    if (selectedAction == MenuAction::BUTTON_MOD_SETTINGS) {
      SETTINGS.buttonModMode = (SETTINGS.buttonModMode + 1) % CrossPointSettings::BUTTON_MOD_MODE_COUNT;
      SETTINGS.saveToFile();
      updateRequired = true;
      return;
    }

    if (selectedAction == MenuAction::SWAP_CONTROLS) {
      SETTINGS.swapPortraitControls = (SETTINGS.swapPortraitControls == 0) ? 1 : 0;
      SETTINGS.saveToFile();
      updateRequired = true;
      return;
    }

    auto actionCallback = onAction;
    actionCallback(selectedAction);
    return;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBack(pendingOrientation);
    return;
  }
}

void EpubReaderMenuActivity::renderScreen() {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;

  const std::string truncTitle = renderer.truncatedText(UI_12_FONT_ID, title.c_str(), contentWidth - 40, EpdFontFamily::BOLD);
  const int titleX = contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, truncTitle.c_str(), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

  std::string progressLine;
  if (totalPages > 0) progressLine = "Chapter: " + std::to_string(currentPage) + "/" + std::to_string(totalPages) + " pages  |  ";
  progressLine += "Book: " + std::to_string(bookProgressPercent) + "%";
  renderer.drawCenteredText(UI_10_FONT_ID, 45, progressLine.c_str());

  const int startY = 75 + contentY;
  constexpr int lineHeight = 30;

  for (size_t i = 0; i < menuItems.size(); ++i) {
    const int displayY = startY + (i * lineHeight);
    const bool isSelected = (static_cast<int>(i) == selectedIndex);

    if (isSelected) {
      renderer.fillRect(contentX, displayY, contentWidth - 1, lineHeight, true);
    }

    renderer.drawText(UI_10_FONT_ID, contentX + 20, displayY, menuItems[i].label.c_str(), !isSelected);

    if (menuItems[i].action == MenuAction::ROTATE_SCREEN) {
      const auto value = orientationLabels[pendingOrientation];
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }

    if (menuItems[i].action == MenuAction::BUTTON_MOD_SETTINGS) {
      const auto value = buttonModLabels[SETTINGS.buttonModMode];
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }

    if (menuItems[i].action == MenuAction::SWAP_CONTROLS) {
      const auto value = swapControlsLabels[SETTINGS.swapPortraitControls];
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }
  }

  const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}''')

write_file('lib/EpdFont/EpdFontFamily.h', r'''#pragma once
#include <cstdint>

#include "EpdFont.h"

class EpdFontFamily {
 public:
  enum Style : uint8_t { REGULAR = 0, BOLD = 1, ITALIC = 2, BOLD_ITALIC = 3, UNDERLINE = 4 };

  static bool globalForceBold;

  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr);

  const EpdFont* getFont(Style style) const;
  const EpdFontData* getData(Style style) const;
  const EpdGlyph* getGlyph(uint32_t cp, Style style) const;
  bool hasPrintableChars(const char* string, Style style) const;
  void getTextDimensions(const char* string, int* w, int* h, Style style) const;

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;
};''')

write_file('lib/EpdFont/EpdFontFamily.cpp', r'''#include "EpdFontFamily.h"

bool EpdFontFamily::globalForceBold = false;

EpdFontFamily::EpdFontFamily(const EpdFont* regular, const EpdFont* bold, const EpdFont* italic,
                             const EpdFont* boldItalic)
    : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic) {}

const EpdFont* EpdFontFamily::getFont(const Style style) const {
  const bool hasBold = globalForceBold || (style & BOLD) != 0;
  const bool hasItalic = (style & ITALIC) != 0;

  if (hasBold && hasItalic && boldItalic) return boldItalic;
  if (hasBold && bold) return bold;
  if (hasItalic && italic) return italic;
  return regular;
}

const EpdFontData* EpdFontFamily::getData(const Style style) const { return getFont(style)->data; }

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style) const {
  return getFont(style)->getGlyph(cp);
}

bool EpdFontFamily::hasPrintableChars(const char* string, const Style style) const {
  return getFont(style)->hasPrintableChars(string);
}

void EpdFontFamily::getTextDimensions(const char* string, int* w, int* h, const Style style) const {
  getFont(style)->getTextDimensions(string, w, h);
}''')


# --- 2. PATCH LARGE FILES ---

patch_block('src/activities/reader/EpubReaderActivity.cpp', 
r'  if (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) {', 
r'  // --- HANDLE FORMAT DEC ---', 
r'''  if (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) {
    if (SETTINGS.swapPortraitControls == 1) {
      btnFormatDec = MappedInputManager::Button::PageBack;
      btnFormatInc = MappedInputManager::Button::PageForward;
      btnNavPrev = MappedInputManager::Button::Left;
      btnNavNext = MappedInputManager::Button::Right;
    } else {
      btnFormatDec = MappedInputManager::Button::Left;
      btnFormatInc = MappedInputManager::Button::Right;
      btnNavPrev = MappedInputManager::Button::PageBack;
      btnNavNext = MappedInputManager::Button::PageForward;
    }
  } else {
    btnFormatDec = MappedInputManager::Button::PageBack;
    btnFormatInc = MappedInputManager::Button::PageForward;
    btnNavPrev = MappedInputManager::Button::Left;
    btnNavNext = MappedInputManager::Button::Right;
  }''')

patch_block('src/activities/reader/EpubReaderActivity.cpp', 
r'      if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && waitingForFormatInc &&', 
r'        if (SETTINGS.buttonModMode == CrossPointSettings::MOD_SIMPLE) {', 
r'''      if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && waitingForFormatInc &&
          (millis() - lastFormatIncRelease < doubleClickMs)) {
        waitingForFormatInc = false;
        xSemaphoreTake(renderingMutex, portMAX_DELAY);

        if (section) {
          cachedSpineIndex = currentSpineIndex;
          cachedChapterTotalPageCount = section->pageCount;
          nextPageNumber = section->currentPage;
        }

        SETTINGS.forceBoldText = (SETTINGS.forceBoldText == 0) ? 1 : 0;
        const char* boldMsg = (SETTINGS.forceBoldText == 1) ? "Bold: ON" : "Bold: OFF";
        SETTINGS.saveToFile();

        if (epub) {
          uint16_t backupSpine = currentSpineIndex;
          uint16_t backupPage = section ? section->currentPage : 0;
          uint16_t backupPageCount = section ? section->pageCount : 0;

          section.reset();
          saveProgress(backupSpine, backupPage, backupPageCount);
        } else {
          section.reset();
        }

        xSemaphoreGive(renderingMutex);
        GUI.drawPopup(renderer, boldMsg);
        clearPopupTimer = millis() + 1000;
        updateRequired = true;
        return;
      } else {''')

patch_block('src/activities/reader/EpubReaderActivity.cpp', 
r'  if (showHelpOverlay) {', 
r'  // --- STANDARD REFRESH ---', 
r'''  if (showHelpOverlay) {
    const int w = renderer.getScreenWidth();
    const int h = renderer.getScreenHeight();

    int32_t overlayFontId = SMALL_FONT_ID;
    int overlayLineHeight = 18;

    int dismissY = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? 500 : 300;
    int dismissX = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? w / 2 : w / 2 + 25;

    drawHelpBox(renderer, dismissX, dismissY, "PRESS ANY KEY\nTO DISMISS", BoxAlign::CENTER, overlayFontId,
                overlayLineHeight);

    if (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) {
      if (SETTINGS.swapPortraitControls == 1) {
        drawHelpBox(renderer, 10, h - 80, "2x: Dark", BoxAlign::LEFT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h / 2 - 70, "1x: Text size –\nHold: Spacing\n2x: Alignment", BoxAlign::RIGHT,
                    overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h / 2 + 10, "1x: Text size +\nHold: Rotate\n2x: Bold", BoxAlign::RIGHT,
                    overlayFontId, overlayLineHeight);
      } else {
        drawHelpBox(renderer, 10, h - 80, "2x: Dark", BoxAlign::LEFT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 145, h - 80, "1x: Text size –\nHold: Spacing\n2x: Alignment", BoxAlign::RIGHT,
                    overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h - 80, "1x: Text size +\nHold: Rotate\n2x: Bold", BoxAlign::RIGHT, overlayFontId,
                    overlayLineHeight);
      }
    } else {
      drawHelpBox(renderer, w - 10, h - 40, "2x: Dark", BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
      drawHelpBox(renderer, w / 2 + 20, 20, "1x: Text size –\nHold: Spacing\n2x: Alignment", BoxAlign::RIGHT,
                  overlayFontId, overlayLineHeight);
      drawHelpBox(renderer, w / 2 + 30, 20, "1x: Text size +\nHold: Rotate\n2x: Bold", BoxAlign::LEFT, overlayFontId,
                  overlayLineHeight);
    }
  }''')

print("\n[SUCCESS] Patching complete! You can now run `pio run -t upload`")
