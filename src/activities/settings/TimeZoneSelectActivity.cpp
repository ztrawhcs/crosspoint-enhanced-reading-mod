// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "TimeZoneSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

void TimeZoneSelectActivity::onEnter() {
  Activity::onEnter();
  totalItems = static_cast<int>(TimeZoneRegistry::getPresetCount());
  selectedIndex = static_cast<int>(TimeZoneRegistry::clampPresetIndex(SETTINGS.timeZonePreset));
  requestUpdate();
}

void TimeZoneSelectActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    SETTINGS.timeZonePreset = static_cast<uint8_t>(selectedIndex);
    TimeUtils::configureTimezone();
    SETTINGS.saveToFile();
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, totalItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, totalItems);
    requestUpdate();
  });

  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
  buttonNavigator.onNextContinuous([this, pageItems] {
    selectedIndex = ButtonNavigator::nextPageIndex(selectedIndex, totalItems, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, pageItems] {
    selectedIndex = ButtonNavigator::previousPageIndex(selectedIndex, totalItems, pageItems);
    requestUpdate();
  });
}

void TimeZoneSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TIMEZONE_SELECT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      totalItems, selectedIndex,
      [](int index) { return std::string(TimeZoneRegistry::getPresetLabel(static_cast<uint8_t>(index))); },
      nullptr, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
