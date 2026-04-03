// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"

class ReadingStatsActivity final : public ActivityWithSubactivity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool waitForConfirmRelease = false;
  std::function<void()> onComplete;
  void openSelectedEntry();

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                std::function<void()> onComplete = nullptr)
      : ActivityWithSubactivity("ReadingStats", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
