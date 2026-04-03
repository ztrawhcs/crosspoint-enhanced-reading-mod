// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "ShortcutVisibilityActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

void ShortcutVisibilityActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = (SETTINGS.*visiblePtr) ? 0 : 1;
  requestUpdate();
}

void ShortcutVisibilityActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    SETTINGS.*visiblePtr = (selectedIndex == 0) ? 1 : 0;
    SETTINGS.saveToFile();
    finish();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kOptionCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kOptionCount);
    requestUpdate();
  });
}

void ShortcutVisibilityActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SHORTCUT_VISIBLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const char* options[kOptionCount] = {tr(STR_STATE_ON), tr(STR_STATE_OFF)};

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      kOptionCount, selectedIndex,
      [&options](int index) { return std::string(options[index]); },
      nullptr, nullptr, nullptr, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
