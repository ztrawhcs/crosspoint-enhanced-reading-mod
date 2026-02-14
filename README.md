# CrossPoint: Enhanced Reading Mod

**A custom firmware modification for the CrossPoint e-reader that puts complete typographic control right at your fingertips.**

In the stock CrossPoint firmware, adjusting how your book looks—changing text size, altering line spacing, or tweaking paragraph alignment—requires breaking your reading flow to dive into menus and settings pages. Some features aren't even quickly accessible at all. 

This modification overhauls how the physical buttons function. By switching to "Full Mod" mode, your device's hardware buttons allow you to immediately apply formatting without ever opening a menu. 

**With this mod, you can instantly:**
* **Increase or decrease text size** (single click)
* **Cycle through line spacing** (Tight, Normal, and Wide) (hold left)
* **Toggle Left or Justified alignment** (double-click left)
* **Turn Bold text on or off** to adapt to changing lighting (double-click right)
* **Rotate screen orientation** (hold right)
* **Toggle Dark / Light Mode** (double-click the back button)

If you ever need a reminder of the controls, just **hold the menu button** for a handy on-screen guide! 

*Note: If you prefer to keep things simple, a menu option allows you to switch to "Simple" mode—a pared-down version that provides quick access to text size while preventing accidental formatting changes.*

---

## v1.0 Release Notes

This update adds a dedicated hardware toggle for bold text and an option to swap the button layout when reading in portrait mode.

### New Features

* **Hardware Bold Toggle:** You can now double-tap the bottom-right button to toggle bold text on or off. This is useful if lighting conditions change or if you're reading a book with a thin default font. The system loads native bold font files when available and applies a custom 1px spacing adjustment to maintain readability.
* **Portrait Controls Swap:** Added a new option in the Reader Menu to swap the physical button layout when reading in Portrait mode. You can now choose the layout that best fits your grip:
  * **Default:** Bottom buttons = Formatting shortcuts / Side buttons = Page Turns
  * **Swapped:** Bottom buttons = Page Turns / Side buttons = Formatting shortcuts

### Tweaks & Fixes
* Updated the Reader Menu to include the Portrait Controls toggle.
* Removed the outdated "Anti-Alias Weight" option, as text thickness is now handled entirely by the new Bold toggle.
* Fixed an issue where UI elements (status bar, help overlays, and menus) would accidentally inherit bold formatting from the reader text.

### Installation
Download the `firmware.bin` file attached below and flash it to your device using your standard flashing tool.

---

## Important Note Before Installing

First and foremost: massive thanks to the original Crosspoint development team. This project is an unofficial, community-driven fork of their incredible work. 

The official Crosspoint firmware is under highly active development, and this repo is just one of many user forks experimenting with new quality-of-life features. 

**My long-term goal is to submit these features as Pull Requests (PRs) to the official project.** However, because the review and integration process takes time, I am releasing this modified firmware now so the community can enjoy these typographic and ergonomic upgrades immediately.

**CRITICAL WARNINGS FOR USERS:**
* **Official Updates Will Erase This:** If you install this mod and later download an official Over-The-Air (OTA) upgrade from the main Crosspoint team, **you will lose all of these features.** Official updates will completely overwrite this custom firmware. 
* **Do Not Bother the Original Devs:** If you encounter a bug, glitch, or crash while using this specific firmware, **please open an issue here on this GitHub page.** Do not submit bug reports to the official Crosspoint team, as they cannot provide support for modified community forks.
