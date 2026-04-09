#include "BookSearchActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Print.h>

#include <cctype>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// HTML-stripping searcher used for uncached chapters (phase 2 / opt-in).
// Streams raw EPUB HTML bytes, strips tags and entities, then slides a match
// window over the plain text.  Keeps a short context buffer for snippets.
// ---------------------------------------------------------------------------
class TextSearchStream : public Print {
 public:
  explicit TextSearchStream(const std::string& q) : query(q), qLen(q.size()) {}

  bool found = false;
  std::string snippet;

  size_t write(uint8_t c) override {
    if (found || qLen == 0) return 1;
    if (c == '<') {
      inTag = true;
      return 1;
    }
    if (c == '>') {
      inTag = false;
      return 1;
    }
    if (inTag) return 1;
    if (c == '&') {
      inEntity = true;
      return 1;
    }
    if (inEntity) {
      if (c == ';') inEntity = false;
      return 1;
    }

    char ch = (c < 32) ? ' ' : static_cast<char>(c);

    // Plain-text buffer for snippet extraction (shift-left when nearly full)
    if (ctxLen >= CTX_SIZE - 1) {
      int keep = CTX_SIZE / 2;
      memmove(ctx, ctx + (ctxLen - keep), static_cast<size_t>(keep));
      ctxLen = keep;
    }
    ctx[ctxLen++] = ch;

    // Rotating match window (last qLen chars, lowercased)
    matchBuf[matchPos] = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    matchPos = static_cast<int>((matchPos + 1) % qLen);
    if (matchFilled < static_cast<int>(qLen)) matchFilled++;

    if (matchFilled == static_cast<int>(qLen)) {
      bool ok = true;
      for (size_t i = 0; i < qLen; i++) {
        if (matchBuf[static_cast<int>((matchPos + i) % qLen)] != query[i]) {
          ok = false;
          break;
        }
      }
      if (ok) {
        found = true;
        int start = ctxLen > 45 ? ctxLen - 45 : 0;
        snippet.assign(ctx + start, static_cast<size_t>(ctxLen - start));
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
    const std::string& t = epub->getTocItem(tocIndex).title;
    if (!t.empty()) return t;
  }
  return "Chapter " + std::to_string(spineIndex + 1);
}

// Fast path: scan pre-built section cache .bin (no ZIP decompression).
void BookSearchActivity::searchSpineItemBinary(const int spineIndex) {
  const std::string path = epub->getCachePath() + "/sections/" + std::to_string(spineIndex) + ".bin";

  FsFile file;
  if (!Storage.openFileForRead("BSA", path, file)) {
    uncachedSpines.push_back(spineIndex);
    return;
  }

  const int qLen = static_cast<int>(queryLower.size());
  const int overlap = qLen - 1;
  uint8_t buf[SCAN_CHUNK + MAX_QUERY_LEN] = {};
  int bufStart = 0;
  bool found = false;
  std::string snippet;

  while (!found) {
    const int n = file.read(buf + bufStart, SCAN_CHUNK);
    if (n <= 0) break;
    const int total = bufStart + n;

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
        const int ctxStart = std::max(0, i - 25);
        const int ctxEnd = std::min(total, i + qLen + 25);
        bool lastSpace = true;
        for (int k = ctxStart; k < ctxEnd; k++) {
          const char c = static_cast<char>(buf[k]);
          if (c >= 32 && c < 127) {
            snippet += c;
            lastSpace = false;
          } else if (!lastSpace) {
            snippet += ' ';
            lastSpace = true;
          }
        }
        if (!snippet.empty() && snippet.back() == ' ') snippet.pop_back();
      }
    }

    if (n < SCAN_CHUNK) break;
    bufStart = std::min(total, overlap);
    memmove(buf, buf + total - bufStart, static_cast<size_t>(bufStart));
  }

  file.close();

  if (found && static_cast<int>(results.size()) < MAX_RESULTS)
    results.push_back({spineIndex, getChapterTitle(spineIndex), snippet});
}

// Slow path: decompress from EPUB ZIP and search HTML (used for uncached chapters).
void BookSearchActivity::searchSpineItemHtml(const int spineIndex) {
  const auto spineItem = epub->getSpineItem(spineIndex);
  if (spineItem.href.empty()) return;
  TextSearchStream stream(queryLower);
  epub->readItemContentsToStream(spineItem.href, stream, 512);
  if (stream.found && static_cast<int>(results.size()) < MAX_RESULTS)
    results.push_back({spineIndex, getChapterTitle(spineIndex), stream.snippet});
}

void BookSearchActivity::startUncachedSearch() {
  searchUncached = true;
  // Re-index: iterate the saved uncached spine list via currentSearchSpine
  currentSearchSpine = 0;
  state = State::SEARCHING;
  requestUpdate();
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
          searchUncached = false;
          currentSearchSpine = 0;
          uncachedSpines.clear();
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
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (onCancel) onCancel();
      return;
    }

    if (!searchUncached) {
      // Phase 1: scan all spines via binary cache
      if (currentSearchSpine < epub->getSpineItemsCount()) {
        searchSpineItemBinary(currentSearchSpine++);
        requestUpdate();
      } else {
        selectedResult = 0;
        state = (results.empty() && uncachedSpines.empty()) ? State::NO_RESULTS : State::RESULTS;
        if (results.empty() && !uncachedSpines.empty()) state = State::NO_RESULTS;
        requestUpdate();
      }
    } else {
      // Phase 2: scan only the previously uncached spines via HTML
      if (currentSearchSpine < static_cast<int>(uncachedSpines.size())) {
        searchSpineItemHtml(uncachedSpines[static_cast<size_t>(currentSearchSpine++)]);
        requestUpdate();
      } else {
        selectedResult = 0;
        state = results.empty() ? State::NO_RESULTS : State::RESULTS;
        requestUpdate();
      }
    }
    return;
  }

  if (state == State::RESULTS || state == State::NO_RESULTS) {
    const int total = totalItems();

    buttonNavigator.onNext([this, total] {
      selectedResult = ButtonNavigator::nextIndex(selectedResult, total);
      requestUpdate();
    });
    buttonNavigator.onPrevious([this, total] {
      selectedResult = ButtonNavigator::previousIndex(selectedResult, total);
      requestUpdate();
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (isUncachedItem(selectedResult)) {
        startUncachedSearch();
      } else if (selectedResult < static_cast<int>(results.size())) {
        if (onResult) onResult(results[static_cast<size_t>(selectedResult)].spineIndex);
      }
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (onCancel) onCancel();
      return;
    }
  }
}

void BookSearchActivity::render(Activity::RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SEARCH_BOOK));

  if (state == State::SEARCHING) {
    const std::string progress =
        std::string(tr(STR_SEARCHING)) + " (" +
        std::to_string(searchUncached ? currentSearchSpine : currentSearchSpine) + "/" +
        std::to_string(searchUncached ? static_cast<int>(uncachedSpines.size()) : epub->getSpineItemsCount()) + ")";
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

  // RESULTS / NO_RESULTS: shared list rendering
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int snippetH = renderer.getLineHeight(UI_10_FONT_ID);
  const int resultItemH = 4 + titleH + 2 + snippetH + 4;  // two-line result row
  const int simpleItemH = 6 + snippetH + 6;               // single-line row (for uncached item)

  const int listStartY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listEndY = pageHeight - metrics.buttonHintsHeight;

  int curY = listStartY;

  if (results.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, curY + 4, tr(STR_SEARCH_NO_RESULTS));
    curY += resultItemH;
  }

  // Draw result rows
  for (int i = 0; i < static_cast<int>(results.size()); i++) {
    if (curY + resultItemH > listEndY) break;
    const bool sel = (i == selectedResult);
    if (sel) renderer.fillRect(0, curY, pageWidth - 1, resultItemH, true);

    const std::string truncTitle =
        renderer.truncatedText(UI_12_FONT_ID, results[static_cast<size_t>(i)].chapterTitle.c_str(),
                               pageWidth - 2 * metrics.contentSidePadding, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, curY + 4, truncTitle.c_str(), !sel,
                      EpdFontFamily::BOLD);

    const std::string truncSnippet = renderer.truncatedText(
        UI_10_FONT_ID, results[static_cast<size_t>(i)].snippet.c_str(), pageWidth - 2 * metrics.contentSidePadding);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, curY + 4 + titleH + 2, truncSnippet.c_str(), !sel);

    curY += resultItemH;
  }

  // Draw "Search N unread chapters" item if applicable
  if (!searchUncached && !uncachedSpines.empty()) {
    const int uncachedIdx = static_cast<int>(results.size());
    if (curY + simpleItemH <= listEndY) {
      // Separator line
      renderer.drawLine(metrics.contentSidePadding, curY, pageWidth - metrics.contentSidePadding, curY, true);
      curY += 2;

      const bool sel = (selectedResult == uncachedIdx);
      if (sel) renderer.fillRect(0, curY, pageWidth - 1, simpleItemH, true);

      const std::string label = "Search " + std::to_string(uncachedSpines.size()) + " unread chapter" +
                                (uncachedSpines.size() == 1 ? "" : "s") + " (slow)";
      renderer.drawCenteredText(UI_10_FONT_ID, curY + 6, label.c_str(), !sel);
      curY += simpleItemH;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}
