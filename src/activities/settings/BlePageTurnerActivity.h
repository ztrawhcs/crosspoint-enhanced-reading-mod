#pragma once

#include <functional>
#include <string>
#include <vector>

#include "activities/Activity.h"

class BlePageTurnerActivity final : public Activity {
 public:
  explicit BlePageTurnerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::function<void()>& onBack)
      : Activity("BlePageTurner", renderer, mappedInput), onBack(onBack) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;

 private:
  const std::function<void()> onBack;
  int selectedIndex = 0;
  bool scanning = false;
  unsigned long scanStartMs = 0;
  static constexpr int SCAN_DURATION_SECS = 8;

  // Snapshot of scan results taken after scan completes (for stable rendering)
  std::vector<std::string> deviceMacs;
  std::vector<std::string> deviceNames;
};
