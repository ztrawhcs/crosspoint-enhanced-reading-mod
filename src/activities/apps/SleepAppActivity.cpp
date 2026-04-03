// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "SleepAppActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr int kItemCount = static_cast<int>(SleepAppActivity::Item::_COUNT);

void SleepAppActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void SleepAppActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    if (onComplete) {
      onComplete();
    } else {
      finish();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (static_cast<Item>(selectedIndex)) {
      case Item::SleepMode:
        SETTINGS.sleepScreen =
            (SETTINGS.sleepScreen + 1) % CrossPointSettings::SLEEP_SCREEN_MODE_COUNT;
        break;
      case Item::SleepImageOrder:
        SETTINGS.sleepImageOrder =
            (SETTINGS.sleepImageOrder + 1) % CrossPointSettings::SLEEP_IMAGE_ORDER_COUNT;
        break;
      case Item::SleepDirectory:
        // No inline edit; directory is set via settings string — just cycle through empty/default
        break;
      default:
        break;
    }
    SETTINGS.saveToFile();
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
    requestUpdate();
  });
}

void SleepAppActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SLEEP_APP));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Sleep screen mode labels (DARK, LIGHT, CUSTOM, COVER, BLANK, COVER_CUSTOM)
  const StrId sleepModeLabels[CrossPointSettings::SLEEP_SCREEN_MODE_COUNT] = {
      StrId::STR_DARK, StrId::STR_LIGHT, StrId::STR_CUSTOM,
      StrId::STR_COVER, StrId::STR_NONE_OPT, StrId::STR_COVER_CUSTOM};

  // Sleep image order labels
  const StrId sleepOrderLabels[CrossPointSettings::SLEEP_IMAGE_ORDER_COUNT] = {
      StrId::STR_SLEEP_ORDER_SEQUENTIAL, StrId::STR_SLEEP_ORDER_RANDOM};

  const char* folderText =
      (strlen(SETTINGS.sleepDirectory) > 0) ? SETTINGS.sleepDirectory : tr(STR_SLEEP_FOLDER_EMPTY);

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      kItemCount, selectedIndex,
      [](int index) {
        switch (static_cast<Item>(index)) {
          case Item::SleepMode: return std::string(tr(STR_SLEEP_SCREEN));
          case Item::SleepDirectory: return std::string(tr(STR_SLEEP_FOLDER));
          case Item::SleepImageOrder: return std::string(tr(STR_SLEEP_IMAGE_ORDER));
          default: return std::string("");
        }
      },
      nullptr, nullptr,
      [&sleepModeLabels, &sleepOrderLabels, &folderText](int index) -> std::string {
        switch (static_cast<Item>(index)) {
          case Item::SleepMode:
            if (SETTINGS.sleepScreen < CrossPointSettings::SLEEP_SCREEN_MODE_COUNT) {
              return std::string(I18N.get(sleepModeLabels[SETTINGS.sleepScreen]));
            }
            return "";
          case Item::SleepDirectory:
            return std::string(folderText);
          case Item::SleepImageOrder:
            if (SETTINGS.sleepImageOrder < CrossPointSettings::SLEEP_IMAGE_ORDER_COUNT) {
              return std::string(I18N.get(sleepOrderLabels[SETTINGS.sleepImageOrder]));
            }
            return "";
          default:
            return "";
        }
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
