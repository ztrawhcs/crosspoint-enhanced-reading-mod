// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "StatusBarSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const StrId kStatusBarLabels[CrossPointSettings::STATUS_BAR_MODE_COUNT] = {
    StrId::STR_NONE_OPT,
    StrId::STR_NO_PROGRESS,
    StrId::STR_STATUS_BAR_FULL_PERCENT,
    StrId::STR_STATUS_BAR_FULL_BOOK,
    StrId::STR_STATUS_BAR_BOOK_ONLY,
    StrId::STR_STATUS_BAR_FULL_CHAPTER,
};
}  // namespace

void StatusBarSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = static_cast<int>(SETTINGS.statusBar);
  if (selectedIndex >= kModeCount) selectedIndex = 0;
  requestUpdate();
}

void StatusBarSettingsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    SETTINGS.statusBar = static_cast<uint8_t>(selectedIndex);
    SETTINGS.saveToFile();
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kModeCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kModeCount);
    requestUpdate();
  });
}

void StatusBarSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STATUS_BAR));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      kModeCount, selectedIndex,
      [](int index) { return std::string(I18N.get(kStatusBarLabels[index])); },
      nullptr, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
