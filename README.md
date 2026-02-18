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

## Additional Features

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
