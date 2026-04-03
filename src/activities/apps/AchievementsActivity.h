// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <vector>

#include "AchievementsStore.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class AchievementsActivity final : public Activity {
  enum class FilterTab : uint8_t { Pending = 0, Completed };

  ButtonNavigator buttonNavigator;
  bool waitForConfirmRelease = false;
  FilterTab selectedTab = FilterTab::Pending;
  int selectedIndex = 0;
  std::vector<AchievementView> achievements;
  std::vector<int> visibleIndexes;
  std::function<void()> onComplete;

  void refreshEntries();
  void rebuildVisibleIndexes();

 public:
  explicit AchievementsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                std::function<void()> onComplete = nullptr)
      : Activity("Achievements", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
