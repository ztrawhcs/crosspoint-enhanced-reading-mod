#pragma once
#include <Epub.h>
#include <Epub/Section.h>

#include "EpubReaderMenuActivity.h"
#include "activities/ActivityWithSubactivity.h"

// --- HIGHLIGHT MODE ---
struct HighlightState {
  enum Mode { INACTIVE, CURSOR, SELECT };
  Mode mode = INACTIVE;
  int cursorLineIndex = 0;  // which PageLine (text-only) the cursor is on
  int selectionStartLine = 0;
  int selectionStartCharOffset = 0;
  int selectionEndLine = 0;
  int selectionEndCharOffset = 0;  // -1 = end of line (last char)
  int selectionStartPage = -1;     // page index where selection begins
  int selectionEndPage = -1;       // page index where selection ends
  bool selectionInitialized = false;

  void reset() {
    mode = INACTIVE;
    cursorLineIndex = 0;
    selectionStartLine = 0;
    selectionStartCharOffset = 0;
    selectionEndLine = 0;
    selectionEndCharOffset = 0;
    selectionStartPage = -1;
    selectionEndPage = -1;
    selectionInitialized = false;
  }
};
// --- HIGHLIGHT MODE ---

class EpubReaderActivity final : public ActivityWithSubactivity {
  std::shared_ptr<Epub> epub;
  std::unique_ptr<Section> section = nullptr;
  int currentSpineIndex = 0;
  int nextPageNumber = 0;
  int pagesUntilFullRefresh = 0;
  int cachedSpineIndex = 0;
  int cachedChapterTotalPageCount = 0;
  size_t totalBookBytes = 0;
  // Signals that the next render should reposition within the newly loaded section
  // based on a cross-book percentage jump.
  bool pendingPercentJump = false;
  // Normalized 0.0-1.0 progress within the target spine item, computed from book percentage.
  float pendingSpineProgress = 0.0f;
  bool pendingSubactivityExit = false;  // Defer subactivity exit to avoid use-after-free
  bool pendingGoHome = false;           // Defer go home to avoid race condition with display task
  bool skipNextButtonCheck = false;     // Skip button processing for one frame after subactivity exit
  // --- HIGHLIGHT MODE ---
  HighlightState highlightState;
  int previousSpineIndex = -1;   // Track spine changes for force-exit
  int highlightCachedPage = -1;  // Page index cached in BW buffer for fast cursor moves
  // --- HIGHLIGHT MODE ---
  const std::function<void()> onGoBack;
  const std::function<void()> onGoHome;

  void renderContents(std::unique_ptr<Page> page, int orientedMarginTop, int orientedMarginRight,
                      int orientedMarginBottom, int orientedMarginLeft);
  void renderStatusBar(int orientedMarginRight, int orientedMarginBottom, int orientedMarginLeft) const;
  void saveProgress(int spineIndex, int currentPage, int pageCount);
  // Jump to a percentage of the book (0-100), mapping it to spine and page.
  void jumpToPercent(int percent);
  void onReaderMenuBack(uint8_t orientation);
  void onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action);
  void applyOrientation(uint8_t orientation);

 public:
  explicit EpubReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Epub> epub,
                              const std::function<void()>& onGoBack, const std::function<void()>& onGoHome)
      : ActivityWithSubactivity("EpubReader", renderer, mappedInput),
        epub(std::move(epub)),
        onGoBack(onGoBack),
        onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&& lock) override;
};
