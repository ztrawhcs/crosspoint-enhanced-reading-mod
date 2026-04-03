// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include "util/ButtonNavigator.h"

#include "../Activity.h"

class ReadingStatsExtendedActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int scrollOffset = 0;

 public:
  explicit ReadingStatsExtendedActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStatsExtended", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
