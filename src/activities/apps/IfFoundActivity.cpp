// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "IfFoundActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

void IfFoundActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void IfFoundActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (onComplete) {
      onComplete();
    } else {
      finish();
    }
    return;
  }
}

void IfFoundActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_IF_FOUND));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int sidePadding = metrics.contentSidePadding;
  const int textWidth = pageWidth - sidePadding * 2;
  int currentY = contentTop + 20;

  const char* contactText = SETTINGS.ifFoundText;
  if (strlen(contactText) == 0) {
    contactText = tr(STR_IF_FOUND_DESC);
  }

  // Draw contact info, wrapped across lines
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  std::string line;
  std::string word;
  std::string text = std::string(contactText);
  text += ' ';

  for (char c : text) {
    if (c == ' ' || c == '\n') {
      std::string candidate = line.empty() ? word : (line + ' ' + word);
      if (renderer.getTextWidth(UI_10_FONT_ID, candidate.c_str(), EpdFontFamily::REGULAR) <= textWidth) {
        line = candidate;
      } else {
        if (!line.empty()) {
          renderer.drawText(UI_10_FONT_ID, sidePadding, currentY, line.c_str());
          currentY += lineHeight + 4;
        }
        line = word;
      }
      word = "";
      if (c == '\n') {
        renderer.drawText(UI_10_FONT_ID, sidePadding, currentY, line.c_str());
        currentY += lineHeight + 4;
        line = "";
      }
    } else {
      word += c;
    }
  }
  if (!line.empty()) {
    renderer.drawText(UI_10_FONT_ID, sidePadding, currentY, line.c_str());
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
