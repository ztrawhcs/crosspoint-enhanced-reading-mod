#include "ActivityWithSubactivity.h"

void ActivityWithSubactivity::renderTaskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    {
      RenderLock lock(*this);
      if (!subActivity) {
        render(std::move(lock));
      }
      // If subActivity is set, consume the notification but skip parent render
      // Note: the sub-activity will call its render() from its own display task
    }
  }
}

void ActivityWithSubactivity::exitActivity() {
  // No need to lock, since onExit() already acquires its own lock
  if (subActivity) {
    LOG_DBG("ACT", "Exiting subactivity...");
    subActivity->onExit();
    subActivity.reset();
  }
}

void ActivityWithSubactivity::enterNewActivity(Activity* activity) {
  // Acquire lock to avoid 2 activities rendering at the same time during transition
  RenderLock lock(*this);
  subActivity.reset(activity);
  subActivity->onEnter();
}

void ActivityWithSubactivity::loop() {
  // Process any pending result from a finished sub-activity
  if (hasPendingResult) {
    hasPendingResult = false;
    auto handler = std::move(pendingResultHandler);
    pendingResultHandler = nullptr;
    if (handler) {
      handler(pendingResult);
    }
    requestUpdate();
    return;
  }

  if (subActivity) {
    subActivity->loop();
  }
}

void ActivityWithSubactivity::requestUpdate(bool immediate) {
  // Always propagate: in 1.2.0, ActivityManager owns rendering and must always be notified.
  // render() now delegates to subActivity when one is active, so there's no risk of the
  // parent overwriting the sub-activity's content.
  Activity::requestUpdate(immediate);
}

void ActivityWithSubactivity::onExit() {
  // No need to lock, onExit() already acquires its own lock
  exitActivity();
  Activity::onExit();
}

void ActivityWithSubactivity::startActivityForResult(std::unique_ptr<Activity> activity,
                                                     ActivityResultHandler resultHandler) {
  pendingResultHandler = std::move(resultHandler);
  hasPendingResult = false;
  activity->parentActivity = this;
  enterNewActivity(activity.release());
}

void ActivityWithSubactivity::finishSubactivity(const ActivityResult& result) {
  // Copy result BEFORE exitActivity() destroys the sub-activity that owns it.
  // 'result' is a reference into the sub-activity's member; calling exitActivity()
  // first frees that object, leaving 'result' as a dangling reference.
  ActivityResult resultCopy = result;
  exitActivity();
  pendingResult = std::move(resultCopy);
  hasPendingResult = true;
}
