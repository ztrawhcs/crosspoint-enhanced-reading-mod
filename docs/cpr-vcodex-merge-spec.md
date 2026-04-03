# cpr-vcodex Merge Spec
## Design Agent Output — Implementation Ready

**Source:** https://github.com/franssjz/cpr-vcodex (MIT License, Copyright 2025 Dave Allie)  
**Target:** `~/crosspoint-enhanced-reading-mod`  
**Date:** 2026-04-03  
**Source commit inspected:** HEAD of main at clone time

---

## Critical Architecture Delta (Read First)

Matt's fork and cpr-vcodex have diverged significantly at the activity management layer:

| Concern | cpr-vcodex | Matt's fork |
|---|---|---|
| Activity lifecycle | `ActivityManager` singleton, stack-based, `startActivityForResult()`, `activityManager.goToXxx()` | Flat `currentActivity` pointer, `exitActivity()`/`enterNewActivity()` callbacks via `onGoHome` etc. |
| Render loop | Dedicated FreeRTOS render task per activity, `RenderLock` RAII via semaphore, `requestUpdate(bool immediate)` | Same per-activity FreeRTOS render task, same `RenderLock` mutex pattern |
| Activity base class | `Activity` has `ActivityManager`-based `finish()`, `startActivityForResult()`, `setResult()`, `onGoHome()`, `onSelectBook()` convenience shims | `Activity` has no such methods; navigation is all via callbacks stored in each activity |
| Settings serialization | JSON via `JsonSettingsIO` (lib-level, SDK-provided) | Binary via `Serialization.h` + `writeSettings()`/`readAndValidate()` |
| Settings fields | Superset: adds ~30 new fields for stats, shortcuts, achievements, sleep folder, daily goal, timezone, date format, etc. | Subset |
| `CrossPointState` | Adds `lastKnownValidTimestamp` field | Missing |
| `FontIds.h` | Adds `BOOKERLY_10`, `NOTOSANS_10`, `OPENDYSLEXIC_7` | Missing |
| `FONT_SIZE` enum | 5 values: `X_SMALL=0, SMALL=1, MEDIUM=2, LARGE=3, EXTRA_LARGE=4` | 4 values: `SMALL=0, MEDIUM=1, LARGE=2, EXTRA_LARGE=3` |
| `UI_THEME` enum | 4 values (adds `LYRA_CUSTOM=3`) | 3 values |
| `main.cpp` | Uses `ActivityManager`, `FontCacheManager`, `ScreenshotUtil`, `HalSystem`, `READING_STATS.loadFromFile()`, `ACHIEVEMENTS.loadFromFile()` | Flat callbacks, no `ActivityManager`, no stats/achievements loading |

**Consequence for implementation:** Every cpr-vcodex activity that calls `startActivityForResult()`, `activityManager.goTo*()`, `finish()`, or `activityManager.replaceActivity()` needs to be adapted to Matt's callback/pointer pattern. Matt's `Activity` class does NOT have these methods.

---

## Attribution

All copied source code must include in a comment block at the top of each new file:

```
// Adapted from cpr-vcodex by franssjz
// Originally developed by Dave Allie (Copyright 2025, MIT License)
// Source: https://github.com/franssjz/cpr-vcodex
```

The MIT license requires the copyright notice be preserved in copies. Add the cpr-vcodex copyright block to `LICENSE` or a `THIRD_PARTY_NOTICES` file in the repo root.

---

## Feature Specs (in merge order)

---

### Feature 1 — Reading Stats
**Branch:** `feature/reading-stats`

#### Source files in cpr-vcodex
- `src/ReadingStatsStore.h` / `.cpp` — singleton store, session tracking, SD persistence
- `src/JsonSettingsIO.h` / `src/JsonSettingsIO.cpp` — JSON serialization for stats
- `src/util/ReadingStatsAnalytics.h` / `.cpp` — analytics helpers
- `src/util/TimeUtils.h` / `.cpp` — NTP + RTC time utilities (needed by stats)
- `src/util/TimeZoneRegistry.h` / `.cpp` — timezone presets
- `src/util/HeaderDateUtils.h` / `.cpp` — date display in headers
- `src/activities/apps/ReadingStatsActivity.h` / `.cpp` — overview screen
- `src/activities/apps/ReadingStatsDetailActivity.h` / `.cpp` — per-book detail
- `src/activities/apps/ReadingStatsExtendedActivity.h` / `.cpp` — extended/export view
- `src/util/AchievementPopupUtils.h` — inline helper for achievement popups (needed even without achievements; referenced from stats exit)
- `src/FirmwareVersion.h` — adds `CROSSPOINT_VERSION_DISPLAY` macro

#### Files to CREATE in Matt's fork
```
src/ReadingStatsStore.h
src/ReadingStatsStore.cpp
src/JsonSettingsIO.h
src/JsonSettingsIO.cpp
src/util/ReadingStatsAnalytics.h
src/util/ReadingStatsAnalytics.cpp
src/util/TimeUtils.h
src/util/TimeUtils.cpp
src/util/TimeZoneRegistry.h
src/util/TimeZoneRegistry.cpp
src/util/HeaderDateUtils.h
src/util/HeaderDateUtils.cpp
src/util/AchievementPopupUtils.h
src/FirmwareVersion.h
src/activities/apps/ReadingStatsActivity.h
src/activities/apps/ReadingStatsActivity.cpp
src/activities/apps/ReadingStatsDetailActivity.h
src/activities/apps/ReadingStatsDetailActivity.cpp
src/activities/apps/ReadingStatsExtendedActivity.h
src/activities/apps/ReadingStatsExtendedActivity.cpp
```

Note: `src/activities/apps/` directory does not yet exist in Matt's fork.

#### Files to MODIFY in Matt's fork

**`src/CrossPointSettings.h`** — add new fields and enums:
- Add `SHORTCUT_LOCATION` enum (`SHORTCUT_HOME=0, SHORTCUT_APPS=1`)
- Add `DAILY_GOAL_TARGET` enum (15/30/45/60 min)
- Add `DATE_FORMAT` enum
- Add `SLEEP_IMAGE_ORDER` enum
- Add `LYRA_CUSTOM=3` to `UI_THEME` enum
- Change `FONT_SIZE` enum to `X_SMALL=0, SMALL=1, MEDIUM=2, LARGE=3, EXTRA_LARGE=4` (see Feature 6 for details)
- Add new settings fields: `displayDay`, `autoSyncDay`, `timeZonePreset`, `dateFormat`, `dailyGoalTarget`, `showStatsAfterReading`, `achievementsEnabled`, `achievementPopups`, all `*Shortcut`, `*ShortcutOrder`, `*ShortcutVisible` fields, `sleepDirectory[128]`, `sleepImageOrder`
- Add `getDailyGoalMs()` method declaration
- Remove `BUTTON_MOD_MODE` enum and `buttonModMode`/`forceBoldText`/`swapPortraitControls`/`swapLandscapeControls`/`highlightModeEnabled` fields ONLY if they are not used elsewhere in Matt's fork — **AUDIT BEFORE REMOVING**. These are Matt's unique additions. They must be kept.
- Add status bar enum fields: `statusBarChapterPageCount`, `statusBarBookProgressPercentage`, `statusBarProgressBar`, `statusBarProgressBarThickness`, `statusBarTitle`, `statusBarBattery` (or keep Matt's simpler `statusBar`)

**`src/CrossPointState.h`** — add `lastKnownValidTimestamp`:
```cpp
uint32_t lastKnownValidTimestamp = 0;
```
Also update `CrossPointState.cpp` to save/load this field, and change `saveToFile() const` in cpr-vcodex vs Matt's `saveToFile()` — reconcile const-ness.

**`src/CrossPointSettings.cpp`** — add `getDailyGoalMs()` implementation; update `getReaderFontId()` to handle `X_SMALL` case (see Feature 6).

**`src/SettingsList.h`** — add new settings entries for daily goal, display day, auto sync day, date format, show stats after reading. Note: cpr-vcodex returns `const std::vector<SettingInfo>&` (static); Matt's returns `std::vector<SettingInfo>` (copy). Reconcile to static const.

**`src/activities/settings/SettingsActivity.cpp`** — add an `STR_APPS` category to `categoryNames` array (currently 4 categories; add 5th). Also add shortcut settings actions.

**`src/activities/settings/SettingsActivity.h`** — update `categoryCount` constant from 4 to 5.

**`src/fontIds.h`** — add three missing font IDs:
```cpp
#define BOOKERLY_10_FONT_ID (-464589115)
#define NOTOSANS_10_FONT_ID (1197636217)
#define OPENDYSLEXIC_7_FONT_ID (1586393943)
```

**`src/main.cpp`** — add:
- `#include "ReadingStatsStore.h"` 
- `READING_STATS.loadFromFile();` in `setup()` after `APP_STATE.loadFromFile()`
- X_SMALL font registration: `renderer.insertFont(BOOKERLY_10_FONT_ID, bookerly10FontFamily)` etc.
- Font family objects for bookerly10, notosans10, opendyslexic7 (see Feature 6)
- Note: Matt's fork does NOT have `ActivityManager` or `FontCacheManager`. Matt uses `renderer.setFontDecompressor()` not `renderer.setFontCacheManager()`. Do not add `FontCacheManager` — it is a cpr-vcodex SDK addition not present in Matt's lib.

**`src/activities/reader/EpubReaderActivity.cpp`** (CONFLICT RISK — HIGH):
- Add `#include "ReadingStatsStore.h"`, `#include "util/AchievementPopupUtils.h"`
- In `onEnter()`: add `READING_STATS.beginSession(...)` call after `RECENT_BOOKS.addBook(...)`. cpr-vcodex does this identically at line ~178.
- In `onExit()`: add `READING_STATS.endSession()` before section.reset() — no conflict with highlight state.
- In `loop()`: add `READING_STATS.tickActiveSession()` at top of loop — no conflict.
- Add `READING_STATS.noteActivity()` when entering reader menu (confirm press).
- Add `READING_STATS.resumeSession()` in the reader menu result callback.
- **Highlight mode conflict zone:** The "go home" and "go back" paths in Matt's loop differ from cpr-vcodex (Matt uses `onGoBack`/`onGoHome` callbacks; cpr-vcodex uses `activityManager`). The `exitReaderToHomeOrStats()` helper in cpr-vcodex calls `activityManager.replaceActivity()` or `activityManager.goHome()`. In Matt's fork this must use the `onGoBack`/`onGoHome` callbacks. The `showStatsAfterReading` branch needs to navigate to `ReadingStatsDetailActivity` — in Matt's fork this must use a callback mechanism rather than `activityManager.replaceActivity()`. Simplest approach: add an `onGoToStatsDetail(const std::string& bookPath)` callback parameter to `EpubReaderActivity` constructor, mirroring the existing `onGoBack`/`onGoHome` pattern.
- **Do NOT touch** `HighlightState`, `highlightState`, `previousSpineIndex`, `highlightCachedPage`, `pendingHighlightCleanup`, `highlightDoubleTapMs`, `highlightLongPressMs` or any code between `// --- HIGHLIGHT MODE ---` comment markers.

#### Activity navigation adaptation (applies to all stats activities)
All stats activities in cpr-vcodex call `startActivityForResult()` and `finish()`. Matt's fork does not have these. Adaptation pattern:
- `finish()` → call the `onBack` callback passed in constructor (or navigate via callback)
- `startActivityForResult(activity, callback)` → Matt's `Activity` does have this in cpr-vcodex but NOT in Matt's fork. Must implement via: store sub-activity pointer, manage lifecycle manually, OR add `startActivityForResult()` to Matt's `Activity` base (preferred — implement cpr-vcodex's `ActivityWithSubactivity` pattern).

**Recommended:** Before implementing any feature, port `ActivityWithSubactivity.h/.cpp` from cpr-vcodex into Matt's fork as a prerequisite. This gives all activities a clean `startActivityForResult()` mechanism without disrupting the flat callback pattern in `main.cpp`.

#### SD card file paths
- Stats JSON: `/.crosspoint/reading_stats.json`
- Stats exports: `/exports/reading_stats_YYYYMMDD_HHMMSS.json` (or `/exports/reading_stats_export.json` if no clock)

#### New PlatformIO library dependencies
- `bitbank2/JPEGDEC @ ^1.8.0` — add to `platformio.ini` (cpr-vcodex uses this for JPEG in Epub lib; Matt's fork doesn't have it, but the Epub lib in cpr-vcodex's lib directory does use it)
- `ArduinoJson 7.4.2` — already present in Matt's fork

**IMPORTANT:** `JsonSettingsIO` is an SDK-level library in cpr-vcodex (provided inside `lib/` or the open-x4-sdk). Matt does NOT have this. The stats store's JSON serialization must be brought in from `src/JsonSettingsIO.cpp` in cpr-vcodex (it is not part of the open-x4-sdk in the cloned repo — it lives in `src/`). Verify by checking `/tmp/cpr-vcodex/src/JsonSettingsIO.cpp`.

---

### Feature 2 — Sync Day (NTP Wi-Fi Clock Sync)
**Branch:** `feature/sync-day`  
**Depends on:** Feature 1 (uses `TimeUtils`, `HeaderDateUtils`, `CrossPointState::lastKnownValidTimestamp`)

#### Source files in cpr-vcodex
- `src/activities/apps/SyncDayActivity.h` / `.cpp`
- `src/activities/apps/SyncDayActivity.h` references `HeaderDateUtils`, `TimeUtils`, `WifiSelectionActivity`

#### Files to CREATE in Matt's fork
```
src/activities/apps/SyncDayActivity.h
src/activities/apps/SyncDayActivity.cpp
```

#### Files to MODIFY in Matt's fork

**`src/activities/home/HomeActivity.cpp`** — add a "Sync Day" entry to home shortcuts. In cpr-vcodex this flows through `ShortcutRegistry`. In Matt's fork there is no `ShortcutRegistry`. Options:
  1. Hard-code a Sync Day button directly (simpler for this feature in isolation)
  2. Port `ShortcutRegistry` as part of Feature 5 and defer this wiring to that branch

Recommended: hard-code for now, refactor in Feature 5.

**Navigation adaptation:** `SyncDayActivity::openWifiSelection()` calls `startActivityForResult(WifiSelectionActivity...)`. Adapt using `ActivityWithSubactivity` pattern (ported in Feature 1 prerequisite). `SyncDayActivity::onExit()` disconnects WiFi if it was connected within the activity — no changes needed there. `SyncDayActivity` calls `finish()` on Back — needs callback.

**`src/main.cpp`** — add `TimeUtils::configureTimezone()` call after `SETTINGS.loadFromFile()`.

#### Settings fields needed (added in Feature 1)
`displayDay`, `autoSyncDay`, `timeZonePreset`, `dateFormat` — all already added in Feature 1.

#### No new PlatformIO library dependencies

---

### Feature 3 — Reading Heatmap + Reading Day
**Branch:** `feature/reading-heatmap`  
**Depends on:** Features 1 and 2

#### Source files in cpr-vcodex
- `src/activities/apps/ReadingHeatmapActivity.h` / `.cpp`
- `src/activities/apps/ReadingDayDetailActivity.h` / `.cpp`

#### Files to CREATE in Matt's fork
```
src/activities/apps/ReadingHeatmapActivity.h
src/activities/apps/ReadingHeatmapActivity.cpp
src/activities/apps/ReadingDayDetailActivity.h
src/activities/apps/ReadingDayDetailActivity.cpp
```

#### Files to MODIFY in Matt's fork
- Add heatmap entry to home/apps navigation (same pattern as Feature 2).
- Navigation: `ReadingHeatmapActivity` uses `startActivityForResult(ReadingDayDetailActivity...)`. Adapt to `ActivityWithSubactivity`.

#### No new library dependencies

---

### Feature 4 — Achievements System
**Branch:** `feature/achievements`  
**Depends on:** Features 1, 2, 3

#### Source files in cpr-vcodex
- `src/AchievementsStore.h` / `src/AchievementsStore.cpp`
- `src/activities/apps/AchievementsActivity.h` / `.cpp`
- `src/util/AchievementPopupUtils.h` (already brought in Feature 1)
- `src/JsonSettingsIO.h` — adds `saveAchievements`/`loadAchievements` declarations (already in Feature 1's JsonSettingsIO)
- `src/JsonSettingsIO.cpp` — adds corresponding implementations

Note: `AchievementsStore` has a `totalBookmarksAdded` counter and `recordBookmarkAdded()` method. Since bookmarks are excluded from the merge, stub this out: `void recordBookmarkAdded() {}` no-op. The `AchievementId` enum includes `FirstBookmark`, `TenBookmarks`, `TwentyFiveBookmarks`, `FiftyBookmarks` — these will simply never unlock (which is correct since Matt's highlight system replaces bookmarks).

#### Files to CREATE in Matt's fork
```
src/AchievementsStore.h
src/AchievementsStore.cpp
src/activities/apps/AchievementsActivity.h
src/activities/apps/AchievementsActivity.cpp
```

#### Files to MODIFY in Matt's fork

**`src/main.cpp`** — add:
```cpp
#include "AchievementsStore.h"
// in setup():
ACHIEVEMENTS.loadFromFile();
```

**`src/activities/reader/EpubReaderActivity.cpp`** — add achievement hooks (CONFLICT RISK — MEDIUM):
- `#include "AchievementsStore.h"`
- In exit/go-home paths: `ACHIEVEMENTS.recordSessionEnded(READING_STATS.getLastSessionSnapshot())`
- Call `showPendingAchievementPopups(renderer)` in exit paths
- **Do NOT call** `ACHIEVEMENTS.recordBookmarkAdded()` — bookmarks excluded
- **Highlight mode safety:** Only add achievement calls in paths that don't touch highlight state machine. The exit paths (`onExit()`, Back long-press) are safe. The confirm press (menu entry) is safe — add `READING_STATS.noteActivity()` before opening menu, and `READINGS_STATS.resumeSession()` in the result callback.

#### SD card file path
- Achievements JSON: `/.crosspoint/achievements.json`

#### Settings fields needed (added in Feature 1)
`achievementsEnabled`, `achievementPopups` — already added.

---

### Feature 5 — Home/Apps Shortcut Configurability
**Branch:** `feature/shortcut-config`  
**Depends on:** Features 1–4 (uses all shortcut types)

#### Source files in cpr-vcodex
- `src/util/ShortcutRegistry.h` — inline-only header defining `ShortcutId`, `ShortcutDefinition`, `getShortcutDefinitions()`, ordering/visibility helpers
- `src/activities/apps/AppsActivity.h` / `.cpp` — the Apps hub screen
- `src/activities/home/FileBrowserActivity.h` / `.cpp` — replaces Matt's `MyLibraryActivity`
- `src/activities/home/RecentBooksActivity.h` / `.cpp` — (same filename as Matt's but different implementation: adds long-press delete, uses `ConfirmationActivity`)
- `src/activities/util/ConfirmationActivity.h` / `.cpp` — yes/no dialog
- `src/activities/settings/ShortcutLocationActivity.h` / `.cpp`
- `src/activities/settings/ShortcutOrderActivity.h` / `.cpp`
- `src/activities/settings/ShortcutVisibilityActivity.h` / `.cpp`
- `src/activities/settings/StatusBarSettingsActivity.h` / `.cpp`
- `src/activities/settings/TimeZoneSelectActivity.h` / `.cpp`
- `src/activities/home/HomeActivity.h` / `.cpp` — heavily modified from Matt's version

#### Files to CREATE in Matt's fork
```
src/util/ShortcutRegistry.h
src/activities/apps/AppsActivity.h
src/activities/apps/AppsActivity.cpp
src/activities/util/ConfirmationActivity.h
src/activities/util/ConfirmationActivity.cpp
src/activities/settings/ShortcutLocationActivity.h
src/activities/settings/ShortcutLocationActivity.cpp
src/activities/settings/ShortcutOrderActivity.h
src/activities/settings/ShortcutOrderActivity.cpp
src/activities/settings/ShortcutVisibilityActivity.h
src/activities/settings/ShortcutVisibilityActivity.cpp
src/activities/settings/StatusBarSettingsActivity.h
src/activities/settings/StatusBarSettingsActivity.cpp
src/activities/settings/TimeZoneSelectActivity.h
src/activities/settings/TimeZoneSelectActivity.cpp
```

#### Files to MODIFY in Matt's fork

**`src/activities/home/HomeActivity.h` / `.cpp`** — the biggest change in this feature. cpr-vcodex's `HomeActivity` uses `ActivityManager`, `ShortcutRegistry`, `ConfirmationActivity`, `startActivityForResult()`, and reads from `READING_STATS` for subtitles. Matt's current `HomeActivity` uses callbacks (`onGoToReader`, `onGoToMyLibrary`, etc.). Strategy: replace Matt's `HomeActivity` with cpr-vcodex's, but adapt the navigation to use callbacks stored as member function pointers or lambdas passed in constructor. Specifically:
  - Matt's `MyLibraryActivity` → maps to cpr-vcodex's `FileBrowserActivity` (different file path, same concept). Either rename or create `FileBrowserActivity` as a copy and deprecate `MyLibraryActivity`.
  - Matt uses `onGoToMyLibrary`, `onGoToRecentBooks`, `onGoToSettings`, `onGoToFileTransfer`, `onGoToBrowser` callbacks. In cpr-vcodex these are `activityManager.goToXxx()` calls. These must become stored callbacks.

**`src/activities/home/RecentBooksActivity.h` / `.cpp`** — replace with cpr-vcodex version (adds long-press delete via `ConfirmationActivity`). Navigation adaptation: `startActivityForResult(ConfirmationActivity...)` needs `ActivityWithSubactivity`.

**`src/main.cpp`** — update `onGoHome()` to pass new callbacks matching updated `HomeActivity` constructor, and add `onGoToApps()` callback.

**`src/SettingsList.h`** — add shortcut location/order/visibility settings entries for all shortcuts.

**`src/activities/settings/SettingsActivity.cpp`** — wire new settings activities into the `STR_APPS` category.

#### Structural mapping conflict: FileBrowserActivity vs MyLibraryActivity
cpr-vcodex's `FileBrowserActivity` (in `src/activities/home/`) replaces Matt's `MyLibraryActivity`. Matt's `MyLibraryActivity` may have custom functionality. Do NOT delete `MyLibraryActivity` — create `FileBrowserActivity` as a new file and route the "Browse Files" shortcut to it. Keep `MyLibraryActivity` to avoid breaking existing navigation until fully migrated.

---

### Feature 6 — X Small Font Size
**Branch:** `feature/xsmall-font`  
**Depends on:** Feature 1 (to avoid enum value collision; Feature 1 changes the `FONT_SIZE` enum)

#### The enum shift problem
Matt's current `FONT_SIZE` enum: `SMALL=0, MEDIUM=1, LARGE=2, EXTRA_LARGE=3`  
cpr-vcodex's `FONT_SIZE` enum: `X_SMALL=0, SMALL=1, MEDIUM=2, LARGE=3, EXTRA_LARGE=4`

Inserting X_SMALL at position 0 shifts all existing values up by 1. This is a **binary settings migration problem** because Matt stores `fontSize` as a raw `uint8_t` in `/.crosspoint/settings.bin`. Users who have `fontSize=0` (SMALL) will read as X_SMALL after the change; users who had `fontSize=2` (LARGE) will read as MEDIUM.

**Migration approach:**
1. Add a migration function in `CrossPointSettings.cpp` (similar to `migrateLegacyFontSize()` already present in cpr-vcodex):
```cpp
uint8_t migrateLegacyFontSize(uint8_t legacyValue) {
  // Old: 0=SMALL, 1=MEDIUM, 2=LARGE, 3=EXTRA_LARGE
  // New: 0=X_SMALL, 1=SMALL, 2=MEDIUM, 3=LARGE, 4=EXTRA_LARGE
  return legacyValue + 1;
}
```
2. Apply migration when loading from binary file (detect via settings file version bump).

#### Source files in cpr-vcodex
- Font data is in the Epub SDK's `lib/EpdFont/` — Matt already has the `bookerly_10_*`, `notosans_10_*`, `opendyslexic_7_*` font data files (check `/Users/matt/crosspoint-enhanced-reading-mod/lib/EpdFont/`)
- `src/fontIds.h` — adds three IDs
- `src/CrossPointSettings.h` — `FONT_SIZE` enum change
- `src/CrossPointSettings.cpp` — `getReaderFontId()` updated, migration function
- `src/main.cpp` — font family objects and `renderer.insertFont()` calls for new fonts

#### Files to MODIFY in Matt's fork

**`src/fontIds.h`** — add the three IDs (already noted in Feature 1, but do this here on its own branch for atomicity).

**`src/CrossPointSettings.h`** — update `FONT_SIZE` enum.

**`src/CrossPointSettings.cpp`** — add `migrateLegacyFontSize()`, update `getReaderFontId()`.

**`src/main.cpp`** — add font declarations:
```cpp
EpdFont bookerly10RegularFont(&bookerly_10_regular);
// ... bold, italic, bolditalic
EpdFontFamily bookerly10FontFamily(...);
// same for notosans10, opendyslexic7
```
And registration:
```cpp
renderer.insertFont(BOOKERLY_10_FONT_ID, bookerly10FontFamily);
renderer.insertFont(NOTOSANS_10_FONT_ID, notosans10FontFamily);
renderer.insertFont(OPENDYSLEXIC_7_FONT_ID, opendyslexic7FontFamily);
```

**`src/SettingsList.h`** — update `STR_FONT_SIZE` enum options to include `STR_X_SMALL` as first option.

#### Verify font data exists
```bash
ls /Users/matt/crosspoint-enhanced-reading-mod/lib/EpdFont/ | grep -E "bookerly_10|notosans_10|opendyslexic_7"
```
If these don't exist, copy them from `/tmp/cpr-vcodex/lib/EpdFont/` (they are compiled font data, safe to copy).

---

### Feature 7 — "If Found, Please Return Me" Screen
**Branch:** `feature/if-found`  
**Depends on:** Feature 5 (shortcut system), or can be standalone with hard-coded nav

#### Source files in cpr-vcodex
- `src/activities/apps/IfFoundActivity.h` / `.cpp`

Content is read from SD file `/.crosspoint/if_found.txt` or equivalent — verify in `.cpp`.

#### Files to CREATE in Matt's fork
```
src/activities/apps/IfFoundActivity.h
src/activities/apps/IfFoundActivity.cpp
```

#### Files to MODIFY in Matt's fork
- Add to Apps shortcuts navigation (see Feature 5 wiring, or hard-code temporarily).

#### No new library dependencies

---

### Feature 8 — ReadMe On-Device Help Guide
**Branch:** `feature/readme-activity`  
**Depends on:** Feature 5 (shortcut) or standalone

#### Source files in cpr-vcodex
- `src/activities/apps/ReadMeActivity.h` / `.cpp`

Contains 7 topics (SyncDay, Stats, Bookmarks, Sleep, Shortcuts, Achievements, IfFound). The `Bookmarks` topic will describe cpr-vcodex's bookmark system. Since Matt's fork doesn't have bookmarks in the same form, either: remove the Bookmarks topic entry, or replace its body text with a note about highlights.

#### Files to CREATE in Matt's fork
```
src/activities/apps/ReadMeActivity.h
src/activities/apps/ReadMeActivity.cpp
```

---

### Feature 9 — Recent Books Removal (Long-Press)
**Branch:** `feature/recent-books-longpress`  
**Depends on:** Feature 5 (uses `ConfirmationActivity`)

This is already part of Feature 5's `RecentBooksActivity` replacement. If Feature 5 is merged, this is automatically included. If implementing independently, port `ConfirmationActivity` alone first.

**Source files in cpr-vcodex:**
- `src/activities/home/RecentBooksActivity.cpp` — adds long-press confirm/delete via `ConfirmationActivity`
- `src/activities/util/ConfirmationActivity.h` / `.cpp`

Same long-press behavior is also added to `HomeActivity.cpp` for the inline recent books list on the home screen.

---

### Feature 10 — Sleep App Improvements (Folder Picker + Shuffle Mode)
**Branch:** `feature/sleep-app`  
**Depends on:** Feature 5 (shortcut to reach sleep app), but sleep app itself is standalone

#### Source files in cpr-vcodex
- `src/activities/apps/SleepAppActivity.h` / `.cpp` — folder picker + shuffle toggle
- `src/activities/apps/SleepPreviewActivity.h` / `.cpp` — preview sleep image in folder
- `src/util/SleepImageUtils.h` / `.cpp` — lists directories, resolves configured directory
- `src/util/BmpMetadataCache.h` / `.cpp` — BMP metadata cache used by sleep image utilities

#### Files to CREATE in Matt's fork
```
src/activities/apps/SleepAppActivity.h
src/activities/apps/SleepAppActivity.cpp
src/activities/apps/SleepPreviewActivity.h
src/activities/apps/SleepPreviewActivity.cpp
src/util/SleepImageUtils.h
src/util/SleepImageUtils.cpp
src/util/BmpMetadataCache.h
src/util/BmpMetadataCache.cpp
```

#### Files to MODIFY in Matt's fork
- `src/CrossPointSettings.h` — add `sleepDirectory[128]`, `sleepImageOrder` fields, `SLEEP_IMAGE_ORDER` enum (done in Feature 1 above).

**`src/activities/boot_sleep/SleepActivity.cpp`** — integrate `SleepImageUtils::resolveConfiguredSleepDirectory()` to pick images from configured folder and honor shuffle/sequential order. Review Matt's existing `SleepActivity` for any custom logic.

---

### Feature 11 — Sleep Screen Cache
**Branch:** `feature/sleep-cache`  
**Depends on:** Feature 10

#### Source files in cpr-vcodex
- `src/util/SleepScreenCache.h` / `.cpp`

Cache stores rendered BW framebuffers at `/.crosspoint/sleep_cache/<hash>.bin`.

#### Files to CREATE in Matt's fork
```
src/util/SleepScreenCache.h
src/util/SleepScreenCache.cpp
```

#### Files to MODIFY in Matt's fork
- `src/activities/boot_sleep/SleepActivity.cpp` — call `SleepScreenCache::load()` before rendering, `SleepScreenCache::save()` after rendering a new BMP.

---

### Feature 12 — `/ignore_stats/` Folder Exclusion
**Branch:** `feature/ignore-stats`  
**Depends on:** Feature 1 (is part of `ReadingStatsStore`)

This feature is **already implemented inside `ReadingStatsStore.cpp`** in cpr-vcodex:

```cpp
bool isIgnoredStatsPath(const std::string& path) {
  // returns true if path == "/ignore_stats" or starts with "/ignore_stats/"
}
```

`ReadingStatsStore::shouldIgnorePath()` and `removeIgnoredBooks()` use this. Since Feature 1 copies `ReadingStatsStore` wholesale, this feature is automatically included when Feature 1 is merged.

**No additional files needed.** Document in changelog.

---

## Merge Order Summary

```
1. feature/reading-stats          ← largest, foundational; adds ActivityWithSubactivity prerequisite
2. feature/sync-day               ← depends on TimeUtils from Feature 1
3. feature/reading-heatmap        ← depends on stats
4. feature/achievements           ← depends on stats
5. feature/shortcut-config        ← largest UI change; restructures home/apps
6. feature/xsmall-font            ← self-contained but enum touches Feature 1's CrossPointSettings
7. feature/if-found               ← small; needs apps nav from Feature 5
8. feature/readme-activity        ← small; needs apps nav from Feature 5
9. feature/recent-books-longpress ← included in Feature 5; standalone only if 5 is deferred
10. feature/sleep-app             ← needs Feature 5 for nav
11. feature/sleep-cache           ← needs Feature 10
12. feature/ignore-stats          ← free; included in Feature 1
```

Each branch is based off `main`. They must be merged sequentially in the order above because each adds to `CrossPointSettings`, `main.cpp`, and the activities directory that later branches depend on.

---

## Key Structural Mismatches Requiring Implementation Judgment

### 1. ActivityManager vs Callbacks
The most pervasive difference. Recommendation for implementation agent:

**Port `ActivityWithSubactivity` from cpr-vcodex into Matt's fork as an infrastructure step in Feature 1.** It gives a clean `startActivityForResult()` without requiring the full `ActivityManager`. Matt's `main.cpp` navigation callbacks remain unchanged. All new activities inherit from `ActivityWithSubactivity` instead of `Activity`, gaining `finish()` and `startActivityForResult()`.

Matt's `Activity.h` does NOT have `finish()`, `startActivityForResult()`, `setResult()`, or `onGoHome()`. These must all be added or routed through `ActivityWithSubactivity`.

### 2. JSON vs Binary Settings
cpr-vcodex stores all settings as JSON (via `JsonSettingsIO` SDK lib). Matt stores settings as binary. The new fields (stats, shortcuts, etc.) can be added to Matt's binary settings schema — just append them at the end of the settings file with version migration. Do NOT switch Matt's settings serialization to JSON; that would break existing user settings.

The `ReadingStatsStore` and `AchievementsStore` use their own separate JSON files (`reading_stats.json`, `achievements.json`) via their own serialization — these are not part of the main `settings.bin` and require bringing in `JsonSettingsIO.cpp` from cpr-vcodex's `src/` directory.

### 3. FontCacheManager
cpr-vcodex uses `FontCacheManager` in `main.cpp` and `renderer.setFontCacheManager()`. Matt's fork uses `renderer.setFontDecompressor()`. Do NOT add `FontCacheManager` — it is not present in Matt's `lib/GfxRenderer/`. The font decompressor path is Matt's approach.

### 4. HalSystem
cpr-vcodex calls `HalSystem::begin()`, `HalSystem::checkPanic()`, `HalSystem::clearPanic()` in `main.cpp`. Matt's fork doesn't have `HalSystem`. Do not add these calls.

### 5. WebDAVHandler
cpr-vcodex has `WebDAVHandler.h/.cpp` in network; Matt doesn't. Not needed for any feature being merged; skip.

### 6. BookmarksActivity / BookmarkStore
`BookmarksActivity.h/.cpp` and `BookmarkStore.h` exist in cpr-vcodex's reader directory. These are the cpr-vcodex bookmark system. Do NOT copy these — they conflict with Matt's highlight state machine. Matt's `EpubReaderActivity` already has its own `bookmarkStore` member using `BookmarkStore` from `HighlightStore.cpp`. Keep Matt's implementation completely intact.

### 7. scripts/git_branch.py
cpr-vcodex adds `git_branch.py` to scripts (sets `CROSSPOINT_VERSION_CODE` build flag) and `FirmwareVersion.h` uses it. Matt doesn't have this script. Either: copy `git_branch.py` + add to `platformio.ini`'s `extra_scripts`, or simplify `FirmwareVersion.h` to just use `CROSSPOINT_VERSION` without the version code.

---

## Files NOT to copy

| File | Reason |
|---|---|
| `src/activities/reader/BookmarksActivity.*` | Conflicts with highlight state machine |
| `src/activities/reader/BookmarkStore.h` | Conflicts with highlight state machine |
| `src/activities/reader/EpubReaderFootnotesActivity.*` | Matt has different footnote handling; audit before touching |
| `src/activities/reader/QrDisplayActivity.*` | May conflict; audit |
| `src/network/WebDAVHandler.*` | Not needed for any merged feature |
| `src/activities/apps/BookmarksAppActivity.*` | Bookmarks excluded |
| Bookmark-related `AchievementId` entries | Keep enum entries (they won't unlock), but stub out `recordBookmarkAdded()` |

---

## Attribution Comment Template

Add to top of every new `.h` and `.cpp` file created from cpr-vcodex source:

```cpp
// Adapted from cpr-vcodex (https://github.com/franssjz/cpr-vcodex)
// Original work Copyright (c) 2025 Dave Allie. MIT License.
```

Add `THIRD_PARTY_NOTICES.md` or append to existing `LICENSE`:
```
This project incorporates code from cpr-vcodex (https://github.com/franssjz/cpr-vcodex),
Copyright (c) 2025 Dave Allie, licensed under the MIT License.
```
