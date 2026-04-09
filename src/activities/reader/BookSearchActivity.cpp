#include "BookSearchActivity.h"

#include <I18n.h>
#include <Print.h>

#include <cctype>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// HTML-stripping case-insensitive substring searcher (Arduino Print subclass)
//
// Streams HTML bytes, strips tags and entities, then slides a match window
// over the resulting plain text.  Keeps a short context buffer so we can
// extract a readable snippet when a match is found.
// ---------------------------------------------------------------------------
class TextSearchStream : public Print {
 public:
  explicit TextSearchStream(const std::string& q) : query(q), qLen(q.size()) {}

  bool found = false;
  std::string snippet;

  size_t write(uint8_t c) override {
    if (found || qLen == 0) return 1;

    // Strip HTML tags
    if (c == '<') {
      inTag = true;
      return 1;
    }
    if (c == '>') {
      inTag = false;
      return 1;
    }
    if (inTag) return 1;

    // Skip HTML entities (&amp; &nbsp; etc.)
    if (c == '&') {
      inEntity = true;
      return 1;
    }
    if (inEntity) {
      if (c == ';') inEntity = false;
      return 1;
    }

    // Normalise whitespace
    char ch = (c < 32) ? ' ' : static_cast<char>(c);

    // Plain-text context buffer (used to extract snippet on match).
    // When nearly full, shift left to keep the tail.
    if (ctxLen >= CTX_SIZE - 1) {
      int keep = CTX_SIZE / 2;
      memmove(ctx, ctx + (ctxLen - keep), static_cast<size_t>(keep));
      ctxLen = keep;
    }
    ctx[ctxLen++] = ch;

    // Rotate the match window (last qLen chars, lowercased)
    matchBuf[matchPos] = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    matchPos = static_cast<int>((matchPos + 1) % qLen);
    if (matchFilled < static_cast<int>(qLen)) matchFilled++;

    // Check for match once the window is full
    if (matchFilled == static_cast<int>(qLen)) {
      bool ok = true;
      for (size_t i = 0; i < qLen; i++) {
        int idx = static_cast<int>((matchPos + i) % qLen);
        if (matchBuf[idx] != query[i]) {
          ok = false;
          break;
        }
      }
      if (ok) {
        found = true;
        // Extract up to 45 chars of context ending at the current position
        int start = ctxLen > 45 ? ctxLen - 45 : 0;
        snippet.assign(ctx + start, static_cast<size_t>(ctxLen - start));
        // Trim leading whitespace
        size_t ns = snippet.find_first_not_of(" \n\r\t");
        if (ns != std::string::npos) snippet = snippet.substr(ns);
      }
    }
    return 1;
  }

 private:
  static constexpr int CTX_SIZE = 128;
  static constexpr int MAX_QLEN = 64;

  const std::string& query;
  const size_t qLen;
  bool inTag = false;
  bool inEntity = false;

  char ctx[CTX_SIZE] = {};
  int ctxLen = 0;

  char matchBuf[MAX_QLEN] = {};
  int matchPos = 0;
  int matchFilled = 0;
};

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
  const auto spineItem = epub->getSpineItem(spineIndex);
  if (spineItem.href.empty()) return;

  TextSearchStream stream(queryLower);
  epub->readItemContentsToStream(spineItem.href, stream, 512);

  if (stream.found && static_cast<int>(results.size()) < MAX_RESULTS) {
    SearchResult result;
    result.spineIndex = spineIndex;
    result.chapterTitle = getChapterTitle(spineIndex);
    result.snippet = stream.snippet;
    results.push_back(std::move(result));
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
    // Progress screen
    const std::string header = tr(STR_SEARCH_BOOK);
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header.c_str());

    const std::string progress = std::string(tr(STR_SEARCHING)) + " (" + std::to_string(currentSearchSpine) + "/" +
                                 std::to_string(epub->getSpineItemsCount()) + ")";
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, progress.c_str());

    renderer.displayBuffer();
    return;
  }

  if (state == State::NO_RESULTS) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SEARCH_BOOK));
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, tr(STR_SEARCH_NO_RESULTS));
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

    // Chapter title (bold)
    const std::string truncTitle =
        renderer.truncatedText(UI_12_FONT_ID, results[static_cast<size_t>(i)].chapterTitle.c_str(),
                               pageWidth - 2 * metrics.contentSidePadding, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, itemY + 2, truncTitle.c_str(), !isSelected,
                      EpdFontFamily::BOLD);

    // Snippet (regular, smaller font)
    const std::string truncSnippet =
        renderer.truncatedText(UI_10_FONT_ID, results[static_cast<size_t>(i)].snippet.c_str(),
                               pageWidth - 2 * metrics.contentSidePadding);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, itemY + titleLineH + 4, truncSnippet.c_str(),
                      !isSelected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}
