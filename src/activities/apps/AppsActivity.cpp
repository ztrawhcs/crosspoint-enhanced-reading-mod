// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "AppsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

void AppsActivity::rebuildShortcuts() {
  shortcuts = getVisibleShortcuts(CrossPointSettings::SHORTCUT_APPS);
}

void AppsActivity::onEnter() {
  Activity::onEnter();
  rebuildShortcuts();
  selectedIndex = 0;
  requestUpdate();
}

void AppsActivity::activateSelected() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(shortcuts.size())) return;
  const auto& entry = shortcuts[selectedIndex];
  switch (entry.id) {
    case ShortcutId::BrowseFiles:
      if (onGoToMyLibrary) onGoToMyLibrary();
      break;
    case ShortcutId::RecentBooks:
      if (onGoToRecentBooks) onGoToRecentBooks();
      break;
    case ShortcutId::Stats:
      if (onGoToReadingStats) onGoToReadingStats();
      break;
    case ShortcutId::SyncDay:
      if (onGoToSyncDay) onGoToSyncDay();
      break;
    case ShortcutId::ReadingHeatmap:
      if (onGoToReadingHeatmap) onGoToReadingHeatmap();
      break;
    case ShortcutId::Achievements:
      if (onGoToAchievements) onGoToAchievements();
      break;
    case ShortcutId::IfFound:
      if (onGoToIfFound) onGoToIfFound();
      break;
    case ShortcutId::ReadMe:
      if (onGoToReadMe) onGoToReadMe();
      break;
    case ShortcutId::FileTransfer:
      if (onGoToFileTransfer) onGoToFileTransfer();
      break;
    case ShortcutId::Sleep:
      if (onGoToSleep) onGoToSleep();
      break;
    case ShortcutId::Settings:
      if (onGoToSettings) onGoToSettings();
      break;
  }
}

void AppsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelected();
    return;
  }

  const int listSize = static_cast<int>(shortcuts.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize] {
    const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
    selectedIndex = ButtonNavigator::nextPageIndex(selectedIndex, listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize] {
    const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
    selectedIndex = ButtonNavigator::previousPageIndex(selectedIndex, listSize, pageItems);
    requestUpdate();
  });
}

void AppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_APPS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (shortcuts.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + contentHeight / 2 - 12, tr(STR_APPS_DESC));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight},
        static_cast<int>(shortcuts.size()), selectedIndex,
        [this](int index) { return std::string(I18N.get(shortcuts[index].labelId)); },
        nullptr, nullptr, nullptr, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
