// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"

class ReadingHeatmapActivity final : public ActivityWithSubactivity {
  ButtonNavigator buttonNavigator;
  int viewedYear = 0;
  unsigned viewedMonth = 0;
  uint32_t selectedDayOrdinal = 0;
  bool waitForConfirmRelease = false;
  std::function<void()> onComplete;

  void goToAdjacentMonth(int delta);
  void goToReferenceMonth();
  void resetSelectedDay();
  void moveSelection(int delta);

 public:
  explicit ReadingHeatmapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                  std::function<void()> onComplete = nullptr)
      : ActivityWithSubactivity("ReadingHeatmap", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
