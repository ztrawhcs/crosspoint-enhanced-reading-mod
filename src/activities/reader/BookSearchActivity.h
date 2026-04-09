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
 * Phase 1 (fast): scans section cache .bin files (pre-extracted, no ZIP).
 * Phase 2 (opt-in): if uncached chapters exist, a selectable footer item
 *   lets the user search those too via EPUB HTML streaming (slower).
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
  // When false: uncached chapters are skipped and tracked for optional phase 2.
  // When true: uncached chapters are searched via EPUB HTML streaming.
  bool searchUncached = false;
  std::vector<int> uncachedSpines;  // spine indices with no cache file

  ButtonNavigator buttonNavigator;

  static constexpr int MAX_RESULTS = 16;
  static constexpr int MAX_QUERY_LEN = 40;
  static constexpr int SCAN_CHUNK = 512;

  // Total selectable items: results + 1 virtual "search unread chapters" item if applicable
  int totalItems() const {
    return static_cast<int>(results.size()) + (!searchUncached && !uncachedSpines.empty() ? 1 : 0);
  }
  bool isUncachedItem(int idx) const {
    return !searchUncached && !uncachedSpines.empty() && idx == static_cast<int>(results.size());
  }

  void startUncachedSearch();
  void searchSpineItemBinary(int spineIndex);
  void searchSpineItemHtml(int spineIndex);
  std::string getChapterTitle(int spineIndex) const;
};
