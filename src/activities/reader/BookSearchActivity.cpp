#include "BookSearchActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <cctype>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// BookSearchActivity
// ---------------------------------------------------------------------------

void BookSearchActivity::onEnter() { ActivityWithSubactivity::onEnter(); }

void BookSearchActivity::onExit() { ActivityWithSubactivity::onExit(); }

std::string BookSearchActivity::getChapterTitle(const int spineIndex) const {
  const int tocIndex = epub->getTocIndexForSpineIndex(spineIndex);
  if (tocIndex != -1) {
    const std::string& title = epub->getTocItem(tocIndex).title;
    if (!title.empty()) return title;
  }
  return "Chapter " + std::to_string(spineIndex + 1);
}

void BookSearchActivity::searchSpineItem(const int spineIndex) {
  // Search the pre-built section cache .bin file rather than the EPUB ZIP.
  // Cache files are sequential binary (no ZIP decompression) so they are
  // orders of magnitude faster to scan on SPI SD card.
  // Word strings are stored as raw bytes (length-prefixed), so the character
  // sequence is present verbatim and a byte-level search finds them.
  const std::string sectionPath = epub->getCachePath() + "/sections/" + std::to_string(spineIndex) + ".bin";

  FsFile file;
  if (!Storage.openFileForRead("BSA", sectionPath, file)) {
    skippedSpines++;
    return;
  }

  const int qLen = static_cast<int>(queryLower.size());
  const int overlap = qLen - 1;  // bytes carried between chunks for boundary matches

  // Stack buffer: chunk + overlap headroom
  uint8_t buf[SCAN_CHUNK + MAX_QUERY_LEN] = {};
  int bufStart = 0;
  bool found = false;
  std::string snippet;

  while (!found) {
    const int n = file.read(buf + bufStart, SCAN_CHUNK);
    if (n <= 0) break;
    const int total = bufStart + n;

    // Scan buf[0..total-qLen] for query (case-insensitive)
    for (int i = 0; i <= total - qLen && !found; i++) {
      bool ok = true;
      for (int j = 0; j < qLen; j++) {
        if (static_cast<char>(tolower(static_cast<unsigned char>(buf[i + j]))) != queryLower[j]) {
          ok = false;
          break;
        }
      }
      if (ok) {
        found = true;
        // Extract printable context around the match, collapsing non-printable
        // bytes (binary metadata) to single spaces so the snippet is readable.
        const int ctxStart = std::max(0, i - 25);
        const int ctxEnd = std::min(total, i + qLen + 25);
        bool lastWasSpace = true;
        for (int k = ctxStart; k < ctxEnd; k++) {
          const char c = static_cast<char>(buf[k]);
          if (c >= 32 && c < 127) {
            snippet += c;
            lastWasSpace = false;
          } else if (!lastWasSpace) {
            snippet += ' ';
            lastWasSpace = true;
          }
        }
        // Trim trailing space
        if (!snippet.empty() && snippet.back() == ' ') snippet.pop_back();
      }
    }

    if (n < SCAN_CHUNK) break;  // EOF

    // Carry overlap bytes so cross-chunk matches aren't missed
    bufStart = std::min(total, overlap);
    memmove(buf, buf + total - bufStart, static_cast<size_t>(bufStart));
  }

  file.close();

  if (found && static_cast<int>(results.size()) < MAX_RESULTS) {
    results.push_back({spineIndex, getChapterTitle(spineIndex), snippet});
  }
}

void BookSearchActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (state == State::ENTER_QUERY) {
    enterNewActivity(new KeyboardEntryActivity(
        renderer, mappedInput, tr(STR_SEARCH_ENTER_QUERY), "", MAX_QUERY_LEN, false,
        [this](const std::string& text) {
          exitActivity();
          if (text.empty()) {
            if (onCancel) onCancel();
            return;
          }
          queryLower = text;
          for (char& c : queryLower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
          state = State::SEARCHING;
          currentSearchSpine = 0;
          skippedSpines = 0;
          results.clear();
          selectedResult = 0;
          requestUpdate();
        },
        [this]() {
          exitActivity();
          if (onCancel) onCancel();
        }));
    return;
  }

  if (state == State::SEARCHING) {
    // Back cancels the search at any point
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (onCancel) onCancel();
      return;
    }

    if (currentSearchSpine < epub->getSpineItemsCount()) {
      searchSpineItem(currentSearchSpine++);
      requestUpdate();
    } else {
      state = results.empty() ? State::NO_RESULTS : State::RESULTS;
      requestUpdate();
    }
    return;
  }

  if (state == State::RESULTS) {
    buttonNavigator.onNext([this] {
      selectedResult = ButtonNavigator::nextIndex(selectedResult, static_cast<int>(results.size()));
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      selectedResult = ButtonNavigator::previousIndex(selectedResult, static_cast<int>(results.size()));
      requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (onResult) onResult(results[static_cast<size_t>(selectedResult)].spineIndex);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (onCancel) onCancel();
      return;
    }
    return;
  }

  if (state == State::NO_RESULTS) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (onCancel) onCancel();
    }
  }
}

void BookSearchActivity::render(Activity::RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  if (state == State::SEARCHING) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SEARCH_BOOK));

    const std::string progress = std::string(tr(STR_SEARCHING)) + " (" + std::to_string(currentSearchSpine) + "/" +
                                 std::to_string(epub->getSpineItemsCount()) + ")";
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 20, progress.c_str());

    if (!results.empty()) {
      const std::string found =
          std::to_string(results.size()) + " match" + (results.size() == 1 ? "" : "es") + " so far";
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, found.c_str());
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
    return;
  }

  if (state == State::NO_RESULTS) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SEARCH_BOOK));

    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 10, tr(STR_SEARCH_NO_RESULTS));

    if (skippedSpines > 0) {
      const std::string note = "(" + std::to_string(skippedSpines) + " unread chapters skipped)";
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 15, note.c_str());
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // RESULTS state
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SEARCH_BOOK));

  constexpr int ITEM_HEIGHT = 46;
  const int startY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int titleLineH = renderer.getLineHeight(UI_12_FONT_ID);

  for (int i = 0; i < static_cast<int>(results.size()); i++) {
    const int itemY = startY + i * ITEM_HEIGHT;
    if (itemY + ITEM_HEIGHT > pageHeight - metrics.buttonHintsHeight) break;

    const bool isSelected = (i == selectedResult);
    if (isSelected) {
      renderer.fillRect(0, itemY, pageWidth - 1, ITEM_HEIGHT, true);
    }

    const std::string truncTitle =
        renderer.truncatedText(UI_12_FONT_ID, results[static_cast<size_t>(i)].chapterTitle.c_str(),
                               pageWidth - 2 * metrics.contentSidePadding, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, itemY + 2, truncTitle.c_str(), !isSelected,
                      EpdFontFamily::BOLD);

    const std::string truncSnippet = renderer.truncatedText(
        UI_10_FONT_ID, results[static_cast<size_t>(i)].snippet.c_str(), pageWidth - 2 * metrics.contentSidePadding);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, itemY + titleLineH + 4, truncSnippet.c_str(),
                      !isSelected);
  }

  if (skippedSpines > 0) {
    // Footer note if some chapters were uncached
    const int footerY = pageHeight - metrics.buttonHintsHeight - renderer.getLineHeight(UI_10_FONT_ID) - 2;
    const std::string note =
        std::to_string(skippedSpines) + " unread chapter" + (skippedSpines == 1 ? "" : "s") + " not searched";
    renderer.drawCenteredText(UI_10_FONT_ID, footerY, note.c_str());
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}
