// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../Activity.h"

class IfFoundActivity final : public Activity {
  std::function<void()> onComplete;

 public:
  explicit IfFoundActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                           std::function<void()> onComplete = nullptr)
      : Activity("IfFound", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
