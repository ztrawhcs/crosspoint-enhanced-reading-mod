// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include "util/TimeZoneRegistry.h"

class TimeZoneSelectActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  int totalItems = 0;

 public:
  explicit TimeZoneSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TimeZoneSelect", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
