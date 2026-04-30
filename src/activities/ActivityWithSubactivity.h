#pragma once
#include <memory>

#include "Activity.h"
#include "ActivityResult.h"

class ActivityWithSubactivity : public Activity {
  friend class Activity;  // Allow Activity::finish() to call finishSubactivity()

 protected:
  std::unique_ptr<Activity> subActivity = nullptr;
  ActivityResultHandler pendingResultHandler = nullptr;
  ActivityResult pendingResult;
  bool hasPendingResult = false;

  void exitActivity();
  void enterNewActivity(Activity* activity);
  [[noreturn]] void renderTaskLoop();

 public:
  explicit ActivityWithSubactivity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity(std::move(name), renderer, mappedInput) {}
  void loop() override;
  // Note: when a subactivity is active, parent requestUpdate() calls are ignored;
  // the subactivity should request its own renders. This pauses parent rendering until exit.
  void requestUpdate(bool immediate = false) override;
  void onExit() override;

  // Start a sub-activity and call resultHandler when it finishes via finish()
  void startActivityForResult(std::unique_ptr<Activity> activity, ActivityResultHandler resultHandler);

  // Called by sub-activities to return to parent, invoking the result handler
  void finishSubactivity(const ActivityResult& result = ActivityResult{});
};
