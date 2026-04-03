#pragma once
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

class RecentBooksActivity final : public ActivityWithSubactivity {
 private:
  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  // Recent tab state
  std::vector<RecentBook> recentBooks;

  // Callbacks
  const std::function<void(const std::string& path)> onSelectBook;
  const std::function<void()> onGoHome;

  // Data loading
  void loadRecentBooks();
  void confirmDeleteSelected();

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               const std::function<void()>& onGoHome,
                               const std::function<void(const std::string& path)>& onSelectBook)
      : ActivityWithSubactivity("RecentBooks", renderer, mappedInput), onSelectBook(onSelectBook), onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;
};
