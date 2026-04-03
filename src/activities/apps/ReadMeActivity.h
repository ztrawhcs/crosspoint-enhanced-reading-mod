// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct ReadMeTopic {
  StrId titleId;
  std::vector<StrId> lineIds;
};

class ReadMeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::function<void()> onComplete;
  int selectedTopic = 0;
  std::vector<ReadMeTopic> topics;

  void buildTopics();

 public:
  explicit ReadMeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                          std::function<void()> onComplete = nullptr)
      : Activity("ReadMe", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
