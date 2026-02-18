#include "EpubReaderMenuActivity.h"

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

    if (selectedAction == MenuAction::SWAP_LANDSCAPE_CONTROLS) {
      SETTINGS.swapLandscapeControls = (SETTINGS.swapLandscapeControls == 0) ? 1 : 0;
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

  const std::string truncTitle =
      renderer.truncatedText(UI_12_FONT_ID, title.c_str(), contentWidth - 40, EpdFontFamily::BOLD);
  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, truncTitle.c_str(), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

  std::string progressLine;
  if (totalPages > 0) {
    progressLine = "Chapter: " + std::to_string(currentPage) + "/" + std::to_string(totalPages) + " pages  |  ";
  }
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

    if (menuItems[i].action == MenuAction::SWAP_LANDSCAPE_CONTROLS) {
      const auto value = swapControlsLabels[SETTINGS.swapLandscapeControls];
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - 20 - width, displayY, value, !isSelected);
    }
  }

  const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
