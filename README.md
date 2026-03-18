# CrossPoint: Enhanced Reading Mod

A custom firmware modification for the **Xteink X4** e-paper reader, built on top of the [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) open-source firmware.

![](./docs/images/cover.jpg)

## What This Mod Does

In the stock CrossPoint firmware, adjusting how your book looks — changing text size, altering line spacing, or tweaking paragraph alignment — requires breaking your reading flow to dive into menus and settings pages. Some features aren't even accessible at all.

This modification overhauls how the physical buttons function. By switching to **Full Mod** mode, your device's hardware buttons allow you to immediately apply formatting without ever opening a menu.

With this mod, you can instantly:

- **Single click** — Increase or decrease text size
- **Hold** — Cycle through line spacing (Tight, Normal, Wide)
- **Double-click left** — Toggle Left or Justified paragraph alignment
- **Double-click right** — Toggle Bold text on or off
- **Hold right** — Rotate screen orientation
- **Double-click back** — Toggle Dark / Light Mode

If you ever need a reminder of the controls, just **hold the menu button** for an on-screen guide.

> **Simple Mode:** If you'd prefer a pared-down experience, a menu option lets you switch to Simple mode — quick access to text size only, with no risk of accidental formatting changes.

## Sentence Highlighting *(v2.5.0)*

You can highlight individual sentences and save them to your device's SD card. Highlights are persistent — they reappear every time you open the book, and they work across page boundaries in both light and dark mode.

### How It Works

Highlighting is a three-step process: **enter cursor mode → select a sentence → save it.**

**Step 1 — Enter Cursor Mode**
- **Double-tap the Power button** while reading. A dark border appears around the page and a highlight bar appears on the first line. You're now in Cursor mode.

**Step 2 — Select a Sentence**
- Use **Up/Down** to move the cursor bar to the line containing the sentence you want.
- **Single-tap Power** to select the full sentence at the cursor. The firmware automatically detects sentence boundaries (periods, exclamation marks, question marks) and highlights from the start to the end of the sentence, even if it spans multiple lines.

**Step 3 — Navigate and Save**
- **Single-tap Power** again to advance to the **next sentence**. Each tap moves the highlight forward by exactly one sentence — useful for reading through a passage and deciding what to save.
- **Down** extends the selection to include one more sentence (additive).
- **Up** shrinks the selection by one line.
- Use the **left rocker** (Back/Confirm) to fine-adjust the start position word by word.
- Use the **right rocker** (Left/Right) to fine-adjust the end position word by word.
- **Double-tap Power** to **save** the highlight. It's written to a text file on the SD card at `/highlights/`.

**Canceling and Deleting**
- **Long-press Back** at any point to cancel and return to normal reading.
- To **delete** a saved highlight: enter Cursor mode, move the cursor onto a highlighted line, and **double-tap Power**. You'll see "Highlight Removed."

**Cheat Sheet:** Hold the Menu button for an on-screen guide showing all highlight controls.

### Highlight Output

Saved highlights are stored as human-readable text files on the SD card in the `/highlights/` directory, one file per book. Each highlight entry shows the chapter name, page number, book progress percentage, and the highlighted text.

## Additional Features

- **Gray background fix** — The stock firmware's first screen refresh after power-on uses a waveform that under-drives the e-ink particles, leaving a faint gray tint across the display until the next full refresh clears it. This mod forces a full waveform on the boot screen so every pixel settles cleanly to white from the start.
- **Portrait button swap** — Choose whether the front or side buttons handle page turns when reading in portrait
- **Landscape button swap** — The same option for landscape orientation
- **Hardware Bold toggle** — Load native bold font files for better readability in changing lighting conditions
- **Anti-aliased text rendering** — Sharp, smooth text with corrected grayscale rendering

## Installing

### Recommended (Web Flasher)

1. Connect your Xteink X4 to your computer via USB-C
2. Download the `firmware.bin` from the [latest release](https://github.com/ztrawhcs/crosspoint-enhanced-reading-mod/releases)
3. Go to https://xteink.dve.al/ and flash the file using the **OTA fast flash controls** section

### Reverting to Stock

Flash the official firmware from https://xteink.dve.al/, or use the **Swap boot partition** button at https://xteink.dve.al/debug.

## Important Notes

- **Official OTA updates will overwrite this mod.** If CrossPoint releases an official update and you apply it, you'll need to re-flash this firmware.
- **Please don't open issues on the upstream CrossPoint repo** if you encounter a bug with this mod. Open an issue here instead.

## Building From Source

This project uses PlatformIO. Clone the repo and flash with:

```sh
git clone --recursive https://github.com/ztrawhcs/crosspoint-enhanced-reading-mod
pio run --target upload
```

## Credits

Built on top of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) by the CrossPoint community, which itself drew inspiration from [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) by atomic14.

---

*This project is not affiliated with Xteink or any manufacturer of the X4 hardware.*
