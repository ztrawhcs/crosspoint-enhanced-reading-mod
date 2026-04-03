// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include "util/ShortcutRegistry.h"

class AppsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  std::vector<ShortcutEntry> shortcuts;

  const std::function<void()> onGoHome;
  const std::function<void()> onGoToMyLibrary;
  const std::function<void()> onGoToRecentBooks;
  const std::function<void()> onGoToReadingStats;
  const std::function<void()> onGoToSyncDay;
  const std::function<void()> onGoToReadingHeatmap;
  const std::function<void()> onGoToAchievements;
  const std::function<void()> onGoToIfFound;
  const std::function<void()> onGoToReadMe;
  const std::function<void()> onGoToFileTransfer;
  const std::function<void()> onGoToSleep;
  const std::function<void()> onGoToSettings;

  void activateSelected();
  void rebuildShortcuts();

 public:
  explicit AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        const std::function<void()>& onGoHome,
                        const std::function<void()>& onGoToMyLibrary,
                        const std::function<void()>& onGoToRecentBooks,
                        const std::function<void()>& onGoToReadingStats,
                        const std::function<void()>& onGoToSyncDay,
                        const std::function<void()>& onGoToReadingHeatmap,
                        const std::function<void()>& onGoToAchievements,
                        const std::function<void()>& onGoToIfFound,
                        const std::function<void()>& onGoToReadMe,
                        const std::function<void()>& onGoToFileTransfer,
                        const std::function<void()>& onGoToSleep,
                        const std::function<void()>& onGoToSettings)
      : Activity("Apps", renderer, mappedInput),
        onGoHome(onGoHome),
        onGoToMyLibrary(onGoToMyLibrary),
        onGoToRecentBooks(onGoToRecentBooks),
        onGoToReadingStats(onGoToReadingStats),
        onGoToSyncDay(onGoToSyncDay),
        onGoToReadingHeatmap(onGoToReadingHeatmap),
        onGoToAchievements(onGoToAchievements),
        onGoToIfFound(onGoToIfFound),
        onGoToReadMe(onGoToReadMe),
        onGoToFileTransfer(onGoToFileTransfer),
        onGoToSleep(onGoToSleep),
        onGoToSettings(onGoToSettings) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
