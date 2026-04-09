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
// window over the plain text.  Keeps a context buffer for full-sentence snippets.
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
        // Use up to 200 chars of context for a full sentence
        int start = ctxLen > 200 ? ctxLen - 200 : 0;
        snippet.assign(ctx + start, static_cast<size_t>(ctxLen - start));
        // Trim leading whitespace
        size_t ns = snippet.find_first_not_of(" \n\r\t");
        if (ns != std::string::npos) snippet = snippet.substr(ns);
        // Trim to sentence start: find last '. '/'! '/'? ' before end
        trimToSentenceStart(snippet);
      }
    }
    return 1;
  }

 private:
  static void trimToSentenceStart(std::string& s) {
    // Walk backwards looking for a sentence terminator followed by space/capital
    // to find the natural start of the current sentence
    const int len = static_cast<int>(s.size());
    for (int i = len - 3; i > 0; i--) {
      char t = s[static_cast<size_t>(i)];
      if ((t == '.' || t == '!' || t == '?') && s[static_cast<size_t>(i + 1)] == ' ') {
        // Start snippet after the terminator+space
        s = s.substr(static_cast<size_t>(i + 2));
        return;
      }
    }
  }

  static constexpr int CTX_SIZE = 384;
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

// ---------------------------------------------------------------------------
// Extract clean word text from a single page in an already-open .bin file.
//
// Binary layout understood here:
//   Page:      uint16_t elementCount, then for each element:
//                uint8_t tag (1=PageLine, 2=PageImage) + element data
//   PageLine:  int16_t xPos + int16_t yPos + TextBlock
//   TextBlock: uint16_t wordCount
//              + for each word: uint32_t len + len bytes   ← the actual text
//              + wordCount × uint16_t (xpos)
//              + wordCount × uint8_t  (EpdFontFamily::Style)
//              + 21 bytes BlockStyle
//                  (uint8_t alignment, bool textAlignDefined,
//                   9 × int16_t margins/padding/textIndent, bool textIndentDefined)
//   PageImage: int16_t xPos + int16_t yPos + ImageBlock
//   ImageBlock: uint32_t len + len bytes (path) + int16_t w + int16_t h
// ---------------------------------------------------------------------------
static std::string extractPageText(FsFile& file, uint32_t pageOffset) {
  file.seek(pageOffset);

  uint16_t elementCount = 0;
  if (file.read(&elementCount, 2) != 2) return {};

  std::string text;
  char wordBuf[512];

  for (uint16_t e = 0; e < elementCount && e < 500; e++) {
    uint8_t tag = 0;
    if (file.read(&tag, 1) != 1) break;

    if (tag == 1) {  // TAG_PageLine
      int16_t skip16;
      file.read(&skip16, 2);  // xPos
      file.read(&skip16, 2);  // yPos

      uint16_t wordCount = 0;
      if (file.read(&wordCount, 2) != 2 || wordCount > 2000) break;

      // Read word strings (the only actual text in this block)
      for (uint16_t w = 0; w < wordCount; w++) {
        uint32_t len = 0;
        if (file.read(&len, 4) != 4) break;
        if (len > 0 && len <= 511) {
          file.read(wordBuf, len);
          if (!text.empty()) text += ' ';
          text.append(wordBuf, static_cast<size_t>(len));
        } else if (len > 511 && len <= 4096) {
          file.seek(file.position() + len);  // skip oversized word
        } else if (len > 4096) {
          break;  // corrupt — bail
        }
      }

      // Skip xpos array  (wordCount × uint16_t = 2 bytes each)
      file.seek(file.position() + static_cast<uint32_t>(wordCount) * 2);
      // Skip styles array (wordCount × uint8_t  = 1 byte each)
      file.seek(file.position() + static_cast<uint32_t>(wordCount) * 1);
      // Skip BlockStyle  (21 bytes total)
      file.seek(file.position() + 21);

    } else if (tag == 2) {  // TAG_PageImage
      int16_t skip16;
      file.read(&skip16, 2);  // xPos
      file.read(&skip16, 2);  // yPos
      uint32_t pathLen = 0;
      if (file.read(&pathLen, 4) != 4 || pathLen > 4096) break;
      file.seek(file.position() + pathLen + 4);  // skip path + int16_t w + int16_t h
    } else {
      break;  // unknown tag — corrupt
    }
  }

  return text;
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
  uint32_t base = 0;  // absolute file offset of buf[0]
  bool found = false;
  uint32_t matchFileOffset = 0;

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
        matchFileOffset = base + static_cast<uint32_t>(i);
      }
    }

    if (n < SCAN_CHUNK) break;
    const int newOverlap = std::min(total, overlap);
    base += static_cast<uint32_t>(total - newOverlap);
    memmove(buf, buf + total - newOverlap, static_cast<size_t>(newOverlap));
    bufStart = newOverlap;
  }

  // Determine which page the match is on by reading the LUT.
  // Section .bin header layout (HEADER_SIZE = 24 bytes):
  //   [0]  uint8_t  version
  //   [1]  int      fontId (4)
  //   [5]  float    lineCompression (4)
  //   [9]  bool     extraParagraphSpacing (1)
  //   [10] uint8_t  paragraphAlignment (1)
  //   [11] uint16_t viewportWidth (2)
  //   [13] uint16_t viewportHeight (2)
  //   [15] bool     hyphenationEnabled (1)
  //   [16] bool     embeddedStyle (1)
  //   [17] bool     forceBoldText (1)
  //   [18] uint16_t pageCount (2)
  //   [20] uint32_t lutOffset (4)
  int matchPage = 0;
  uint32_t pageOffset = 0;
  std::string snippet;

  if (found) {
    file.seek(18);
    uint16_t pc = 0;
    file.read(&pc, sizeof(pc));
    uint32_t lutOffset = 0;
    file.read(&lutOffset, sizeof(lutOffset));
    file.seek(lutOffset);
    for (uint16_t p = 0; p < pc; p++) {
      uint32_t pos = 0;
      file.read(&pos, sizeof(pos));
      if (pos <= matchFileOffset) {
        matchPage = static_cast<int>(p);
        pageOffset = pos;
      } else {
        break;
      }
    }

    // Extract clean page text, then find the sentence containing the match.
    const std::string pageText = extractPageText(file, pageOffset);

    if (!pageText.empty()) {
      // Find query position within the clean page text
      std::string pageLower = pageText;
      for (char& c : pageLower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      const size_t qPos = pageLower.find(queryLower);

      if (qPos != std::string::npos) {
        const int tLen = static_cast<int>(pageText.size());
        const int mStart = static_cast<int>(qPos);
        const int mEnd = mStart + qLen;

        // Walk back to find sentence start
        int sentStart = 0;
        for (int i = mStart - 2; i >= 1; i--) {
          char t = pageText[static_cast<size_t>(i)];
          if ((t == '.' || t == '!' || t == '?') && pageText[static_cast<size_t>(i + 1)] == ' ') {
            sentStart = i + 2;
            break;
          }
        }

        // Walk forward to find sentence end
        int sentEnd = tLen;
        for (int i = mEnd; i < tLen; i++) {
          char t = pageText[static_cast<size_t>(i)];
          if (t == '.' || t == '!' || t == '?') {
            sentEnd = i + 1;
            break;
          }
        }

        snippet = pageText.substr(static_cast<size_t>(sentStart),
                                  static_cast<size_t>(sentEnd - sentStart));

        // If sentence too long, cap at 300 chars centered on the match
        if (static_cast<int>(snippet.size()) > 300) {
          const int matchInSent = mStart - sentStart;
          const int capStart = std::max(0, matchInSent - 100);
          snippet = snippet.substr(static_cast<size_t>(capStart),
                                   std::min(static_cast<int>(snippet.size()) - capStart, 300));
        }
      } else {
        // Query not found in clean text (e.g. match was in binary metadata) — use page start
        snippet = pageText.size() > 200 ? pageText.substr(0, 200) : pageText;
      }
    }
  }

  file.close();

  if (found && static_cast<int>(results.size()) < MAX_RESULTS)
    results.push_back({spineIndex, matchPage, getChapterTitle(spineIndex), snippet});
}

// Slow path: decompress from EPUB ZIP and search HTML (used for uncached chapters).
void BookSearchActivity::searchSpineItemHtml(const int spineIndex) {
  const auto spineItem = epub->getSpineItem(spineIndex);
  if (spineItem.href.empty()) return;
  TextSearchStream stream(queryLower);
  epub->readItemContentsToStream(spineItem.href, stream, 512);
  // HTML path can't determine exact page number — default to 0 (chapter start)
  if (stream.found && static_cast<int>(results.size()) < MAX_RESULTS)
    results.push_back({spineIndex, 0, getChapterTitle(spineIndex), stream.snippet});
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

    // Allow navigating partial results while search is in progress
    if (!results.empty()) {
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
        if (selectedResult < static_cast<int>(results.size())) {
          const auto& r = results[static_cast<size_t>(selectedResult)];
          if (onResult) onResult(r.spineIndex, r.pageNumber);
        }
        return;
      }
    }

    if (!searchUncached) {
      // Phase 1: scan all spines via binary cache
      if (currentSearchSpine < epub->getSpineItemsCount()) {
        searchSpineItemBinary(currentSearchSpine++);
        requestUpdate();
      } else {
        selectedResult = 0;
        state = results.empty() ? State::NO_RESULTS : State::RESULTS;
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
        const auto& r = results[static_cast<size_t>(selectedResult)];
        if (onResult) onResult(r.spineIndex, r.pageNumber);
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

  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int snippetH = renderer.getLineHeight(UI_10_FONT_ID);
  const int resultItemH = 4 + titleH + 2 + snippetH + 4;  // two-line result row
  const int simpleItemH = 6 + snippetH + 6;               // single-line row

  const int listEndY = pageHeight - metrics.buttonHintsHeight;

  if (state == State::SEARCHING) {
    // Compact progress block at top
    const std::string progress =
        std::string(tr(STR_SEARCHING)) + " (" +
        std::to_string(currentSearchSpine) + "/" +
        std::to_string(searchUncached ? static_cast<int>(uncachedSpines.size()) : epub->getSpineItemsCount()) + ")";

    int curY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, curY, progress.c_str());
    curY += snippetH + 4;

    if (!results.empty()) {
      const std::string found =
          std::to_string(results.size()) + " match" + (results.size() == 1 ? "" : "es") + " so far";
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, curY, found.c_str());
      curY += snippetH + 4;

      // Draw partial results below progress
      renderer.drawLine(metrics.contentSidePadding, curY, pageWidth - metrics.contentSidePadding, curY, true);
      curY += 2;

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
            UI_10_FONT_ID, results[static_cast<size_t>(i)].snippet.c_str(),
            pageWidth - 2 * metrics.contentSidePadding);
        renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, curY + 4 + titleH + 2, truncSnippet.c_str(),
                          !sel);
        curY += resultItemH;
      }
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), results.empty() ? "" : tr(STR_SELECT),
                                              results.empty() ? "" : tr(STR_DIR_LEFT),
                                              results.empty() ? "" : tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    if (!results.empty()) GUI.drawSideButtonHints(renderer, ">", "<");
    renderer.displayBuffer();
    return;
  }

  // RESULTS / NO_RESULTS: full list rendering
  int curY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

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
