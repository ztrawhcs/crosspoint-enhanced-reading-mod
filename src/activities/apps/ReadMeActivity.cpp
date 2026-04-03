// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#include "ReadMeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

void ReadMeActivity::buildTopics() {
  topics.clear();

  topics.push_back({StrId::STR_README_TOPIC_NAVIGATION,
                    {StrId::STR_README_NAV_1, StrId::STR_README_NAV_2, StrId::STR_README_NAV_3,
                     StrId::STR_README_NAV_4}});

  topics.push_back({StrId::STR_README_TOPIC_HIGHLIGHTS,
                    {StrId::STR_README_HIGHLIGHT_1, StrId::STR_README_HIGHLIGHT_2,
                     StrId::STR_README_HIGHLIGHT_3, StrId::STR_README_HIGHLIGHT_4}});

  topics.push_back({StrId::STR_README_TOPIC_SETTINGS,
                    {StrId::STR_README_SETTINGS_1, StrId::STR_README_SETTINGS_2}});

  topics.push_back({StrId::STR_README_TOPIC_SYNC,
                    {StrId::STR_README_SYNC_1, StrId::STR_README_SYNC_2}});
}

void ReadMeActivity::onEnter() {
  Activity::onEnter();
  buildTopics();
  selectedTopic = 0;
  requestUpdate();
}

void ReadMeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (onComplete) {
      onComplete();
    } else {
      finish();
    }
    return;
  }

  const int topicCount = static_cast<int>(topics.size());

  buttonNavigator.onNextRelease([this, topicCount] {
    selectedTopic = ButtonNavigator::nextIndex(selectedTopic, topicCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, topicCount] {
    selectedTopic = ButtonNavigator::previousIndex(selectedTopic, topicCount);
    requestUpdate();
  });
}

void ReadMeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_README));

  // Draw topic tabs
  const int tabY = metrics.topPadding + metrics.headerHeight;
  std::vector<TabInfo> tabs;
  for (int i = 0; i < static_cast<int>(topics.size()); ++i) {
    tabs.push_back({I18N.get(topics[i].titleId), i == selectedTopic});
  }
  GUI.drawTabBar(renderer, Rect{0, tabY, pageWidth, metrics.tabBarHeight}, tabs, false);

  // Draw topic content
  const int contentTop = tabY + metrics.tabBarHeight + metrics.verticalSpacing;
  const int sidePadding = metrics.contentSidePadding;
  const int textWidth = pageWidth - sidePadding * 2;
  const int smallLineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int titleLineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  int currentY = contentTop;

  if (selectedTopic >= 0 && selectedTopic < static_cast<int>(topics.size())) {
    const auto& topic = topics[selectedTopic];
    for (const auto& lineId : topic.lineIds) {
      const char* text = I18N.get(lineId);
      // Simple word-wrap
      std::string line;
      std::string word;
      std::string src = std::string(text) + ' ';
      for (char c : src) {
        if (c == ' ') {
          std::string candidate = line.empty() ? word : (line + ' ' + word);
          if (renderer.getTextWidth(SMALL_FONT_ID, candidate.c_str(), EpdFontFamily::REGULAR) <= textWidth) {
            line = candidate;
          } else {
            if (!line.empty()) {
              renderer.drawText(SMALL_FONT_ID, sidePadding, currentY, line.c_str());
              currentY += smallLineHeight + 2;
            }
            line = word;
          }
          word = "";
        } else {
          word += c;
        }
      }
      if (!line.empty()) {
        renderer.drawText(SMALL_FONT_ID, sidePadding, currentY, line.c_str());
        currentY += smallLineHeight + 2;
      }
      currentY += 8;  // Extra spacing between bullet points
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
