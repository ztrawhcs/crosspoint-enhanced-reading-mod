#pragma once
#include <Epub.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"

/**
 * Full-text book search activity.
 *
 * Presents the on-screen keyboard (KeyboardEntryActivity) to collect a query,
 * then scans each spine's pre-built section cache .bin file for the query
 * string (one spine per loop() call so the display can show progress).
 * Section cache files are pre-extracted binary files — no ZIP decompression,
 * so they're fast to scan.  Results list lets the user jump to any match.
 */
class BookSearchActivity final : public ActivityWithSubactivity {
 public:
  using OnResultCallback = std::function<void(int spineIndex)>;
  using OnCancelCallback = std::function<void()>;

  explicit BookSearchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub,
                              OnResultCallback onResult, OnCancelCallback onCancel)
      : ActivityWithSubactivity("BookSearch", renderer, mappedInput),
        epub(std::move(epub)),
        onResult(std::move(onResult)),
        onCancel(std::move(onCancel)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;

 private:
  struct SearchResult {
    int spineIndex;
    std::string chapterTitle;
    std::string snippet;
  };

  enum class State { ENTER_QUERY, SEARCHING, RESULTS, NO_RESULTS };

  std::shared_ptr<Epub> epub;
  OnResultCallback onResult;
  OnCancelCallback onCancel;

  State state = State::ENTER_QUERY;
  std::string queryLower;
  std::vector<SearchResult> results;
  int selectedResult = 0;
  int currentSearchSpine = 0;
  int skippedSpines = 0;  // sections with no cache file (not yet read by user)

  ButtonNavigator buttonNavigator;

  static constexpr int MAX_RESULTS = 8;
  static constexpr int MAX_QUERY_LEN = 40;
  static constexpr int SCAN_CHUNK = 512;

  void searchSpineItem(int spineIndex);
  std::string getChapterTitle(int spineIndex) const;
};
