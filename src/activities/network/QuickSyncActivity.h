#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../Activity.h"

/**
 * One-tap "Quick Sync" activity.
 *
 * Reads /quicksync.json from the SD card (a single required field,
 * `manifest_url`, which already embeds the auth token), auto-joins the last
 * connected WiFi network (falling through the rest of the saved networks on
 * failure), fetches the manifest with the persisted cursor, downloads every
 * new EPUB, persists the new cursor, drops WiFi and offers the newest book.
 *
 * The whole feature is config-gated: without /quicksync.json on the SD card the
 * home menu entry never appears and this activity is unreachable.
 */
class QuickSyncActivity final : public Activity {
 public:
  enum class SyncState {
    CONNECTING,      // Walking the saved-network candidate list
    WIFI_SELECTION,  // Manual WifiSelectionActivity subactivity is active
    FETCHING,        // Fetching the manifest
    DOWNLOADING,     // Downloading new items
    DONE,            // "N new items" summary
    ERROR            // Error state with message
  };

  explicit QuickSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("QuickSync", renderer, mappedInput) {}

  /** True when /quicksync.json exists on the SD card (home-menu gate). */
  static bool isConfigured();

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // One manifest entry. Kept deliberately small: the server caps the manifest
  // at 25 items, so the whole vector is well under 4 KB of heap.
  struct SyncItem {
    std::string id;
    std::string title;
    std::string url;
    uint32_t seq = 0;
  };

  SyncState state = SyncState::CONNECTING;
  std::string manifestUrl;
  std::string destDir = "/";
  std::string errorMessage;
  std::string statusMessage;

  std::vector<std::string> candidateSsids;  // Saved networks, last-connected first
  size_t candidateIndex = 0;
  uint32_t connectStartTime = 0;
  bool wifiActivated = false;

  std::vector<SyncItem> items;
  size_t itemIndex = 0;
  int downloadedCount = 0;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;

  uint32_t cursor = 0;
  std::string newestPath;   // Full SD path of the newest downloaded book
  std::string newestTitle;  // Title shown on the summary screen
  uint32_t doneTime = 0;    // millis() when the summary screen was entered
  bool openNewestOnExit = false;

  static constexpr uint32_t CONNECT_TIMEOUT_MS = 12000;
  // Dwell on the "N new items" summary before opening the newest book, so the
  // count is actually readable on a 1-2s e-ink refresh. Back/Confirm override.
  static constexpr uint32_t AUTO_OPEN_DELAY_MS = 4000;

  bool loadConfig();
  void buildCandidateList();
  void startNextCandidate();
  void checkConnectionStatus();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void fetchManifest();
  void downloadNextItem();
  uint32_t loadCursor() const;
  void saveCursor(uint32_t value);
  void openNewestBook();
  void failWith(const char* message);

  bool preventAutoSleep() override { return true; }
};
