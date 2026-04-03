// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class SleepAppActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::function<void()> onComplete;

  int selectedIndex = 0;

 public:
  enum class Item : int {
    SleepMode = 0,
    SleepDirectory,
    SleepImageOrder,
    _COUNT
  };

 public:
  explicit SleepAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                            std::function<void()> onComplete = nullptr)
      : Activity("SleepApp", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
