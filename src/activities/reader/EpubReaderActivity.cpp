#include "EpubReaderActivity.h"

#include <Epub/Page.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <sstream>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "EpubReaderChapterSelectionActivity.h"
#include "EpubReaderPercentSelectionActivity.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HighlightStore.h"  // --- HIGHLIGHT MODE ---

namespace {
// pagesPerRefresh now comes from SETTINGS.getRefreshFrequency()
constexpr unsigned long skipChapterMs = 700;
constexpr unsigned long goHomeMs = 1000;
constexpr unsigned long formattingToggleMs = 500;
// New constant for double click speed
constexpr unsigned long doubleClickMs = 400;

// --- HIGHLIGHT MODE ---
constexpr unsigned long highlightDoubleTapMs = 350;  // Double-tap Power window
constexpr unsigned long highlightLongPressMs = 500;  // Long-press Back to cancel
// --- HIGHLIGHT MODE ---

// Global state for the Help Overlay and Night Mode
static bool showHelpOverlay = false;
static bool isNightMode = false;

constexpr int statusBarMargin = 19;
constexpr int progressBarMarginTop = 1;

int clampPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

void applyReaderOrientation(GfxRenderer& renderer, const uint8_t orientation) {
  switch (orientation) {
    case CrossPointSettings::ORIENTATION::PORTRAIT:
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case CrossPointSettings::ORIENTATION::INVERTED:
      renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      break;
  }
}

// Enum for cleaner alignment logic
enum class BoxAlign { LEFT, RIGHT, CENTER };

// Helper to draw multi-line text cleanly
void drawHelpBox(const GfxRenderer& renderer, int x, int y, const char* text, BoxAlign align, int32_t fontId,
                 int lineHeight) {
  // Split text into lines
  std::vector<std::string> lines;
  std::stringstream ss(text);
  std::string line;
  int maxWidth = 0;

  while (std::getline(ss, line, '\n')) {
    lines.push_back(line);
    int w = renderer.getTextWidth(fontId, line.c_str());
    if (w > maxWidth) maxWidth = w;
  }

  // Padding
  int padding = 16;
  int boxWidth = maxWidth + padding;
  int boxHeight = (lines.size() * lineHeight) + padding;

  int drawX = x;
  if (align == BoxAlign::RIGHT) {
    drawX = x - boxWidth;
  } else if (align == BoxAlign::CENTER) {
    drawX = x - (boxWidth / 2);
  }

  // Ensure we don't draw off the bottom edge
  if (y + boxHeight > renderer.getScreenHeight()) {
    y = renderer.getScreenHeight() - boxHeight - 5;
  }

  // Fill White (Clear background)
  renderer.fillRect(drawX, y, boxWidth, boxHeight, false);
  // Draw Border Black (Thickness: 4 for Bold)
  renderer.drawRect(drawX, y, boxWidth, boxHeight, 4, true);

  // Draw each line
  for (size_t i = 0; i < lines.size(); i++) {
    // ALWAYS center text horizontally within the box
    int lineWidth = renderer.getTextWidth(fontId, lines[i].c_str());
    int lineX_centered = drawX + (boxWidth - lineWidth) / 2;

    renderer.drawText(fontId, lineX_centered, y + (padding / 2) + (i * lineHeight), lines[i].c_str());
  }
}

// --- HIGHLIGHT MODE ---
// Returns the X pixel position (relative to page coordinate space, before adding orientedMarginLeft)
// for a given character offset within a PageLine. Snaps to word boundaries.
// For start-of-selection: returns the xpos of the word containing charOffset (bar starts here).
static int16_t charOffsetToStartPixel(const PageLine* pl, int charOffset) {
  const auto& words = pl->getBlock()->getWords();
  const auto& xpositions = pl->getBlock()->getWordXPositions();
  if (words.empty() || xpositions.empty()) return 0;

  int pos = 0;
  auto wordIt = words.begin();
  auto xposIt = xpositions.begin();
  int16_t lastXPos = *xposIt;

  while (wordIt != words.end() && xposIt != xpositions.end()) {
    lastXPos = *xposIt;
    int wordEnd = pos + static_cast<int>(wordIt->size());
    if (charOffset <= wordEnd) {
      return *xposIt;
    }
    pos = wordEnd + 1;
    ++wordIt;
    ++xposIt;
  }
  return lastXPos;
}

// For end-of-selection: returns the xpos of the NEXT word after the one containing charOffset,
// so the bar includes the full last word. Returns -1 if charOffset is in the last word (use full width).
static int charOffsetToEndPixel(const PageLine* pl, int charOffset) {
  const auto& words = pl->getBlock()->getWords();
  const auto& xpositions = pl->getBlock()->getWordXPositions();
  if (words.empty() || xpositions.empty()) return -1;

  int pos = 0;
  auto wordIt = words.begin();
  auto xposIt = xpositions.begin();

  while (wordIt != words.end() && xposIt != xpositions.end()) {
    int wordEnd = pos + static_cast<int>(wordIt->size());
    if (charOffset <= wordEnd) {
      // Found the containing word — return the next word's start xpos
      auto nextXposIt = xposIt;
      ++nextXposIt;
      if (nextXposIt != xpositions.end()) {
        return static_cast<int>(*nextXposIt);
      }
      return -1;  // this is the last word, keep full width
    }
    pos = wordEnd + 1;
    ++wordIt;
    ++xposIt;
  }
  return -1;  // past all words, keep full width
}
// --- HIGHLIGHT MODE ---

}  // namespace

void EpubReaderActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  // Reset help overlay state when entering a book
  showHelpOverlay = false;
  // Reset Night Mode on entry
  isNightMode = false;

  if (!epub) {
    return;
  }

  applyReaderOrientation(renderer, SETTINGS.orientation);

  epub->setupCacheDir();

  FsFile f;
  if (Storage.openFileForRead("ERS", epub->getCachePath() + "/bookstats.bin", f)) {
    uint8_t statsData[4];
    if (f.read(statsData, 4) == 4) {
      totalBookBytes = statsData[0] | (statsData[1] << 8) | (statsData[2] << 16) | (statsData[3] << 24);
      LOG_DBG("ERS", "Loaded bookstats: %u bytes", (unsigned)totalBookBytes);
    }
    f.close();
  } else {
    const size_t bookSize = epub->getBookSize();
    if (bookSize > 0 && Storage.openFileForWrite("ERS", epub->getCachePath() + "/bookstats.bin", f)) {
      uint8_t statsData[4];
      statsData[0] = bookSize & 0xFF;
      statsData[1] = (bookSize >> 8) & 0xFF;
      statsData[2] = (bookSize >> 16) & 0xFF;
      statsData[3] = (bookSize >> 24) & 0xFF;
      f.write(statsData, 4);
      f.close();
      totalBookBytes = bookSize;
      LOG_DBG("ERS", "Saved bookstats: %u bytes", (unsigned)totalBookBytes);
    }
  }

  if (Storage.openFileForRead("ERS", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[6];
    int dataSize = f.read(data, 6);
    if (dataSize == 4 || dataSize == 6) {
      currentSpineIndex = data[0] + (data[1] << 8);
      nextPageNumber = data[2] + (data[3] << 8);
      cachedSpineIndex = currentSpineIndex;
      LOG_DBG("ERS", "Loaded cache: %d, %d", currentSpineIndex, nextPageNumber);
    }
    if (dataSize == 6) {
      cachedChapterTotalPageCount = data[4] + (data[5] << 8);
    }
    f.close();
  }

  if (currentSpineIndex == 0) {
    int textSpineIndex = epub->getSpineIndexForTextReference();
    if (textSpineIndex != 0) {
      currentSpineIndex = textSpineIndex;
      LOG_DBG("ERS", "Opened for first time, navigating to text reference at index %d", textSpineIndex);
    }
  }

  APP_STATE.openEpubPath = epub->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(epub->getPath(), epub->getTitle(), epub->getAuthor(), epub->getThumbBmpPath());

  // Trigger first update
  requestUpdate();
}

void EpubReaderActivity::onExit() {
  ActivityWithSubactivity::onExit();

  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  // --- HIGHLIGHT MODE ---
  highlightState.reset();
  highlightCachedPage = -1;
  // --- HIGHLIGHT MODE ---

  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  section.reset();
  epub.reset();
}

void EpubReaderActivity::loop() {
  static bool pendingPowerPageTurn = false;

  // --- POPUP AUTO-DISMISS ---
  static unsigned long clearPopupTimer = 0;
  if (clearPopupTimer > 0 && millis() > clearPopupTimer) {
    clearPopupTimer = 0;
    requestUpdate();
  }

  // --- HELP OVERLAY INTERCEPTION ---
  if (showHelpOverlay) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Left) ||
        mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
        mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
        mappedInput.wasReleased(MappedInputManager::Button::Power)) {
      showHelpOverlay = false;
      requestUpdate();
      return;
    }
    return;
  }

  // --- DOUBLE CLICK STATE VARIABLES ---
  static unsigned long lastFormatDecRelease = 0;
  static bool waitingForFormatDec = false;
  static unsigned long lastFormatIncRelease = 0;
  static bool waitingForFormatInc = false;

  if (subActivity) {
    subActivity->loop();
    if (pendingSubactivityExit) {
      pendingSubactivityExit = false;
      exitActivity();
      requestUpdate();
      skipNextButtonCheck = true;  // Skip button processing to ignore stale events
    }
    if (pendingGoHome) {
      pendingGoHome = false;
      exitActivity();
      if (onGoHome) {
        onGoHome();
      }
      return;
    }
    return;
  }

  if (pendingGoHome) {
    pendingGoHome = false;
    if (onGoHome) {
      onGoHome();
    }
    return;
  }

  if (skipNextButtonCheck) {
    const bool confirmCleared = !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
                                !mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    const bool backCleared = !mappedInput.isPressed(MappedInputManager::Button::Back) &&
                             !mappedInput.wasReleased(MappedInputManager::Button::Back);
    if (confirmCleared && backCleared) {
      skipNextButtonCheck = false;
    }
    return;
  }

  // --- HIGHLIGHT MODE --- Force-exit on spine (chapter) change
  if (highlightState.mode != HighlightState::INACTIVE && previousSpineIndex >= 0 &&
      currentSpineIndex != previousSpineIndex) {
    highlightState.reset();
    highlightCachedPage = -1;
    LOG_DBG("ERS", "Highlight mode force-exited: spine index changed");
  }
  previousSpineIndex = currentSpineIndex;

  // --- HIGHLIGHT MODE --- Power double-tap detection (enter highlight / save highlight)
  // Step 9: Only process highlight mode inputs if the feature is enabled
  if (SETTINGS.highlightModeEnabled) {
    static unsigned long lastPowerRelease = 0;
    static bool waitingForPowerDoubleTap = false;

    if (mappedInput.wasReleased(MappedInputManager::Button::Power)) {
      if (waitingForPowerDoubleTap && (millis() - lastPowerRelease < highlightDoubleTapMs)) {
        // Double-tap Power detected
        waitingForPowerDoubleTap = false;

        if (highlightState.mode == HighlightState::INACTIVE) {
          // Enter CURSOR mode
          RenderLock lock(*this);
          highlightState.mode = HighlightState::CURSOR;
          highlightState.cursorLineIndex = 0;
          LOG_DBG("ERS", "Highlight mode: entered CURSOR");
          GUI.drawPopup(renderer, "Highlight Mode");
          clearPopupTimer = millis() + 800;
          requestUpdate();
          return;
        } else if (highlightState.mode == HighlightState::SELECT) {
          // Save highlight
          RenderLock lock(*this);
          // Extract highlighted text — may span two pages
          std::string extractedText;
          // BUGFIX: use selectionStartPage, not section->currentPage — currentPage may have been
          // auto-turned to selectionEndPage by single-tap Power navigation during selection.
          int startPageIdx = (highlightState.selectionStartPage >= 0) ? highlightState.selectionStartPage
                                                                      : (section ? section->currentPage : 0);
          int endPageIdx = (highlightState.selectionEndPage >= 0) ? highlightState.selectionEndPage : startPageIdx;
          if (section) {
            int savedCurrentPage = section->currentPage;
            section->currentPage = startPageIdx;
            auto startPageObj = section->loadPageFromSectionFile();
            section->currentPage = savedCurrentPage;
            if (startPageObj) {
              int textLineCount = HighlightStore::countTextLines(*startPageObj);
              int startLine = std::max(0, std::min(highlightState.selectionStartLine, textLineCount - 1));
              if (endPageIdx == startPageIdx) {
                int endLine = std::max(0, std::min(highlightState.selectionEndLine, textLineCount - 1));
                extractedText =
                    HighlightStore::extractText(*startPageObj, startLine, highlightState.selectionStartCharOffset,
                                                endLine, highlightState.selectionEndCharOffset);
              } else {
                // First part: start line to end of start page
                extractedText = HighlightStore::extractText(
                    *startPageObj, startLine, highlightState.selectionStartCharOffset, textLineCount - 1, -1);
                // Second part: start of end page to end line
                int savedPage = section->currentPage;
                section->currentPage = endPageIdx;
                auto endPageObj = section->loadPageFromSectionFile();
                section->currentPage = savedPage;
                if (endPageObj) {
                  int endLineCount = HighlightStore::countTextLines(*endPageObj);
                  int endLine = std::max(0, std::min(highlightState.selectionEndLine, endLineCount - 1));
                  std::string endText =
                      HighlightStore::extractText(*endPageObj, 0, 0, endLine, highlightState.selectionEndCharOffset);
                  if (!endText.empty()) {
                    if (!extractedText.empty()) extractedText += ' ';
                    extractedText += endText;
                  }
                }
              }
            }
          }
          if (extractedText.empty()) extractedText = "[no text extracted]";

          // Gather metadata for save
          std::string chapterName = "";
          const int tocIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
          if (tocIndex != -1) chapterName = epub->getTocItem(tocIndex).title;
          float bookProgress = 0.0f;
          if (epub && epub->getBookSize() > 0 && section && section->pageCount > 0) {
            const float chapterProgress =
                static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
            bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f;
          }

          HighlightStore::saveHighlight(epub->getTitle(), epub->getAuthor(), currentSpineIndex, chapterName,
                                        startPageIdx, endPageIdx, section ? section->pageCount : 0, bookProgress,
                                        extractedText);

          highlightState.reset();
          highlightCachedPage = -1;
          LOG_DBG("ERS", "Highlight saved, returning to NORMAL");
          GUI.drawPopup(renderer, "Highlight Saved");
          clearPopupTimer = millis() + 1000;
          requestUpdate();
          return;
        }
      } else {
        waitingForPowerDoubleTap = true;
        lastPowerRelease = millis();
        return;  // Prevent fallthrough to navigation section where wasReleased(Power) fires page turn
      }
    }

    // Power single-tap timeout: dispatch the short tap action
    if (waitingForPowerDoubleTap && (millis() - lastPowerRelease > highlightDoubleTapMs)) {
      waitingForPowerDoubleTap = false;

      if (highlightState.mode == HighlightState::CURSOR) {
        // Single Power tap in CURSOR mode: if cursor is on a saved highlight → delete it.
        // Otherwise → enter SELECT mode.
        RenderLock lock(*this);
        // Check if cursor is on a saved highlight — delete it if so
        bool deletedHighlight = false;
        if (section && epub) {
          int curPageIdx = section->currentPage;
          auto curPage = section->loadPageFromSectionFile();
          if (curPage) {
            int textLineCount = HighlightStore::countTextLines(*curPage);
            int cursorLine = highlightState.cursorLineIndex;
            if (cursorLine >= textLineCount) cursorLine = textLineCount - 1;
            if (cursorLine < 0) cursorLine = 0;
            auto savedHighlights =
                HighlightStore::loadHighlightsForPage(epub->getTitle(), currentSpineIndex, curPageIdx);
            for (const auto& hl : savedHighlights) {
              int sl = 0, sc = 0, el = 0, ec = -1;
              // Use the same role chain as rendering to verify this highlight is actually
              // visible on the current page. This prevents deleting the wrong highlight
              // when multiple highlights share a chapter and FULL matching fails.
              bool isMultiPage = (hl.startPage != hl.endPage);
              bool hlOnPage =
                  HighlightStore::findHighlightBounds(*curPage, hl.text, HighlightStore::HighlightPageRole::FULL, sl, sc, el, ec) ||
                  HighlightStore::findHighlightBounds(*curPage, hl.text, HighlightStore::HighlightPageRole::START, sl, sc, el, ec) ||
                  (isMultiPage && HighlightStore::findHighlightBounds(*curPage, hl.text, HighlightStore::HighlightPageRole::MIDDLE, sl, sc, el, ec)) ||
                  HighlightStore::findHighlightBounds(*curPage, hl.text, HighlightStore::HighlightPageRole::END, sl, sc, el, ec);
              if (!hlOnPage) continue;  // not visible on this page — skip
              if (cursorLine >= sl && cursorLine <= el) {
                HighlightStore::deleteHighlight(epub->getTitle(), hl.spineIndex, hl.startPage, hl.endPage);
                deletedHighlight = true;
                LOG_DBG("ERS", "Highlight deleted via single-tap: spine=%d pages=%d-%d", hl.spineIndex, hl.startPage,
                        hl.endPage);
                break;
              }
            }
          }
        }
        if (deletedHighlight) {
          highlightState.reset();
          highlightCachedPage = -1;
          GUI.drawPopup(renderer, "Highlight Removed");
          clearPopupTimer = millis() + 1000;
          requestUpdate();
          return;
        }
        highlightState.mode = HighlightState::SELECT;
        highlightState.selectionStartLine = highlightState.cursorLineIndex;
        highlightState.selectionEndLine = highlightState.cursorLineIndex;
        highlightState.selectionInitialized = true;

        // Snap to sentence start (cross-page backward) and find sentence end
        highlightState.selectionStartCharOffset = 0;
        highlightState.selectionEndCharOffset = -1;
        highlightState.selectionStartPage = section ? section->currentPage : -1;
        highlightState.selectionEndPage = section ? section->currentPage : -1;
        if (section) {
          const int curPageIdx = section->currentPage;
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            int lineCount = HighlightStore::countTextLines(*tempPage);

            // === Find sentence start — may be on a previous page ===
            int selStartPage = curPageIdx;
            int selStartLine = highlightState.cursorLineIndex;
            int selStartChar = 0;

            // Helper: given a line's text, find the start of the sentence
            // that ENDS this line (returns sentEnd pos, or -1 if none).
            // Shared lambda not available in C++17 easily — use inline logic.

            // Step 1: check cursor line for an internal sentence boundary
            {
              std::string curLineText = HighlightStore::getLineText(*tempPage, selStartLine);
              int fss = HighlightStore::findFirstSentenceStart(curLineText);
              if (fss > 0) {
                selStartChar = fss;
                // sentence boundary found on this line — done
              } else {
                // Step 2: walk backward within current page
                bool found = false;
                for (int li = selStartLine - 1; li >= 0 && !found; li--) {
                  std::string lineText = HighlightStore::getLineText(*tempPage, li);
                  int sentEnd = HighlightStore::findLastSentenceEnd(lineText);
                  if (sentEnd > 0) {
                    int nextStart = sentEnd;
                    while (nextStart < (int)lineText.size() && lineText[nextStart] == ' ') nextStart++;
                    if (nextStart < (int)lineText.size()) {
                      selStartLine = li;
                      selStartChar = nextStart;
                    } else {
                      selStartLine = li + 1;
                      selStartChar = 0;
                    }
                    found = true;
                  }
                }
                // Step 3: if we hit line 0 without finding a boundary, check previous page
                if (!found && curPageIdx > 0) {
                  int savedPage = section->currentPage;
                  section->currentPage = curPageIdx - 1;
                  auto prevPage = section->loadPageFromSectionFile();
                  section->currentPage = savedPage;
                  if (prevPage) {
                    int prevLineCount = HighlightStore::countTextLines(*prevPage);
                    for (int li = prevLineCount - 1; li >= 0 && !found; li--) {
                      std::string lineText = HighlightStore::getLineText(*prevPage, li);
                      int sentEnd = HighlightStore::findLastSentenceEnd(lineText);
                      if (sentEnd > 0) {
                        int nextStart = sentEnd;
                        while (nextStart < (int)lineText.size() && lineText[nextStart] == ' ') nextStart++;
                        if (nextStart < (int)lineText.size()) {
                          selStartPage = curPageIdx - 1;
                          selStartLine = li;
                          selStartChar = nextStart;
                        } else if (li + 1 < prevLineCount) {
                          selStartPage = curPageIdx - 1;
                          selStartLine = li + 1;
                          selStartChar = 0;
                        } else {
                          // sentence starts at line 0 of the current page
                          selStartPage = curPageIdx;
                          selStartLine = 0;
                          selStartChar = 0;
                        }
                        found = true;
                      }
                    }
                  }
                  if (!found) {
                    selStartLine = 0;
                    selStartChar = 0;
                  }
                } else if (!found) {
                  selStartLine = 0;
                  selStartChar = 0;
                }
              }
            }

            highlightState.selectionStartPage = selStartPage;
            highlightState.selectionStartLine = selStartLine;
            highlightState.selectionStartCharOffset = selStartChar;
            highlightState.selectionEndPage = selStartPage;
            highlightState.selectionEndLine = selStartLine;

            // === Find sentence end — search from start position, possibly across pages ===
            bool foundEnd = false;

            // If start is on the previous page, search there first
            if (selStartPage < curPageIdx) {
              int savedPage = section->currentPage;
              section->currentPage = selStartPage;
              auto startPageObj = section->loadPageFromSectionFile();
              section->currentPage = savedPage;
              if (startPageObj) {
                int spLines = HighlightStore::countTextLines(*startPageObj);
                for (int li = selStartLine; li < spLines && !foundEnd; li++) {
                  std::string lineText = HighlightStore::getLineText(*startPageObj, li);
                  int ci = (li == selStartLine) ? selStartChar : 0;
                  for (; ci < (int)lineText.size(); ci++) {
                    char c = lineText[ci];
                    if (c == '.' || c == '!' || c == '?') {
                      highlightState.selectionEndPage = selStartPage;
                      highlightState.selectionEndLine = li;
                      highlightState.selectionEndCharOffset = ci + 1;
                      foundEnd = true;
                      break;
                    }
                  }
                }
              }
            }

            // Search current page
            if (!foundEnd) {
              int startLi = (selStartPage == curPageIdx) ? selStartLine : 0;
              int startCi = (selStartPage == curPageIdx) ? selStartChar : 0;
              for (int li = startLi; li < lineCount && !foundEnd; li++) {
                std::string lineText = HighlightStore::getLineText(*tempPage, li);
                int ci = (li == startLi) ? startCi : 0;
                for (; ci < (int)lineText.size(); ci++) {
                  char c = lineText[ci];
                  if (c == '.' || c == '!' || c == '?') {
                    highlightState.selectionEndPage = curPageIdx;
                    highlightState.selectionEndLine = li;
                    highlightState.selectionEndCharOffset = ci + 1;
                    foundEnd = true;
                    break;
                  }
                }
              }
            }

            // Search next page
            if (!foundEnd && curPageIdx + 1 < section->pageCount) {
              int savedPage = section->currentPage;
              section->currentPage = curPageIdx + 1;
              auto nextPage = section->loadPageFromSectionFile();
              section->currentPage = savedPage;
              if (nextPage) {
                int nextLineCount = HighlightStore::countTextLines(*nextPage);
                for (int li = 0; li < nextLineCount && !foundEnd; li++) {
                  std::string lineText = HighlightStore::getLineText(*nextPage, li);
                  for (int ci = 0; ci < (int)lineText.size(); ci++) {
                    char c = lineText[ci];
                    if (c == '.' || c == '!' || c == '?') {
                      highlightState.selectionEndPage = curPageIdx + 1;
                      highlightState.selectionEndLine = li;
                      highlightState.selectionEndCharOffset = ci + 1;
                      foundEnd = true;
                      break;
                    }
                  }
                }
              }
            }

            if (!foundEnd) {
              highlightState.selectionEndLine = selStartLine;
              highlightState.selectionEndCharOffset = -1;
            }
          }
        }

        // If the sentence starts on a previous page, turn back there so user can see it
        if (section && highlightState.selectionStartPage >= 0 &&
            highlightState.selectionStartPage != section->currentPage) {
          section->currentPage = highlightState.selectionStartPage;
        }

        LOG_DBG("ERS", "Highlight mode: entered SELECT at line %d (startChar=%d, endChar=%d)",
                highlightState.cursorLineIndex, highlightState.selectionStartCharOffset,
                highlightState.selectionEndCharOffset);
        requestUpdate();
        return;
      }
      // If INACTIVE: set pending flag so page turn fires below
      if (highlightState.mode == HighlightState::INACTIVE) {
        pendingPowerPageTurn = true;
      }
      // If SELECT: single Power tap MOVES to the next sentence (not extend — drop old, highlight new)
      if (highlightState.mode == HighlightState::SELECT) {
        RenderLock lock(*this);
        if (section) {
          int curEndPage =
              (highlightState.selectionEndPage >= 0) ? highlightState.selectionEndPage : section->currentPage;
          int savedPage = section->currentPage;
          section->currentPage = curEndPage;
          auto tempPage = section->loadPageFromSectionFile();
          section->currentPage = savedPage;
          if (tempPage) {
            const int lineCount = HighlightStore::countTextLines(*tempPage);
            int searchLine = highlightState.selectionEndLine;
            int searchChar = (highlightState.selectionEndCharOffset == -1)
                                 ? static_cast<int>(HighlightStore::getLineText(*tempPage, searchLine).size())
                                 : highlightState.selectionEndCharOffset;

            // Move selection START to the beginning of the next sentence
            // (skip whitespace after the old sentence-ending punctuation)
            {
              int newStartPage = curEndPage;
              int newStartLine = searchLine;
              int newStartChar = searchChar;
              bool startFound = false;
              for (int li = newStartLine; li < lineCount && !startFound; li++) {
                std::string lt = HighlightStore::getLineText(*tempPage, li);
                int ci = (li == newStartLine) ? newStartChar : 0;
                while (ci < static_cast<int>(lt.size()) && lt[ci] == ' ') ci++;
                if (ci < static_cast<int>(lt.size())) {
                  newStartLine = li;
                  newStartChar = ci;
                  startFound = true;
                }
              }
              if (!startFound && curEndPage + 1 < section->pageCount) {
                newStartPage = curEndPage + 1;
                newStartLine = 0;
                newStartChar = 0;
              }
              highlightState.selectionStartPage = newStartPage;
              highlightState.selectionStartLine = newStartLine;
              highlightState.selectionStartCharOffset = newStartChar;
            }

            // Find the end of the next sentence
            bool found = false;
            for (int li = searchLine; li < lineCount && !found; li++) {
              std::string lineText = HighlightStore::getLineText(*tempPage, li);
              int ci = (li == searchLine) ? searchChar : 0;
              for (; ci < static_cast<int>(lineText.size()); ci++) {
                char c = lineText[ci];
                if (c == '.' || c == '!' || c == '?') {
                  highlightState.selectionEndPage = curEndPage;
                  highlightState.selectionEndLine = li;
                  highlightState.selectionEndCharOffset = ci + 1;
                  found = true;
                  break;
                }
              }
            }
            if (!found && curEndPage + 1 < section->pageCount) {
              section->currentPage = curEndPage + 1;
              auto nextPage = section->loadPageFromSectionFile();
              section->currentPage = savedPage;
              if (nextPage) {
                int nextLineCount = HighlightStore::countTextLines(*nextPage);
                for (int li = 0; li < nextLineCount && !found; li++) {
                  std::string lineText = HighlightStore::getLineText(*nextPage, li);
                  for (int ci = 0; ci < (int)lineText.size(); ci++) {
                    char c = lineText[ci];
                    if (c == '.' || c == '!' || c == '?') {
                      highlightState.selectionEndPage = curEndPage + 1;
                      highlightState.selectionEndLine = li;
                      highlightState.selectionEndCharOffset = ci + 1;
                      found = true;
                      break;
                    }
                  }
                }
              }
            }
          }
        }
        // Navigate to the start page so user sees the beginning of the new selection.
        // (Consistent with "enter SELECT" behaviour — show where the sentence starts,
        // not where it ends. Use Down button to extend to end page if needed.)
        if (section && highlightState.selectionStartPage >= 0 &&
            highlightState.selectionStartPage != section->currentPage) {
          section->currentPage = highlightState.selectionStartPage;
        }
        requestUpdate();
        return;
      }
    }
  }  // if (SETTINGS.highlightModeEnabled)

  // --- HIGHLIGHT MODE --- Input interception when active
  if (highlightState.mode != HighlightState::INACTIVE) {
    // Long-press Back → cancel and exit highlight mode (from any state)
    if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= highlightLongPressMs) {
      RenderLock lock(*this);
      highlightState.reset();
      highlightCachedPage = -1;
      LOG_DBG("ERS", "Highlight mode: cancelled via long-press Back");
      requestUpdate();
      return;
    }

    // Left+Right chord (either button pressed while other is held): clear ALL highlights on current page,
    // stay in highlight mode (useful when erase is being finicky or to wipe a whole page at once)
    if ((mappedInput.wasPressed(MappedInputManager::Button::Left) && mappedInput.isPressed(MappedInputManager::Button::Right)) ||
        (mappedInput.wasPressed(MappedInputManager::Button::Right) && mappedInput.isPressed(MappedInputManager::Button::Left))) {
      RenderLock lock(*this);
      if (section && epub) {
        int curPageIdx = section->currentPage;
        auto chordPage = section->loadPageFromSectionFile();
        auto savedHighlights = HighlightStore::loadHighlightsForPage(epub->getTitle(), currentSpineIndex, curPageIdx);
        int count = 0;
        for (const auto& hl : savedHighlights) {
          // Only delete highlights that actually render on this page (same logic as rendering).
          // Prevents wiping valid highlights on other pages of the same chapter.
          if (chordPage) {
            int sl, sc, el, ec;
            bool isMultiPage = (hl.startPage != hl.endPage);
            bool visible =
                HighlightStore::findHighlightBounds(*chordPage, hl.text, HighlightStore::HighlightPageRole::FULL, sl, sc, el, ec) ||
                HighlightStore::findHighlightBounds(*chordPage, hl.text, HighlightStore::HighlightPageRole::START, sl, sc, el, ec) ||
                (isMultiPage && HighlightStore::findHighlightBounds(*chordPage, hl.text, HighlightStore::HighlightPageRole::MIDDLE, sl, sc, el, ec)) ||
                HighlightStore::findHighlightBounds(*chordPage, hl.text, HighlightStore::HighlightPageRole::END, sl, sc, el, ec);
            if (!visible) continue;
          }
          HighlightStore::deleteHighlight(epub->getTitle(), hl.spineIndex, hl.startPage, hl.endPage);
          count++;
        }
        highlightCachedPage = -1;
        if (count > 0) {
          LOG_DBG("ERS", "Highlight chord: cleared %d highlight(s) on page %d", count, curPageIdx);
          GUI.drawPopup(renderer, "Highlights Cleared");
          clearPopupTimer = millis() + 1000;
        } else {
          LOG_DBG("ERS", "Highlight chord: no highlights on page %d to clear", curPageIdx);
        }
        requestUpdate();
      }
      return;
    }

    if (highlightState.mode == HighlightState::CURSOR) {
      // BTN_UP / BTN_DOWN move cursor line by line
      if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
        RenderLock lock(*this);
        if (highlightState.cursorLineIndex > 0) {
          highlightState.cursorLineIndex--;
          LOG_DBG("ERS", "Highlight cursor up → line %d", highlightState.cursorLineIndex);
          requestUpdate();
        }
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
        RenderLock lock(*this);
        // Step 5: Clamp cursor to actual number of text lines on current page
        // We need the page to count lines, but we don't have it in loop().
        // Instead, we allow the cursor to advance freely here and clamp in render().
        // A max safety limit prevents runaway values.
        highlightState.cursorLineIndex++;
        if (highlightState.cursorLineIndex > 200) highlightState.cursorLineIndex = 200;  // safety cap
        LOG_DBG("ERS", "Highlight cursor down → line %d", highlightState.cursorLineIndex);
        requestUpdate();
        return;
      }
    }

    if (highlightState.mode == HighlightState::SELECT) {
      // BTN_UP shrinks selection by one line; handles cross-page case
      if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
        RenderLock lock(*this);
        if (section) {
          const int selEndPage =
              (highlightState.selectionEndPage >= 0) ? highlightState.selectionEndPage : section->currentPage;
          const int selStartPage =
              (highlightState.selectionStartPage >= 0) ? highlightState.selectionStartPage : section->currentPage;
          const bool crossPage = (selEndPage > selStartPage);
          const bool canShrink = crossPage || (highlightState.selectionEndLine > highlightState.selectionStartLine);
          if (canShrink) {
            if (crossPage && highlightState.selectionEndLine == 0) {
              // Cross-page: retreat end to the last line of the previous page
              highlightState.selectionEndPage = selEndPage - 1;
              int savedPage = section->currentPage;
              section->currentPage = highlightState.selectionEndPage;
              auto prevPage = section->loadPageFromSectionFile();
              section->currentPage = savedPage;
              if (prevPage) {
                int prevLineCount = HighlightStore::countTextLines(*prevPage);
                highlightState.selectionEndLine = prevLineCount - 1;
                std::string lineText = HighlightStore::getLineText(*prevPage, highlightState.selectionEndLine);
                int sentenceEnd = HighlightStore::findLastSentenceEnd(lineText);
                highlightState.selectionEndCharOffset = (sentenceEnd > 0) ? sentenceEnd : -1;
                // Turn view to the new end page
                section->currentPage = highlightState.selectionEndPage;
              }
            } else {
              // Normal shrink on the current end page
              highlightState.selectionEndLine--;
              highlightState.selectionEndCharOffset = -1;
              auto tempPage = section->loadPageFromSectionFile();
              if (tempPage) {
                std::string lineText = HighlightStore::getLineText(*tempPage, highlightState.selectionEndLine);
                int sentenceEnd = HighlightStore::findLastSentenceEnd(lineText);
                highlightState.selectionEndCharOffset = (sentenceEnd > 0) ? sentenceEnd : -1;
              }
            }
            LOG_DBG("ERS", "Highlight select shrunk → page=%d line=%d (endChar=%d)", highlightState.selectionEndPage,
                    highlightState.selectionEndLine, highlightState.selectionEndCharOffset);
            requestUpdate();
          }
        }
        return;
      }

      // BTN_DOWN extends selection by exactly one sentence per press; crosses page boundary if needed
      if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
        RenderLock lock(*this);
        if (section) {
          for (int step = 0; step < 1; step++) {
            int curEndPage =
                (highlightState.selectionEndPage >= 0) ? highlightState.selectionEndPage : section->currentPage;
            int savedPage = section->currentPage;
            section->currentPage = curEndPage;
            auto tempPage = section->loadPageFromSectionFile();
            section->currentPage = savedPage;
            if (!tempPage) break;
            const int lineCount = HighlightStore::countTextLines(*tempPage);
            int searchLine = highlightState.selectionEndLine;
            int searchChar = (highlightState.selectionEndCharOffset == -1)
                                 ? static_cast<int>(HighlightStore::getLineText(*tempPage, searchLine).size())
                                 : highlightState.selectionEndCharOffset;
            bool found = false;
            for (int li = searchLine; li < lineCount && !found; li++) {
              std::string lineText = HighlightStore::getLineText(*tempPage, li);
              int ci = (li == searchLine) ? searchChar : 0;
              for (; ci < static_cast<int>(lineText.size()); ci++) {
                char c = lineText[ci];
                if (c == '.' || c == '!' || c == '?') {
                  highlightState.selectionEndPage = curEndPage;
                  highlightState.selectionEndLine = li;
                  highlightState.selectionEndCharOffset = ci + 1;
                  found = true;
                  break;
                }
              }
            }
            if (!found && curEndPage + 1 < section->pageCount) {
              section->currentPage = curEndPage + 1;
              auto nextPage = section->loadPageFromSectionFile();
              section->currentPage = savedPage;
              if (nextPage) {
                int nextLineCount = HighlightStore::countTextLines(*nextPage);
                for (int li = 0; li < nextLineCount && !found; li++) {
                  std::string lineText = HighlightStore::getLineText(*nextPage, li);
                  for (int ci = 0; ci < (int)lineText.size(); ci++) {
                    char c = lineText[ci];
                    if (c == '.' || c == '!' || c == '?') {
                      highlightState.selectionEndPage = curEndPage + 1;
                      highlightState.selectionEndLine = li;
                      highlightState.selectionEndCharOffset = ci + 1;
                      found = true;
                      break;
                    }
                  }
                }
              }
            }
            if (!found) break;
            // Stop advancing if this step crossed a page boundary
            if (highlightState.selectionEndPage != curEndPage) break;
          }
        }
        LOG_DBG("ERS", "Highlight select extended → page=%d line=%d char=%d", highlightState.selectionEndPage,
                highlightState.selectionEndLine, highlightState.selectionEndCharOffset);
        // Auto-turn to the end page so the user can see what's selected
        if (section && highlightState.selectionEndPage >= 0 &&
            highlightState.selectionEndPage != section->currentPage) {
          section->currentPage = highlightState.selectionEndPage;
        }
        requestUpdate();
        return;
      }

      // Left rocker Back → fine-adjust selection start: move left by one word
      if (mappedInput.wasReleased(MappedInputManager::Button::Back) &&
          mappedInput.getHeldTime() < highlightLongPressMs) {
        RenderLock lock(*this);
        if (highlightState.selectionStartCharOffset > 0 && section) {
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            std::string lineText = HighlightStore::getLineText(*tempPage, highlightState.selectionStartLine);
            int off = highlightState.selectionStartCharOffset - 1;
            // Skip any trailing space immediately before current position
            while (off > 0 && off < (int)lineText.size() && lineText[off] == ' ') off--;
            // Walk back to the start of the current/previous word
            while (off > 0 && lineText[off - 1] != ' ') off--;
            highlightState.selectionStartCharOffset = off;
          } else {
            highlightState.selectionStartCharOffset--;
          }
        } else if (highlightState.selectionStartCharOffset > 0) {
          highlightState.selectionStartCharOffset--;
        } else if (highlightState.selectionStartLine > 0 && section) {
          // At start of line — cross to last word of previous line
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            highlightState.selectionStartLine--;
            std::string prevLine = HighlightStore::getLineText(*tempPage, highlightState.selectionStartLine);
            int off = (int)prevLine.size();
            while (off > 0 && prevLine[off - 1] == ' ') off--;
            while (off > 0 && prevLine[off - 1] != ' ') off--;
            highlightState.selectionStartCharOffset = off;
          }
        }
        LOG_DBG("ERS", "Highlight start word-left → %d", highlightState.selectionStartCharOffset);
        requestUpdate();
        return;
      }

      // Left rocker Confirm → fine-adjust selection start: move right by one word
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        RenderLock lock(*this);
        if (section) {
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            std::string lineText = HighlightStore::getLineText(*tempPage, highlightState.selectionStartLine);
            int off = highlightState.selectionStartCharOffset;
            // Skip past the current word
            while (off < (int)lineText.size() && lineText[off] != ' ') off++;
            // Skip the space(s)
            while (off < (int)lineText.size() && lineText[off] == ' ') off++;
            if (off < (int)lineText.size()) {
              highlightState.selectionStartCharOffset = off;
            } else if (highlightState.selectionStartLine < highlightState.selectionEndLine) {
              // At end of line — cross to start of next line
              highlightState.selectionStartLine++;
              highlightState.selectionStartCharOffset = 0;
            }
          } else {
            highlightState.selectionStartCharOffset++;
          }
        } else {
          highlightState.selectionStartCharOffset++;
        }
        LOG_DBG("ERS", "Highlight start word-right → %d", highlightState.selectionStartCharOffset);
        requestUpdate();
        return;
      }

      // Right rocker Left → fine-adjust selection end: move left by one word
      if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
        RenderLock lock(*this);
        if (section) {
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            std::string lineText = HighlightStore::getLineText(*tempPage, highlightState.selectionEndLine);
            int off = highlightState.selectionEndCharOffset;
            if (off == -1) off = static_cast<int>(lineText.size());
            // Skip back past any trailing space
            while (off > 0 && off <= (int)lineText.size() && lineText[off - 1] == ' ') off--;
            // Walk back to start of this word
            while (off > 0 && lineText[off - 1] != ' ') off--;
            // Point to just before the space — i.e. end of previous word
            if (off > 0) {
              // find end of previous word
              int wordEnd = off - 1;
              while (wordEnd > 0 && lineText[wordEnd - 1] != ' ') wordEnd--;
              // off = start of current word, so end of previous is off-1 minus trailing spaces
              highlightState.selectionEndCharOffset = off - 1;
              while (highlightState.selectionEndCharOffset > 0 &&
                     lineText[highlightState.selectionEndCharOffset] == ' ')
                highlightState.selectionEndCharOffset--;
              highlightState.selectionEndCharOffset++;  // one past last char of prev word
            } else {
              highlightState.selectionEndCharOffset = 0;
            }
          } else {
            // fallback: resolve -1 then decrement
            if (highlightState.selectionEndCharOffset == -1) highlightState.selectionEndCharOffset = 50;
            if (highlightState.selectionEndCharOffset > 0) highlightState.selectionEndCharOffset--;
          }
          // If we've hit the very start of the end line, cross to end of previous line
          if (highlightState.selectionEndCharOffset == 0 &&
              highlightState.selectionEndLine > highlightState.selectionStartLine) {
            highlightState.selectionEndLine--;
            highlightState.selectionEndCharOffset = -1;
          }
        } else {
          if (highlightState.selectionEndCharOffset == -1) highlightState.selectionEndCharOffset = 50;
          if (highlightState.selectionEndCharOffset > 0) highlightState.selectionEndCharOffset--;
        }
        LOG_DBG("ERS", "Highlight end word-left → %d", highlightState.selectionEndCharOffset);
        requestUpdate();
        return;
      }

      // Right rocker Right → fine-adjust selection end: move right by one word
      if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
        RenderLock lock(*this);
        if (section) {
          auto tempPage = section->loadPageFromSectionFile();
          if (tempPage) {
            int lineCount = HighlightStore::countTextLines(*tempPage);
            if (highlightState.selectionEndCharOffset == -1) {
              // At end of current line — cross to next line
              if (highlightState.selectionEndLine + 1 < lineCount) {
                highlightState.selectionEndLine++;
                std::string nextLine = HighlightStore::getLineText(*tempPage, highlightState.selectionEndLine);
                int off = 0;
                // Move past first word to land after it
                while (off < (int)nextLine.size() && nextLine[off] != ' ') off++;
                while (off < (int)nextLine.size() && nextLine[off] == ' ') off++;
                highlightState.selectionEndCharOffset = (off < (int)nextLine.size()) ? off : -1;
              }
            } else {
              std::string lineText = HighlightStore::getLineText(*tempPage, highlightState.selectionEndLine);
              int off = highlightState.selectionEndCharOffset;
              // Skip past any space at current position
              while (off < (int)lineText.size() && lineText[off] == ' ') off++;
              // Skip past the current word
              while (off < (int)lineText.size() && lineText[off] != ' ') off++;
              // Skip trailing space(s) to land on start of next word
              while (off < (int)lineText.size() && lineText[off] == ' ') off++;
              if (off >= (int)lineText.size()) {
                highlightState.selectionEndCharOffset = -1;  // reached end of line
              } else {
                highlightState.selectionEndCharOffset = off;
              }
            }
          } else {
            highlightState.selectionEndCharOffset++;
          }
        }
        LOG_DBG("ERS", "Highlight end word-right → %d", highlightState.selectionEndCharOffset);
        requestUpdate();
        return;
      }
    }

    // Consume ALL other inputs silently — this blocks page turns, formatting, menu, etc.
    // regardless of which physical button is mapped to what action.
    if (mappedInput.wasAnyReleased() || mappedInput.wasAnyPressed()) {
      return;
    }
    return;
  }
  // --- HIGHLIGHT MODE --- (end of input interception)

  // --- CONFIRM BUTTON (MENU / HELP) ---
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && mappedInput.getHeldTime() > formattingToggleMs) {
      // Only show the help overlay for orientations where we have correct hint positions.
      // Inverted portrait and CW landscape do nothing on long-press.
      const bool overlaySupported = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT ||
                                     SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CCW);
      if (overlaySupported) {
        showHelpOverlay = true;
        requestUpdate();
      }
      return;
    }

    const int currentPage = section ? section->currentPage + 1 : 0;
    const int totalPages = section ? section->pageCount : 0;
    float bookProgress = 0.0f;
    if (epub && epub->getBookSize() > 0 && section && section->pageCount > 0) {
      const float chapterProgress = static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
      bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f;
    }
    const int bookProgressPercent = clampPercent(static_cast<int>(bookProgress + 0.5f));
    exitActivity();
    enterNewActivity(new EpubReaderMenuActivity(
        this->renderer, this->mappedInput, epub->getTitle(), currentPage, totalPages, bookProgressPercent, bookProgress,
        totalBookBytes, SETTINGS.orientation, [this](const uint8_t orientation) { onReaderMenuBack(orientation); },
        [this](EpubReaderMenuActivity::MenuAction action) { onReaderMenuConfirm(action); }));
  }

  // --- BACK BUTTON LOGIC (Go Home / Dark Mode) ---
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= goHomeMs) {
    onGoBack();
    return;
  }

  if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL) {
    static unsigned long lastBackRelease = 0;
    static bool waitingForBack = false;

    if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
      if (waitingForBack && (millis() - lastBackRelease < doubleClickMs)) {
        waitingForBack = false;
        isNightMode = !isNightMode;
        GUI.drawPopup(renderer, isNightMode ? "Dark Mode" : "Light Mode");
        clearPopupTimer = millis() + 1000;
        requestUpdate();
        return;
      } else {
        waitingForBack = true;
        lastBackRelease = millis();
      }
    }

    if (waitingForBack && (millis() - lastBackRelease > doubleClickMs)) {
      waitingForBack = false;
      onGoHome();
      return;
    }
  } else {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
      onGoHome();
      return;
    }
  }

  // --- BUTTON ROLE ASSIGNMENT ---
  // Determines which physical buttons control formatting vs. page navigation,
  // based on orientation and the user's swap preferences.
  //
  // Default portrait:   Left/Right = format,   PageBack/PageForward = navigate
  // Swapped portrait:   PageBack/PageForward = format,   Left/Right = navigate
  // Default landscape:  PageBack/PageForward = format,   Left/Right = navigate
  // Swapped landscape:  Left/Right = format,   PageBack/PageForward = navigate

  MappedInputManager::Button btnFormatDec;
  MappedInputManager::Button btnFormatInc;
  MappedInputManager::Button btnNavPrev;
  MappedInputManager::Button btnNavNext;

  const bool isLandscape = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CW ||
                            SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CCW);

  if (!isLandscape) {
    // Portrait (normal or inverted)
    if (SETTINGS.swapPortraitControls == 1) {
      btnFormatDec = MappedInputManager::Button::PageBack;
      btnFormatInc = MappedInputManager::Button::PageForward;
      btnNavPrev = MappedInputManager::Button::Left;
      btnNavNext = MappedInputManager::Button::Right;
    } else {
      btnFormatDec = MappedInputManager::Button::Left;
      btnFormatInc = MappedInputManager::Button::Right;
      btnNavPrev = MappedInputManager::Button::PageBack;
      btnNavNext = MappedInputManager::Button::PageForward;
    }
  } else {
    // Landscape (CW or CCW)
    if (SETTINGS.swapLandscapeControls == 1) {
      btnFormatDec = MappedInputManager::Button::Left;
      btnFormatInc = MappedInputManager::Button::Right;
      btnNavPrev = MappedInputManager::Button::PageBack;
      btnNavNext = MappedInputManager::Button::PageForward;
    } else {
      btnFormatDec = MappedInputManager::Button::PageBack;
      btnFormatInc = MappedInputManager::Button::PageForward;
      btnNavPrev = MappedInputManager::Button::Left;
      btnNavNext = MappedInputManager::Button::Right;
    }
  }

  // --- HANDLE FORMAT DEC ---
  bool executeFormatDecSingle = false;

  if (SETTINGS.buttonModMode != CrossPointSettings::MOD_OFF && mappedInput.wasReleased(btnFormatDec)) {
    if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && mappedInput.getHeldTime() > formattingToggleMs) {
      waitingForFormatDec = false;
      {
        RenderLock lock(*this);
        if (section) {
          cachedSpineIndex = currentSpineIndex;
          cachedChapterTotalPageCount = section->pageCount;
          nextPageNumber = section->currentPage;
        }
        SETTINGS.lineSpacing++;
        if (SETTINGS.lineSpacing >= CrossPointSettings::LINE_COMPRESSION_COUNT) {
          SETTINGS.lineSpacing = 0;
        }
        SETTINGS.saveToFile();
        section.reset();
      }
      const char* spacingMsg = "Spacing: Normal";
      if (SETTINGS.lineSpacing == CrossPointSettings::LINE_COMPRESSION::TIGHT) {
        spacingMsg = "Spacing: Tight";
      } else if (SETTINGS.lineSpacing == CrossPointSettings::LINE_COMPRESSION::WIDE) {
        spacingMsg = "Spacing: Wide";
      }
      GUI.drawPopup(renderer, spacingMsg);
      clearPopupTimer = millis() + 1000;
      requestUpdate();
      return;
    } else {
      if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && waitingForFormatDec &&
          (millis() - lastFormatDecRelease < doubleClickMs)) {
        waitingForFormatDec = false;
        {
          RenderLock lock(*this);
          if (section) {
            cachedSpineIndex = currentSpineIndex;
            cachedChapterTotalPageCount = section->pageCount;
            nextPageNumber = section->currentPage;
          }
          if (SETTINGS.paragraphAlignment == CrossPointSettings::PARAGRAPH_ALIGNMENT::LEFT_ALIGN) {
            SETTINGS.paragraphAlignment = CrossPointSettings::PARAGRAPH_ALIGNMENT::JUSTIFIED;
          } else {
            SETTINGS.paragraphAlignment = CrossPointSettings::PARAGRAPH_ALIGNMENT::LEFT_ALIGN;
          }
          SETTINGS.saveToFile();
          section.reset();
        }
        if (SETTINGS.paragraphAlignment == CrossPointSettings::PARAGRAPH_ALIGNMENT::LEFT_ALIGN) {
          GUI.drawPopup(renderer, "Align: Left");
        } else {
          GUI.drawPopup(renderer, "Align: Justified");
        }
        clearPopupTimer = millis() + 1000;
        requestUpdate();
        return;
      } else {
        if (SETTINGS.buttonModMode == CrossPointSettings::MOD_SIMPLE) {
          executeFormatDecSingle = true;
        } else {
          waitingForFormatDec = true;
          lastFormatDecRelease = millis();
        }
      }
    }
  }

  if (waitingForFormatDec && (millis() - lastFormatDecRelease > doubleClickMs)) {
    waitingForFormatDec = false;
    executeFormatDecSingle = true;
  }

  if (executeFormatDecSingle) {
    bool changed = false;
    bool limitReached = false;
    {
      RenderLock lock(*this);
      if (SETTINGS.fontSize > CrossPointSettings::FONT_SIZE::SMALL) {
        SETTINGS.fontSize--;
        changed = true;
      } else {
        limitReached = true;
      }
      if (changed) {
        if (section) {
          cachedSpineIndex = currentSpineIndex;
          cachedChapterTotalPageCount = section->pageCount;
          nextPageNumber = section->currentPage;
        }
        SETTINGS.saveToFile();
        section.reset();
      }
    }
    if (changed) {
      requestUpdate();
    } else if (limitReached) {
      GUI.drawPopup(renderer, "Min Size Reached");
      clearPopupTimer = millis() + 1000;
    }
  }

  // --- HANDLE FORMAT INC ---
  bool executeFormatIncSingle = false;

  if (SETTINGS.buttonModMode != CrossPointSettings::MOD_OFF && mappedInput.wasReleased(btnFormatInc)) {
    if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && mappedInput.getHeldTime() > formattingToggleMs) {
      waitingForFormatInc = false;
      uint8_t newOrientation = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT)
                                   ? CrossPointSettings::ORIENTATION::LANDSCAPE_CCW
                                   : CrossPointSettings::ORIENTATION::PORTRAIT;
      applyOrientation(newOrientation);
      const char* orientMsg = (newOrientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? "Portrait" : "Landscape";
      GUI.drawPopup(renderer, orientMsg);
      clearPopupTimer = millis() + 1000;
      requestUpdate();
      return;
    } else {
      if (SETTINGS.buttonModMode == CrossPointSettings::MOD_FULL && waitingForFormatInc &&
          (millis() - lastFormatIncRelease < doubleClickMs)) {
        // DOUBLE CLICK: Toggle Bold (Full Mode Only)
        waitingForFormatInc = false;
        const char* boldMsg;
        {
          RenderLock lock(*this);

          if (section) {
            cachedSpineIndex = currentSpineIndex;
            cachedChapterTotalPageCount = section->pageCount;
            nextPageNumber = section->currentPage;
          }

          SETTINGS.forceBoldText = (SETTINGS.forceBoldText == 0) ? 1 : 0;
          boldMsg = (SETTINGS.forceBoldText == 1) ? "Bold: ON" : "Bold: OFF";
          SETTINGS.saveToFile();

          // Reset section to force rebuild with proper font metrics
          if (epub) {
            uint16_t backupSpine = currentSpineIndex;
            uint16_t backupPage = section ? section->currentPage : 0;
            uint16_t backupPageCount = section ? section->pageCount : 0;

            section.reset();
            saveProgress(backupSpine, backupPage, backupPageCount);
          } else {
            section.reset();
          }
        }
        GUI.drawPopup(renderer, boldMsg);
        clearPopupTimer = millis() + 1000;
        requestUpdate();
        return;
      } else {
        if (SETTINGS.buttonModMode == CrossPointSettings::MOD_SIMPLE) {
          executeFormatIncSingle = true;
        } else {
          waitingForFormatInc = true;
          lastFormatIncRelease = millis();
        }
      }
    }
  }

  if (waitingForFormatInc && (millis() - lastFormatIncRelease > doubleClickMs)) {
    waitingForFormatInc = false;
    executeFormatIncSingle = true;
  }

  if (executeFormatIncSingle) {
    bool changed = false;
    bool limitReached = false;
    {
      RenderLock lock(*this);
      if (SETTINGS.fontSize < CrossPointSettings::FONT_SIZE::EXTRA_LARGE) {
        SETTINGS.fontSize++;
        changed = true;
      } else {
        limitReached = true;
      }
      if (changed) {
        if (section) {
          cachedSpineIndex = currentSpineIndex;
          cachedChapterTotalPageCount = section->pageCount;
          nextPageNumber = section->currentPage;
        }
        SETTINGS.saveToFile();
        section.reset();
      }
    }
    if (changed) {
      requestUpdate();
    } else if (limitReached) {
      GUI.drawPopup(renderer, "Max Size Reached");
      clearPopupTimer = millis() + 1000;
    }
  }

  // --- HANDLE NAVIGATION BUTTONS ---
  const bool usePressForPageTurn = !SETTINGS.longPressChapterSkip;

  bool prevTriggered = usePressForPageTurn ? mappedInput.wasPressed(btnNavPrev) : mappedInput.wasReleased(btnNavPrev);

  const bool powerPageTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                             (pendingPowerPageTurn || mappedInput.wasReleased(MappedInputManager::Button::Power));
  pendingPowerPageTurn = false;

  bool nextTriggered = usePressForPageTurn ? (mappedInput.wasPressed(btnNavNext) || powerPageTurn)
                                           : (mappedInput.wasReleased(btnNavNext) || powerPageTurn);

  if (SETTINGS.buttonModMode == CrossPointSettings::MOD_OFF) {
    if (usePressForPageTurn) {
      if (mappedInput.wasPressed(btnFormatDec)) prevTriggered = true;
      if (mappedInput.wasPressed(btnFormatInc)) nextTriggered = true;
    } else {
      if (mappedInput.wasReleased(btnFormatDec)) prevTriggered = true;
      if (mappedInput.wasReleased(btnFormatInc)) nextTriggered = true;
    }
  }

  if (!prevTriggered && !nextTriggered) {
    return;
  }

  if (currentSpineIndex > 0 && currentSpineIndex >= epub->getSpineItemsCount()) {
    currentSpineIndex = epub->getSpineItemsCount() - 1;
    nextPageNumber = UINT16_MAX;
    requestUpdate();
    return;
  }

  const bool skipChapter = SETTINGS.longPressChapterSkip && mappedInput.getHeldTime() > skipChapterMs;

  if (skipChapter) {
    // We don't want to delete the section mid-render, so grab the semaphore
    {
      RenderLock lock(*this);
      nextPageNumber = 0;
      currentSpineIndex = nextTriggered ? currentSpineIndex + 1 : currentSpineIndex - 1;
      section.reset();
    }
    requestUpdate();
    return;
  }

  if (!section) {
    requestUpdate();
    return;
  }

  if (prevTriggered) {
    if (section->currentPage > 0) {
      section->currentPage--;
    } else if (currentSpineIndex > 0) {
      // We don't want to delete the section mid-render, so grab the semaphore
      {
        RenderLock lock(*this);
        nextPageNumber = UINT16_MAX;
        currentSpineIndex--;
        section.reset();
      }
    }
    requestUpdate();
  } else {
    if (section->currentPage < section->pageCount - 1) {
      section->currentPage++;
    } else {
      // We don't want to delete the section mid-render, so grab the semaphore
      {
        RenderLock lock(*this);
        nextPageNumber = 0;
        currentSpineIndex++;
        section.reset();
      }
    }
    requestUpdate();
  }
}

void EpubReaderActivity::onReaderMenuBack(const uint8_t orientation) {
  exitActivity();
  applyOrientation(orientation);
  requestUpdate();
}

void EpubReaderActivity::jumpToPercent(int percent) {
  if (!epub) {
    return;
  }

  const size_t bookSize = epub->getBookSize();
  if (bookSize == 0) {
    return;
  }

  percent = clampPercent(percent);

  size_t targetSize =
      (bookSize / 100) * static_cast<size_t>(percent) + (bookSize % 100) * static_cast<size_t>(percent) / 100;
  if (percent >= 100) {
    targetSize = bookSize - 1;
  }

  const int spineCount = epub->getSpineItemsCount();
  if (spineCount == 0) {
    return;
  }

  int targetSpineIndex = spineCount - 1;
  size_t prevCumulative = 0;

  for (int i = 0; i < spineCount; i++) {
    const size_t cumulative = epub->getCumulativeSpineItemSize(i);
    if (targetSize <= cumulative) {
      targetSpineIndex = i;
      prevCumulative = (i > 0) ? epub->getCumulativeSpineItemSize(i - 1) : 0;
      break;
    }
  }

  const size_t cumulative = epub->getCumulativeSpineItemSize(targetSpineIndex);
  const size_t spineSize = (cumulative > prevCumulative) ? (cumulative - prevCumulative) : 0;
  pendingSpineProgress =
      (spineSize == 0) ? 0.0f : static_cast<float>(targetSize - prevCumulative) / static_cast<float>(spineSize);
  if (pendingSpineProgress < 0.0f) {
    pendingSpineProgress = 0.0f;
  } else if (pendingSpineProgress > 1.0f) {
    pendingSpineProgress = 1.0f;
  }

  // Reset state so render() reloads and repositions on the target spine.
  {
    RenderLock lock(*this);
    currentSpineIndex = targetSpineIndex;
    nextPageNumber = 0;
    pendingPercentJump = true;
    section.reset();
  }
}

void EpubReaderActivity::onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action) {
  switch (action) {
    case EpubReaderMenuActivity::MenuAction::SELECT_CHAPTER: {
      const int currentP = section ? section->currentPage : 0;
      const int totalP = section ? section->pageCount : 0;
      const int spineIdx = currentSpineIndex;
      const std::string path = epub->getPath();

      // 1. Close the menu
      exitActivity();
      enterNewActivity(new EpubReaderChapterSelectionActivity(
          this->renderer, this->mappedInput, epub, path, spineIdx, currentP, totalP,
          [this] {
            exitActivity();
            requestUpdate();
          },
          [this](const int newSpineIndex) {
            if (currentSpineIndex != newSpineIndex) {
              currentSpineIndex = newSpineIndex;
              nextPageNumber = 0;
              section.reset();
            }
            exitActivity();
            requestUpdate();
          },
          [this](const int newSpineIndex, const int newPage) {
            if (currentSpineIndex != newSpineIndex || (section && section->currentPage != newPage)) {
              currentSpineIndex = newSpineIndex;
              nextPageNumber = newPage;
              section.reset();
            }
            exitActivity();
            requestUpdate();
          }));

      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_TO_PERCENT: {
      float bookProgress = 0.0f;
      if (epub && epub->getBookSize() > 0 && section && section->pageCount > 0) {
        const float chapterProgress = static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
        bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f;
      }
      const int initialPercent = clampPercent(static_cast<int>(bookProgress + 0.5f));
      exitActivity();
      enterNewActivity(new EpubReaderPercentSelectionActivity(
          renderer, mappedInput, initialPercent,
          [this](const int percent) {
            jumpToPercent(percent);
            exitActivity();
            requestUpdate();
          },
          [this]() {
            exitActivity();
            requestUpdate();
          }));
      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_HOME: {
      pendingGoHome = true;
      break;
    }
    case EpubReaderMenuActivity::MenuAction::DELETE_CACHE: {
      {
        RenderLock lock(*this);
        if (epub) {
          // 2. BACKUP: Read current progress
          // We use the current variables that track our position
          uint16_t backupSpine = currentSpineIndex;
          uint16_t backupPage = section->currentPage;
          uint16_t backupPageCount = section->pageCount;

          section.reset();
          // 3. WIPE: Clear the cache directory
          epub->clearCache();

          // 4. RESTORE: Re-setup the directory and rewrite the progress file
          epub->setupCacheDir();

          saveProgress(backupSpine, backupPage, backupPageCount);
        }
      }
      // Defer go home to avoid race condition with display task
      pendingGoHome = true;
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SYNC: {
      if (KOREADER_STORE.hasCredentials()) {
        const int currentPage = section ? section->currentPage : 0;
        const int totalPages = section ? section->pageCount : 0;
        exitActivity();
        enterNewActivity(new KOReaderSyncActivity(
            renderer, mappedInput, epub, epub->getPath(), currentSpineIndex, currentPage, totalPages,
            [this]() { pendingSubactivityExit = true; },
            [this](int newSpineIndex, int newPage) {
              if (currentSpineIndex != newSpineIndex || (section && section->currentPage != newPage)) {
                currentSpineIndex = newSpineIndex;
                nextPageNumber = newPage;
                section.reset();
              }
              pendingSubactivityExit = true;
            }));
      }
      break;
    }
    case EpubReaderMenuActivity::MenuAction::BUTTON_MOD_SETTINGS: {
      SETTINGS.buttonModMode = (SETTINGS.buttonModMode + 1) % CrossPointSettings::BUTTON_MOD_MODE_COUNT;
      SETTINGS.saveToFile();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SWAP_CONTROLS: {
      SETTINGS.swapPortraitControls = (SETTINGS.swapPortraitControls == 0) ? 1 : 0;
      SETTINGS.saveToFile();
      pendingSubactivityExit = true;
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SWAP_LANDSCAPE_CONTROLS: {
      SETTINGS.swapLandscapeControls = (SETTINGS.swapLandscapeControls == 0) ? 1 : 0;
      SETTINGS.saveToFile();
      pendingSubactivityExit = true;
      break;
    }
  }
}

void EpubReaderActivity::applyOrientation(const uint8_t orientation) {
  if (SETTINGS.orientation == orientation) {
    return;
  }

  // Preserve current reading position so we can restore after reflow.
  {
    RenderLock lock(*this);
    if (section) {
      cachedSpineIndex = currentSpineIndex;
      cachedChapterTotalPageCount = section->pageCount;
      nextPageNumber = section->currentPage;
    }

    // Persist the selection so the reader keeps the new orientation on next launch.
    SETTINGS.orientation = orientation;
    SETTINGS.saveToFile();

    // Update renderer orientation to match the new logical coordinate system.
    applyReaderOrientation(renderer, SETTINGS.orientation);

    // Reset section to force re-layout in the new orientation.
    section.reset();
  }
}

// TODO: Failure handling
void EpubReaderActivity::render(Activity::RenderLock&& lock) {
  if (!epub) {
    return;
  }

  if (currentSpineIndex < 0) {
    currentSpineIndex = 0;
  }
  if (currentSpineIndex > epub->getSpineItemsCount()) {
    currentSpineIndex = epub->getSpineItemsCount();
  }

  if (currentSpineIndex == epub->getSpineItemsCount()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_END_OF_BOOK), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += SETTINGS.screenMargin;
  orientedMarginLeft += SETTINGS.screenMargin;
  orientedMarginRight += SETTINGS.screenMargin;
  orientedMarginBottom += SETTINGS.screenMargin;

  auto metrics = UITheme::getInstance().getMetrics();

  if (SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::NONE) {
    const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
    orientedMarginBottom += statusBarMargin - SETTINGS.screenMargin +
                            (showProgressBar ? (metrics.bookProgressBarHeight + progressBarMarginTop) : 0);
  }

  if (!section) {
    const auto filepath = epub->getSpineItem(currentSpineIndex).href;
    LOG_DBG("ERS", "Loading file: %s, index: %d", filepath.c_str(), currentSpineIndex);
    section = std::unique_ptr<Section>(new Section(epub, currentSpineIndex, renderer));

    const uint16_t viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
    const uint16_t viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;

    bool useBold = (SETTINGS.forceBoldText == 1);

    // TURN ON GLOBAL BOLD FOR CACHE BUILDER
    EpdFontFamily::globalForceBold = useBold;

    if (!section->loadSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                  SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                  viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle, useBold)) {
      LOG_DBG("ERS", "Cache not found, building...");

      const auto popupFn = [this]() { GUI.drawPopup(renderer, tr(STR_INDEXING)); };

      if (!section->createSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                      SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                      viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle, useBold,
                                      popupFn)) {
        LOG_ERR("ERS", "Failed to persist page data to SD");
        section.reset();

        // Ensure bold is off before exiting on fail
        EpdFontFamily::globalForceBold = false;
        return;
      }
    } else {
      LOG_DBG("ERS", "Cache found, skipping build...");
    }

    // TURN GLOBAL BOLD BACK OFF
    EpdFontFamily::globalForceBold = false;

    if (nextPageNumber == UINT16_MAX) {
      section->currentPage = section->pageCount - 1;
    } else {
      section->currentPage = nextPageNumber;
    }

    if (cachedChapterTotalPageCount > 0) {
      if (currentSpineIndex == cachedSpineIndex && section->pageCount != cachedChapterTotalPageCount) {
        float progress = static_cast<float>(section->currentPage) / static_cast<float>(cachedChapterTotalPageCount);
        section->currentPage = static_cast<int>(progress * section->pageCount);
      }
      cachedChapterTotalPageCount = 0;
    }

    if (pendingPercentJump && section->pageCount > 0) {
      int newPage = static_cast<int>(pendingSpineProgress * static_cast<float>(section->pageCount));
      if (newPage >= section->pageCount) {
        newPage = section->pageCount - 1;
      }
      section->currentPage = newPage;
      pendingPercentJump = false;
    }
  }

  renderer.clearScreen();

  if (section->pageCount == 0) {
    LOG_DBG("ERS", "No pages to render");
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_CHAPTER), true, EpdFontFamily::BOLD);
    renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
    renderer.displayBuffer();
    return;
  }

  if (section->currentPage < 0 || section->currentPage >= section->pageCount) {
    LOG_DBG("ERS", "Page out of bounds: %d (max %d)", section->currentPage, section->pageCount);
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_OUT_OF_BOUNDS), true, EpdFontFamily::BOLD);
    renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
    renderer.displayBuffer();
    return;
  }

  {
    auto p = section->loadPageFromSectionFile();
    if (!p) {
      LOG_ERR("ERS", "Failed to load page from SD - clearing section cache");
      section->clearCache();
      section.reset();
      requestUpdate();  // Try again after clearing cache
      // TODO: prevent infinite loop if the page keeps failing to load for some reason
      return;
    }
    const auto start = millis();
    renderContents(std::move(p), orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
    LOG_DBG("ERS", "Rendered page in %dms", millis() - start);
    renderer.clearFontCache();
  }
  saveProgress(currentSpineIndex, section->currentPage, section->pageCount);
}

void EpubReaderActivity::saveProgress(int spineIndex, int currentPage, int pageCount) {
  FsFile f;
  if (Storage.openFileForWrite("ERS", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[6];
    data[0] = currentSpineIndex & 0xFF;
    data[1] = (currentSpineIndex >> 8) & 0xFF;
    data[2] = currentPage & 0xFF;
    data[3] = (currentPage >> 8) & 0xFF;
    data[4] = pageCount & 0xFF;
    data[5] = (pageCount >> 8) & 0xFF;
    f.write(data, 6);
    f.close();
    LOG_DBG("ERS", "Progress saved: Chapter %d, Page %d", spineIndex, currentPage);
  } else {
    LOG_ERR("ERS", "Could not save progress!");
  }
}

void EpubReaderActivity::renderContents(std::unique_ptr<Page> page, const int orientedMarginTop,
                                        const int orientedMarginRight, const int orientedMarginBottom,
                                        const int orientedMarginLeft) {
  bool useBold = (SETTINGS.forceBoldText == 1);
  const bool inHighlightMode = (highlightState.mode != HighlightState::INACTIVE);

  // --- HIGHLIGHT MODE FAST PATH ---
  // If we have a cached clean page buffer for this exact page, restore from cache
  // instead of re-rendering all text. Saves ~100-200ms per cursor move.
  const int currentPageIdx = section ? section->currentPage : -1;
  bool usedCache = false;
  if (inHighlightMode && highlightCachedPage == currentPageIdx && renderer.hasBwBufferStored()) {
    usedCache = renderer.restoreBwBufferKeep();
    if (usedCache) {
      LOG_DBG("ERS", "Highlight fast path: restored cached page %d", currentPageIdx);
    }
  }

  if (!usedCache) {
    // Invalidate stale cache when doing a full render
    if (highlightCachedPage != currentPageIdx) {
      highlightCachedPage = -1;
    }
  }

  // Force special handling for pages with images when anti-aliasing is on
  bool imagePageWithAA = !usedCache && page->hasImages() && SETTINGS.textAntiAliasing;

  if (!usedCache) {
    // Draw the normal black text
    EpdFontFamily::globalForceBold = useBold;
    page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);

    // IMMEDIATELY TURN OFF BOLD SO THE UI REMAINS NORMAL
    EpdFontFamily::globalForceBold = false;

    renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginLeft);

    if (isNightMode) {
      renderer.invertScreen();
    }

    // Cache the clean page (before highlights) for fast cursor moves
    if (inHighlightMode) {
      renderer.storeBwBuffer();
      highlightCachedPage = currentPageIdx;
    }
  }

  // --- HIGHLIGHT MODE --- Rendering overlay (Step 5) + Visual indicator (Step 7)
  bool drewHighlights = false;  // tracks whether any highlights were drawn this frame
  if (highlightState.mode != HighlightState::INACTIVE) {
    const int fontId = SETTINGS.getReaderFontId();
    const int textLineCount = HighlightStore::countTextLines(*page);

    // Step 7: Draw a 2px border around the content area to signal highlight mode
    {
      const int bx = orientedMarginLeft - 2;
      const int by = orientedMarginTop - 2;
      const int bw = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight + 4;
      const int bh = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom + 4;
      const bool ink = !isNightMode;
      renderer.fillRect(bx, by, bw, 2, ink);           // top
      renderer.fillRect(bx, by + bh - 2, bw, 2, ink);  // bottom
      renderer.fillRect(bx, by, 2, bh, ink);           // left
      renderer.fillRect(bx + bw - 2, by, 2, bh, ink);  // right
    }

    if (highlightState.mode == HighlightState::CURSOR && textLineCount > 0) {
      // Clamp cursor to valid range
      int cursorLine = highlightState.cursorLineIndex;
      if (cursorLine >= textLineCount) cursorLine = textLineCount - 1;
      if (cursorLine < 0) cursorLine = 0;

      // Get the geometry for this text line
      int16_t lineY = 0, lineH = 0;
      if (HighlightStore::getLineGeometry(*page, fontId, cursorLine, lineY, lineH)) {
        // Draw inverted bar across the full width of the content area
        const int barX = orientedMarginLeft;
        const int barW = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
        const int barY = lineY + orientedMarginTop;
        // In light mode: black bar + white text. In night mode (buffer already inverted):
        // white bar + black text, so the highlight contrasts against the dark background.
        drewHighlights = true;
        renderer.fillRect(barX, barY, barW, lineH, !isNightMode);
        int textIdx = 0;
        for (const auto& el : page->elements) {
          if (el->getTag() == TAG_PageLine) {
            if (textIdx == cursorLine) {
              auto* pl = static_cast<PageLine*>(el.get());
              const auto& block = pl->getBlock();
              const auto& words = block->getWords();
              const auto& xpositions = block->getWordXPositions();
              auto wordIt = words.begin();
              auto xposIt = xpositions.begin();
              for (size_t w = 0; w < words.size(); w++) {
                renderer.drawText(fontId, *xposIt + orientedMarginLeft, pl->yPos + orientedMarginTop, wordIt->c_str(),
                                  isNightMode);
                ++wordIt;
                ++xposIt;
              }
              break;
            }
            textIdx++;
          }
        }
      }
    } else if (highlightState.mode == HighlightState::SELECT && textLineCount > 0) {
      // Determine this page's role in the selection
      const int viewPage = section ? section->currentPage : -1;
      const int selStartPage = (highlightState.selectionStartPage >= 0) ? highlightState.selectionStartPage : viewPage;
      const int selEndPage = (highlightState.selectionEndPage >= 0) ? highlightState.selectionEndPage : viewPage;
      const bool onStartPage = (viewPage == selStartPage);
      const bool onEndPage = (viewPage == selEndPage);

      // Start of highlighted range: line 0 if we've turned past the start page
      int startLine = onStartPage ? highlightState.selectionStartLine : 0;
      // End of highlighted range: last line if selection continues to a later page
      int endLine = onEndPage ? highlightState.selectionEndLine : (textLineCount - 1);
      const bool endOnLaterPage = !onEndPage;

      if (startLine < 0) startLine = 0;
      if (endLine >= textLineCount) endLine = textLineCount - 1;

      for (int lineIdx = startLine; lineIdx <= endLine; lineIdx++) {
        int16_t lineY = 0, lineH = 0;
        if (!HighlightStore::getLineGeometry(*page, fontId, lineIdx, lineY, lineH)) continue;

        // Find the PageLine pointer for this text line index
        PageLine* pl = nullptr;
        {
          int textIdx = 0;
          for (const auto& el : page->elements) {
            if (el->getTag() == TAG_PageLine) {
              if (textIdx == lineIdx) {
                pl = static_cast<PageLine*>(el.get());
                break;
              }
              textIdx++;
            }
          }
        }
        if (!pl) continue;

        // Compute bar dimensions, trimming for start/end char offsets
        int barX = orientedMarginLeft;
        int barW = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;

        // For start line: trim left edge (only on the page where selection begins)
        if (onStartPage && lineIdx == startLine && highlightState.selectionStartCharOffset > 0) {
          int16_t startPx = charOffsetToStartPixel(pl, highlightState.selectionStartCharOffset);
          barX = orientedMarginLeft + startPx;
          barW = (renderer.getScreenWidth() - orientedMarginRight) - barX;
        }
        // For end line: trim right edge (only if end is on this page)
        if (!endOnLaterPage && lineIdx == endLine && highlightState.selectionEndCharOffset != -1 &&
            highlightState.selectionEndCharOffset > 0) {
          int endPx = charOffsetToEndPixel(pl, highlightState.selectionEndCharOffset);
          if (endPx >= 0) {
            barW = endPx - (barX - orientedMarginLeft);
          }
          // endPx == -1 means charOffset is in the last word: keep full width
        }
        if (barW <= 0) continue;  // nothing to draw

        const int barY = lineY + orientedMarginTop;

        drewHighlights = true;
        renderer.fillRect(barX, barY, barW, lineH, !isNightMode);

        const auto& words = pl->getBlock()->getWords();
        const auto& xpositions = pl->getBlock()->getWordXPositions();
        auto wordIt = words.begin();
        auto xposIt = xpositions.begin();
        for (size_t w = 0; w < words.size(); w++) {
          int wordAbsX = *xposIt + orientedMarginLeft;
          // Only draw words that fall within the highlight bar — avoids inverted-text bleed
          if (wordAbsX >= barX && wordAbsX < barX + barW) {
            renderer.drawText(fontId, wordAbsX, pl->yPos + orientedMarginTop, wordIt->c_str(), isNightMode);
          }
          ++wordIt;
          ++xposIt;
        }
      }
    }
  }
  // --- HIGHLIGHT MODE ---

  // --- HIGHLIGHT MODE --- Draw saved (persisted) highlights
  if (SETTINGS.highlightModeEnabled && section) {
    const int fontId = SETTINGS.getReaderFontId();
    int currentPageIdx = section->currentPage;
    auto savedHighlights = HighlightStore::loadHighlightsForPage(epub->getTitle(), currentSpineIndex, currentPageIdx);

    for (const auto& hl : savedHighlights) {
      // Try FULL first (both first and last word on this page), then START (first word
      // here, continues to next page), then MIDDLE (intermediate page, entire page
      // highlighted — only for multi-page highlights), then END (last word here, started
      // on previous page). MIDDLE must be tried before END — without it, END would
      // incorrectly match on intermediate pages whenever the highlight's last word happens
      // to appear there, causing only the top line(s) to be highlighted instead of the
      // full page. MIDDLE is skipped for single-page highlights to avoid ghost highlights
      // when a highlight reflows to a different page.
      int startLine = 0, startChar = 0, endLine = 0, endChar = -1;
      // Only attempt MIDDLE (entire-page highlight) for highlights that were originally
      // multi-page. Single-page highlights that reflow to a different page would otherwise
      // unconditionally match MIDDLE and ghost-highlight an unrelated page.
      bool isMultiPage = (hl.startPage != hl.endPage);
      if (!HighlightStore::findHighlightBounds(*page, hl.text, HighlightStore::HighlightPageRole::FULL, startLine,
                                               startChar, endLine, endChar) &&
          !HighlightStore::findHighlightBounds(*page, hl.text, HighlightStore::HighlightPageRole::START, startLine,
                                               startChar, endLine, endChar) &&
          !(isMultiPage && HighlightStore::findHighlightBounds(*page, hl.text, HighlightStore::HighlightPageRole::MIDDLE,
                                                               startLine, startChar, endLine, endChar)) &&
          !HighlightStore::findHighlightBounds(*page, hl.text, HighlightStore::HighlightPageRole::END, startLine,
                                               startChar, endLine, endChar))
        continue;
      const int textLineCount = HighlightStore::countTextLines(*page);
      if (startLine < 0) startLine = 0;
      if (endLine >= textLineCount) endLine = textLineCount - 1;

      for (int lineIdx = startLine; lineIdx <= endLine; lineIdx++) {
        int16_t lineY = 0, lineH = 0;
        if (!HighlightStore::getLineGeometry(*page, fontId, lineIdx, lineY, lineH)) continue;

        // Find the PageLine pointer for pixel-accurate trimming
        PageLine* pl = nullptr;
        {
          int tidx = 0;
          for (const auto& el : page->elements) {
            if (el->getTag() == TAG_PageLine) {
              if (tidx == lineIdx) {
                pl = static_cast<PageLine*>(el.get());
                break;
              }
              tidx++;
            }
          }
        }
        if (!pl) continue;

        int barX = orientedMarginLeft;
        int barW = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;

        // Trim left edge of first line to match sentence start word
        if (lineIdx == startLine && startChar > 0) {
          int16_t startPx = charOffsetToStartPixel(pl, startChar);
          barX = orientedMarginLeft + startPx;
          barW = (renderer.getScreenWidth() - orientedMarginRight) - barX;
        }
        // Trim right edge of last line to match sentence end word
        if (lineIdx == endLine && endChar != -1 && endChar > 0) {
          int endPx = charOffsetToEndPixel(pl, endChar);
          if (endPx >= 0) barW = endPx - (barX - orientedMarginLeft);
        }
        if (barW <= 0) continue;

        const int barY = lineY + orientedMarginTop;
        drewHighlights = true;
        renderer.fillRect(barX, barY, barW, lineH, !isNightMode);

        const auto& words = pl->getBlock()->getWords();
        const auto& xpositions = pl->getBlock()->getWordXPositions();
        auto wordIt = words.begin();
        auto xposIt = xpositions.begin();
        for (size_t w = 0; w < words.size(); w++) {
          int wordAbsX = *xposIt + orientedMarginLeft;
          if (wordAbsX >= barX && wordAbsX < barX + barW) {
            renderer.drawText(fontId, wordAbsX, pl->yPos + orientedMarginTop, wordIt->c_str(), isNightMode);
          }
          ++wordIt;
          ++xposIt;
        }
      }
    }
  }
  // --- HIGHLIGHT MODE ---

  // --- HELP OVERLAY RENDERING ---
  if (showHelpOverlay) {
    const int w = renderer.getScreenWidth();
    const int h = renderer.getScreenHeight();

    int32_t overlayFontId = SMALL_FONT_ID;
    int overlayLineHeight = 18;

    int dismissY = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? 500 : 300;
    int dismissX = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? w / 2 : w / 2 + 25;

    drawHelpBox(renderer, dismissX, dismissY,
                "PRESS ANY KEY\n"
                "TO DISMISS",
                BoxAlign::CENTER, overlayFontId, overlayLineHeight);

    // --- HIGHLIGHT MODE --- Step 8: Help overlay for highlight mode
    if (SETTINGS.highlightModeEnabled) {
      int hlHelpY = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::PORTRAIT) ? 60 : 40;
      drawHelpBox(renderer, w / 2, hlHelpY,
                  "HIGHLIGHT MODE\n"
                  "2x Pwr: Enter/Save\n"
                  "Up/Down: Move cursor\n"
                  "1x Pwr: Select sentence\n"
                  "1x Pwr: Next sentence\n"
                  "Down: Extend selection\n"
                  "L/R rocker: Fine adjust\n"
                  "Hold Back: Cancel",
                  BoxAlign::CENTER, overlayFontId, overlayLineHeight);
    }
    // --- HIGHLIGHT MODE ---

    const bool isLandscape = (SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CW ||
                              SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CCW);

    if (!isLandscape) {
      // Portrait help overlay
      if (SETTINGS.swapPortraitControls == 1) {
        // Swapped portrait: side buttons = format, front L/R = navigate
        drawHelpBox(renderer, 10, h - 80, "2x: Dark", BoxAlign::LEFT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h / 2 - 70,
                    "1x: Text size –\n"
                    "Hold: Spacing\n"
                    "2x: Alignment",
                    BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h / 2 + 10,
                    "1x: Text size +\n"
                    "Hold: Rotate\n"
                    "2x: Bold",
                    BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
      } else {
        // Default portrait: front L/R = format, side buttons = navigate
        drawHelpBox(renderer, 10, h - 80, "2x: Dark", BoxAlign::LEFT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 145, h - 80,
                    "1x: Text size –\n"
                    "Hold: Spacing\n"
                    "2x: Alignment",
                    BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
        drawHelpBox(renderer, w - 10, h - 80,
                    "1x: Text size +\n"
                    "Hold: Rotate\n"
                    "2x: Bold",
                    BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
      }
    } else {
      // Landscape help overlay — only render hint boxes for CCW (the primary landscape orientation).
      // CW and other landscape orientations are skipped to avoid wrong positions.
      if (SETTINGS.orientation == CrossPointSettings::ORIENTATION::LANDSCAPE_CCW) {
        if (SETTINGS.swapLandscapeControls == 1) {
          // Swapped CCW: front buttons = format, stacked vertically on the right side.
          drawHelpBox(renderer, w - 10, h - 40, "2x: Dark", BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
          drawHelpBox(renderer, w - 10, 20,
                      "1x: Text size +\n"
                      "Hold: Rotate\n"
                      "2x: Bold",
                      BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
          drawHelpBox(renderer, w - 10, 110,
                      "1x: Text size –\n"
                      "Hold: Spacing\n"
                      "2x: Alignment",
                      BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
        } else {
          // Default CCW: side buttons = format, top-center.
          drawHelpBox(renderer, w - 10, h - 40, "2x: Dark", BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
          drawHelpBox(renderer, w / 2 + 20, 20,
                      "1x: Text size –\n"
                      "Hold: Spacing\n"
                      "2x: Alignment",
                      BoxAlign::RIGHT, overlayFontId, overlayLineHeight);
          drawHelpBox(renderer, w / 2 + 30, 20,
                      "1x: Text size +\n"
                      "Hold: Rotate\n"
                      "2x: Bold",
                      BoxAlign::LEFT, overlayFontId, overlayLineHeight);
        }
      }
    }
  }

  // --- STANDARD or IMAGE REFRESH ---
  // In highlight mode, use displayHighlightBuffer() (lut_bw_fast: 4-frame A2-like LUT, ~3× faster
  // than OTP FAST_REFRESH) — the overlay is BW-only so grayscale quality doesn't matter.
  if (imagePageWithAA) {
    // Double FAST_REFRESH with selective image blanking (pablohc's technique):
    // HALF_REFRESH sets particles too firmly for the grayscale LUT to adjust.
    // Instead, blank only the image area and do two fast refreshes.
    // Step 1: Display page with image area blanked (text appears, image area white)
    // Step 2: Re-render with images and display again (images appear clean)
    int16_t imgX, imgY, imgW, imgH;
    if (page->getImageBoundingBox(imgX, imgY, imgW, imgH)) {
      renderer.fillRect(imgX + orientedMarginLeft, imgY + orientedMarginTop, imgW, imgH, false);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);

      // Re-render page content to restore images into the blanked area
      page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
      renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    } else {
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    }
    // Double FAST_REFRESH handles ghosting for image pages; don't count toward full refresh cadence
  } else if (!inHighlightMode && pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else if (inHighlightMode) {
    // Fast BW LUT (~4 frames vs OTP ~12): cuts highlight cursor latency by ~3×
    renderer.displayHighlightBuffer();
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    pagesUntilFullRefresh--;
  }

  // In highlight mode, the BW buffer is used as a page cache for fast cursor moves.
  // Skip the AA store/restore cycle — AA is already disabled during highlight mode.
  if (!inHighlightMode) {
    renderer.storeBwBuffer();

    // Anti-aliasing grayscale passes
    if (SETTINGS.textAntiAliasing && !showHelpOverlay && !isNightMode) {
      renderer.clearScreen(0x00);

      // TURN ON BOLD FOR GRAYSCALE PASSES
      EpdFontFamily::globalForceBold = useBold;

      // --- LSB (Light Grays) Pass ---
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
      page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
      renderer.copyGrayscaleLsbBuffers();

      renderer.clearScreen(0x00);

      // --- MSB (Dark Grays) Pass ---
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
      page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
      renderer.copyGrayscaleMsbBuffers();

      // TURN BOLD OFF BEFORE FINAL FLUSH
      EpdFontFamily::globalForceBold = false;

      renderer.displayGrayBuffer();
      renderer.setRenderMode(GfxRenderer::BW);
    }
    renderer.restoreBwBuffer();
  }
  // If anti-aliasing ran, it wiped highlights from the display. The BW buffer (restored above)
  // still has the highlights — do one fast refresh to put them back on screen.
  // (This path is only reached when !inHighlightMode, so drewHighlights here means persisted
  // saved-highlight bars drawn over regular reading — not the active cursor/selection overlay.)
  if (drewHighlights && SETTINGS.textAntiAliasing && !showHelpOverlay && !isNightMode && !inHighlightMode) {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

void EpubReaderActivity::renderStatusBar(const int orientedMarginRight, const int orientedMarginBottom,
                                         const int orientedMarginLeft) const {
  auto metrics = UITheme::getInstance().getMetrics();

  const bool showProgressPercentage = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL;
  const bool showBookProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                                   SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_BOOK_PROGRESS_BAR;
  const bool showChapterProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showProgressText = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR;
  const bool showBookPercentage = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showBattery = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::NO_PROGRESS ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showChapterTitle = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::NO_PROGRESS ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER;

  const auto screenHeight = renderer.getScreenHeight();
  const auto textY = screenHeight - orientedMarginBottom - 4;
  int progressTextWidth = 0;

  const float sectionChapterProg = static_cast<float>(section->currentPage) / section->pageCount;
  const float bookProgress = epub->calculateProgress(currentSpineIndex, sectionChapterProg) * 100;

  if (showProgressText || showProgressPercentage || showBookPercentage) {
    char progressStr[32];

    if (showProgressPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%d/%d  %.0f%%", section->currentPage + 1, section->pageCount,
               bookProgress);
    } else if (showBookPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%.0f%%", bookProgress);
    } else {
      snprintf(progressStr, sizeof(progressStr), "%d/%d", section->currentPage + 1, section->pageCount);
    }

    progressTextWidth = renderer.getTextWidth(SMALL_FONT_ID, progressStr);
    renderer.drawText(SMALL_FONT_ID, renderer.getScreenWidth() - orientedMarginRight - progressTextWidth, textY,
                      progressStr);
  }

  if (showBookProgressBar) {
    GUI.drawReadingProgressBar(renderer, static_cast<size_t>(bookProgress));
  }

  if (showChapterProgressBar) {
    const float chapterProgress =
        (section->pageCount > 0) ? (static_cast<float>(section->currentPage + 1) / section->pageCount) * 100 : 0;
    GUI.drawReadingProgressBar(renderer, static_cast<size_t>(chapterProgress));
  }

  if (showBattery) {
    GUI.drawBatteryLeft(renderer, Rect{orientedMarginLeft + 1, textY, metrics.batteryWidth, metrics.batteryHeight},
                        showBatteryPercentage);
  }

  if (showChapterTitle) {
    const int rendererableScreenWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;

    const int batterySize = showBattery ? (showBatteryPercentage ? 50 : 20) : 0;
    const int titleMarginLeft = batterySize + 30;
    const int titleMarginRight = progressTextWidth + 30;

    int titleMarginLeftAdjusted = std::max(titleMarginLeft, titleMarginRight);
    int availableTitleSpace = rendererableScreenWidth - 2 * titleMarginLeftAdjusted;
    const int tocIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);

    std::string title;
    int titleWidth;
    if (tocIndex == -1) {
      title = tr(STR_UNNAMED);
      titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    } else {
      const auto tocItem = epub->getTocItem(tocIndex);
      title = tocItem.title;
      titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
      if (titleWidth > availableTitleSpace) {
        availableTitleSpace = rendererableScreenWidth - titleMarginLeft - titleMarginRight;
        titleMarginLeftAdjusted = titleMarginLeft;
      }
      if (titleWidth > availableTitleSpace) {
        title = renderer.truncatedText(SMALL_FONT_ID, title.c_str(), availableTitleSpace);
        titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
      }
    }

    renderer.drawText(SMALL_FONT_ID,
                      titleMarginLeftAdjusted + orientedMarginLeft + (availableTitleSpace - titleWidth) / 2, textY,
                      title.c_str());
  }
}
