# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **"ztrawhcs"** fork of the open-source [Crosspoint e-reader firmware](https://github.com/crosspoint-reader/crosspoint-reader), targeting the Xteink X4 ESP32-C3 e-ink device. It adds custom features on top of upstream while tracking upstream releases for merging.

- **This repo:** https://github.com/ztrawhcs/crosspoint-enhanced-reading-mod
- **Upstream repo:** https://github.com/crosspoint-reader/crosspoint-reader
- **Current version:** v2.0.0

## Build Commands

```bash
# Development build
pio run

# Flash to device over USB
pio run --target upload

# Monitor serial output
pio device monitor

# Release build
pio run -e gh_release

# Format all C++ source files (required before committing — enforced in CI)
bin/clang-format-fix

# Format only git-modified files
bin/clang-format-fix -g

# Static analysis
pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high

# Run hyphenation tests
test/run_hyphenation_eval.sh
```

## Architecture

### Activity-Based State Machine

Each screen is an **Activity** with `onEnter()`, `loop()`, `onExit()` lifecycle methods, plus a `render(Activity::RenderLock&&)` override (added in upstream's 1.1.0-rc rendering refactor). Activities are self-contained and transition to others via callbacks. All activities live in `src/activities/`.

The base class handles the render thread. Button/input logic stays in `loop()`. Signal redraws via the base class mechanism (e.g., `requestRender()` or equivalent — check the base `Activity` class).

Key activities:
- `EpubReaderActivity` — core EPUB rendering; contains most custom features
- `EpubReaderMenuActivity` — in-reader menu; extended with custom menu items
- `HomeActivity`, `MyLibraryActivity` — navigation/file browser
- `CrossPointWebServerActivity` — built-in HTTP file upload server

### Settings Singleton

`CrossPointSettings` (`src/CrossPointSettings.h/cpp`) is a global `SETTINGS` singleton. Custom settings added by this fork:
- `SETTINGS.buttonModMode` — `MOD_OFF` / `MOD_SIMPLE` / `MOD_FULL`
- `SETTINGS.swapPortraitControls` — bool, swaps nav buttons in portrait
- `SETTINGS.swapLandscapeControls` — bool, swaps nav buttons in landscape
- `SETTINGS.forceBoldText` — uint8_t (0 or 1)

### Hardware Abstraction Layer

`HalDisplay`, `HalGPIO`, `HalStorage` decouple from ESP32-C3 specifics. Button mapping goes through `MappedInputManager` (`src/input/`), which translates physical buttons to logical actions.

### Libraries (`lib/`)

- `GfxRenderer` — vector graphics and text layout primitives
- `EpdFont` — font rasterization; `EpdFontFamily::globalForceBold` static controls force-bold rendering
- `Epub` — EPUB parsing, CSS rendering, chapter navigation
- `I18n` — i18n string IDs (`I18nKeys.h`) and translation YAMLs; added by upstream 1.1.0-rc

## Custom Features

All of these must be preserved through upstream merges.

### Boot Screen Branding (`src/activities/boot/BootActivity.cpp`)
```cpp
renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, "Crosspoint: Enhanced Reading Mod", true, EpdFontFamily::BOLD);
renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, "ztrawhcs version 1.2.0");
```

### Button Mod System
Three modes in `SETTINGS.buttonModMode`. In `MOD_FULL`:
- Long press nav → skip chapter
- Double-click format → toggle bold (shows "Bold: ON" / "Bold: OFF" popup)
- Hold format → rotate orientation
- Single-click format up/down → font size change

Menu item `BUTTON_MOD_SETTINGS` cycles Off → Simple → Full.

### Portrait / Landscape Controls Swap
`SETTINGS.swapPortraitControls` and `SETTINGS.swapLandscapeControls` swap physical nav buttons per orientation. Help overlay updates to reflect swap state. CW and CCW landscape orientations are handled separately.

Menu items: `SWAP_CONTROLS` ("Portrait Controls"), `SWAP_LANDSCAPE_CONTROLS` ("Landscape Controls").

### Night Mode
`isNightMode` bool member on `EpubReaderActivity`. Calls `renderer.invertScreen()` after rendering each page.

### Force Bold Text
```cpp
EpdFontFamily::globalForceBold = (SETTINGS.forceBoldText == 1);
page->render(renderer, SETTINGS.getReaderFontId(), marginLeft, marginTop);
EpdFontFamily::globalForceBold = false;  // restore immediately — without this, UI also renders bold
```

### Estimated Print Page Count
```cpp
constexpr size_t BYTES_PER_PAGE = 2675;  // calibrated: 1,843,269 bytes / 689 pages
```
- `totalBookBytes` (size_t) — set from `epub->getBookSize()`
- `bookProgressExact` (float, 0.0–100.0) — finer-grained than int percent
- Reader menu shows: `"Print Page: X of Y"` where Y = `totalBookBytes / BYTES_PER_PAGE`
- `EpubReaderMenuActivity` constructor takes `bookProgressExact` and `totalBookBytes` as extra params beyond upstream signature
- Status bar displays `bookProgressExact` for smoother progress display

## What Is Explicitly Dropped

Do not port these — they were developed and abandoned:

- Word index anchor system (`wordIndexAnchor`, `hasWordIndexAnchor` in EpubReaderActivity)
- `getWordIndexForPage()` / `findPageForWordIndex()` in Section
- `paragraphStartWordIndex` / `currentPageWordIndex` in ChapterHtmlSlimParser
- Section file version 14 / expanded `HEADER_SIZE`
- `TextBlock::getWordCount()`

Font size changes use the ratio fallback in the base code.

## Merging Upstream

When merging upstream releases, the conflict-prone files are:

1. `src/activities/reader/EpubReaderActivity.cpp` — hardest; contains all custom feature logic
2. `src/activities/reader/EpubReaderActivity.h` — custom members (`totalBookBytes`, `bookProgressExact`, `isNightMode`)
3. `src/activities/reader/EpubReaderMenuActivity.h/.cpp` — custom menu items + extra constructor params
4. `lib/Epub/Epub/blocks/TextBlock.h` — take upstream as-is

**Strategy:** take upstream as the base for conflicted files, then layer custom features back on top. Take upstream wholesale for: i18n files, HomeActivity, MyLibraryActivity, BmpViewerActivity, themes, platformio.ini, docs, and anything not in the conflict list.

### Threading Model (upstream 1.1.0-rc refactor)

The old per-activity FreeRTOS threading model was replaced:

**Removed:** `displayTaskHandle`, `renderingMutex`, `updateRequired`, `taskTrampoline()`, `displayTaskLoop()`, `renderScreen()`

**Added:** `render(Activity::RenderLock&& lock)` override — base class handles the render thread

Button/page logic stays in `loop()`. Move `renderScreen()` content into `render()`.

### i18n for Custom Menu Items

Check `lib/I18n/I18nKeys.h` for existing `StrId` values. For custom items (Button Mods, Portrait Controls, Landscape Controls), either add new `StrId` entries + English strings in `lib/I18n/translations/english.yaml`, or verify whether the menu renderer supports plain string labels alongside `StrId`.

## CI/CD

GitHub Actions runs on every push/PR:
1. **clang-format** check (clang-format-21)
2. **cppcheck** static analysis
3. **Full firmware build** with RAM/Flash stats

Release artifacts (bootloader, firmware ELF/BIN, map files) are generated by tagging a git commit.
