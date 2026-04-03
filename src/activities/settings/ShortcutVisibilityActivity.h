// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <string>

#include "../Activity.h"
#include "CrossPointSettings.h"
#include "util/ButtonNavigator.h"
#include "util/ShortcutRegistry.h"

class ShortcutVisibilityActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  ShortcutId shortcutId;
  uint8_t CrossPointSettings::* visiblePtr;
  int selectedIndex = 0;

  static constexpr int kOptionCount = 2;

 public:
  explicit ShortcutVisibilityActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                      ShortcutId shortcutId, uint8_t CrossPointSettings::* visiblePtr)
      : Activity("ShortcutVisibility", renderer, mappedInput),
        shortcutId(shortcutId),
        visiblePtr(visiblePtr) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
