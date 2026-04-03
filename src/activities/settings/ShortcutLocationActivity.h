// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>
#include <string>

#include "../Activity.h"
#include "CrossPointSettings.h"
#include "util/ButtonNavigator.h"
#include "util/ShortcutRegistry.h"

class ShortcutLocationActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  ShortcutId shortcutId;
  uint8_t CrossPointSettings::* locationPtr;
  int selectedIndex = 0;

  static constexpr int kLocationCount = 2;

 public:
  explicit ShortcutLocationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                    ShortcutId shortcutId, uint8_t CrossPointSettings::* locationPtr)
      : Activity("ShortcutLocation", renderer, mappedInput),
        shortcutId(shortcutId),
        locationPtr(locationPtr) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
