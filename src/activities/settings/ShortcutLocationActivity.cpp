// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "ShortcutLocationActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

void ShortcutLocationActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = static_cast<int>(SETTINGS.*locationPtr);
  requestUpdate();
}

void ShortcutLocationActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    SETTINGS.*locationPtr = static_cast<uint8_t>(selectedIndex);
    SETTINGS.saveToFile();
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kLocationCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kLocationCount);
    requestUpdate();
  });
}

void ShortcutLocationActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SHORTCUT_LOCATION));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const char* locationNames[kLocationCount] = {tr(STR_SHORTCUT_HOME), tr(STR_SHORTCUT_APPS)};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      kLocationCount, selectedIndex,
      [&locationNames](int index) { return std::string(locationNames[index]); },
      nullptr, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
