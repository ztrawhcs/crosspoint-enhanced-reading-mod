// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <string>

#include "../Activity.h"
#include "CrossPointSettings.h"
#include "util/ButtonNavigator.h"
#include "util/ShortcutRegistry.h"

class ShortcutOrderActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  ShortcutId shortcutId;
  uint8_t CrossPointSettings::* orderPtr;
  int selectedIndex = 0;

  static constexpr int kMaxOrder = 10;

 public:
  explicit ShortcutOrderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 ShortcutId shortcutId, uint8_t CrossPointSettings::* orderPtr)
      : Activity("ShortcutOrder", renderer, mappedInput),
        shortcutId(shortcutId),
        orderPtr(orderPtr) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
