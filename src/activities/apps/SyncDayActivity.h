// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
#pragma once

#include <functional>

#include "../ActivityWithSubactivity.h"

class SyncDayActivity final : public ActivityWithSubactivity {
  bool wifiConnectedOnEnter = false;
  bool connectedInActivity = false;
  bool syncing = false;
  bool lastSyncSucceeded = false;
  bool lastSyncFailed = false;
  unsigned long lastClockRefreshMs = 0;
  std::function<void()> onComplete;

  void openWifiSelection();
  void syncTime();
  void updateClockTick();
  bool isWifiConnected() const;
  std::string getStatusMessage() const;

 public:
  explicit SyncDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                           std::function<void()> onComplete = nullptr)
      : ActivityWithSubactivity("SyncDay", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return syncing; }
};
