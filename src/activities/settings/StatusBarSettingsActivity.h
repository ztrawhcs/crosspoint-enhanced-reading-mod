// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../Activity.h"
#include "CrossPointSettings.h"
#include "util/ButtonNavigator.h"

class StatusBarSettingsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  static constexpr int kModeCount = CrossPointSettings::STATUS_BAR_MODE_COUNT;

 public:
  explicit StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("StatusBarSettings", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
