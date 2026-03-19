# Changelog

## v2.5.4 — Triple-Tap Clear + Cross-Page Navigation Fix

### New Features

- **Triple-tap Power to clear all highlights on current page** — While in highlight mode, triple-tapping the power button deletes all saved highlights that span the current page. Fires immediately on the 3rd tap; each tap must land within 350ms of the previous. Replaces the non-functional Left+Right chord (those buttons share an ADC resistor ladder and cannot be pressed simultaneously). Help overlay updated.

### Bug Fixes

- **Fixed: Down arrow jumps multiple sentences after crossing a page boundary** — When a selection started on the previous page (sentence detected at top of current page), the SELECT mode Up handler was loading `section->currentPage` (the start page, which the reader auto-turned to when entering SELECT mode) instead of `selectionEndPage`. This corrupted `selectionEndCharOffset` with text from the wrong page, causing the next Down press to search from a garbage position and skip multiple sentences. Now explicitly saves/restores `section->currentPage` around the page load in the "normal shrink" branch, matching the pattern already used by the Down handler.
- **Fixed: Triple-tap clear shows popup but highlight remains** — The old chord delete used `findHighlightBounds` (text matching) to filter highlights per page, which can produce false negatives for reflowed text. Replaced with page-number range filtering (`hl.startPage <= currentPage <= hl.endPage`), which is simpler and directly answers whether the highlight overlaps the current page.

---

## v2.5.3 — Highlight Rendering & Delete Fixes

### Bug Fixes

- **Fixed phantom highlights covering 2/3 of page**: The `START` and `END` matching roles were using single-word matching, causing common first/last words ("The", "He", "a") to match on unrelated pages after font-size changes or text reflow. Now uses two-word phrase matching — the first *two* words of the highlight must appear consecutively, preventing false positives.
- **Fixed single-tap delete removing wrong highlight**: The delete check now uses the same FULL→START→MIDDLE→END role chain as rendering, so it only targets highlights actually visible on the current page instead of deleting the first highlight in the chapter list.
- **Fixed chord delete (Left+Right) wiping highlights from other pages**: The chord delete now loads the current page and verifies each highlight is actually visible before deleting it, preventing collateral deletion of valid highlights elsewhere in the chapter.

---

## v2.5.2 — Highlight UX Overhaul

### Improvements

- **Single-tap Power advances by sentence**: In select mode, tapping Power now moves to the next sentence (dropping the previous), instead of extending the selection. This makes it natural to read through a passage sentence-by-sentence and save the one you want.
- **Page render caching**: Cursor moves within the same page now skip full-page text rendering (~100-200ms savings per keypress). The clean page is cached on first render and restored from memory on subsequent cursor moves.
- **Cleaner highlight output**: The highlight file on SD card no longer shows internal reference IDs. Each entry shows chapter, page, progress %, and the highlighted text, separated by blank lines.
- **Updated help overlay**: The on-screen cheat sheet (hold Menu) now documents the sentence-advance and selection-extend controls.

### Bug Fixes

- Fixed: Each Power tap in select mode was accumulating sentences instead of advancing — now correctly highlights one sentence at a time.

---

## v2.5.1 — Faster Highlight Navigation

### Performance

- **Highlight cursor speed**: Line-to-line navigation in highlight mode now uses a 4-frame A2-like BW waveform (`lut_bw_fast`) instead of the OTP FAST_REFRESH waveform (~12 frames). Cuts per-keypress display latency by roughly 3×. Some additional ghosting on unchanged pixels is expected and acceptable during active cursor movement; the next page turn clears it.
- **Anti-aliasing already skipped in highlight mode** — confirmed in code; the BW fast path is used end-to-end for all highlight cursor updates.

---

## v2.5.0 — Sentence Highlighting

This is a major feature release. Sentence highlighting is one of the most-requested features, and it now works reliably across page boundaries.

### New Features

- **Sentence Highlight Mode** — Press Power (short tap) while reading to enter Cursor mode. Navigate to a sentence with Up/Down, then tap Power again to select the full sentence. The selection is highlighted and saved to the SD card.
- **Persistent Highlights** — Saved highlights survive power cycles and appear every time you open that book. Stored per-book in a companion `.highlights` file on the SD card.
- **Cross-Page Highlights** — Sentences that span a page boundary are handled correctly: the reader auto-turns to show the anchor, and the highlight renders across both pages.
- **Dark Mode Support** — Highlights render correctly in both light and dark mode.

### Controls (Highlight Mode)

| Action | Button |
|---|---|
| Enter Cursor mode | Short Power tap while reading |
| Move cursor up/down | Up / Down |
| Select sentence at cursor | Short Power tap |
| Extend selection forward (one sentence) | Down |
| Shrink selection (one line) | Up |
| Fine-adjust start word left/right | Back / Confirm |
| Save highlight | Double-tap Power |
| Delete highlight at cursor | Double-tap Power (in Cursor mode, over saved highlight) |
| Cancel / exit Highlight mode | Long-press Back |

### Bug Fixes

- Fixed: Down arrow was advancing the selection by up to 3 sentences per press — now strictly advances one sentence at a time.
- Fixed: Cross-page backward sentence detection — cursor at the top line of a page now correctly locates the sentence start on the previous page.
- Fixed: Word outline bleed — earlier builds highlighted the full line width; now only the words within the sentence boundaries are inverted.
- Fixed: Page-jump on Down — selection end no longer jumps multiple pages in a single press.
- Fixed: Gray background flash on boot (cosmetic fix, full-waveform refresh on startup).

### Performance

- Anti-aliasing is disabled during active highlight mode for faster rendering and snappier navigation.

---

## v2.0.0 — Enhanced Reading Mod (initial release)

- Full button remapping in **Full Mod** mode: single-click resize, hold for line spacing, double-click for alignment and bold, hold-right for rotation, double-click Back for dark mode
- **Simple Mode** option for text-size-only access
- Portrait and landscape button swap options
- Hardware Bold toggle (loads native bold font files)
- Anti-aliased text rendering with corrected grayscale
- On-screen control guide (hold menu button)
