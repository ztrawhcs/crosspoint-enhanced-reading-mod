#include "BlePageTurnerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ble/BlePageTurner.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BlePageTurnerActivity::onEnter() {
  Activity::onEnter();
  scanning = false;
  selectedIndex = 0;
  deviceMacs.clear();
  deviceNames.clear();
  requestUpdate();
}

void BlePageTurnerActivity::onExit() {
  if (scanning) {
    BLE_PAGE_TURNER.stopScan();
    scanning = false;
  }
  Activity::onExit();
}

void BlePageTurnerActivity::loop() {
  // Check if scan just finished
  if (scanning && !BLE_PAGE_TURNER.isScanning()) {
    scanning = false;
    deviceMacs = BLE_PAGE_TURNER.getScanMacs();
    deviceNames = BLE_PAGE_TURNER.getScanNames();
    selectedIndex = 0;
    requestUpdate();
    return;
  }

  // Refresh display periodically while scanning so new devices appear
  if (scanning) {
    const size_t currentCount = BLE_PAGE_TURNER.getScanMacs().size();
    if (currentCount != deviceMacs.size()) {
      deviceMacs = BLE_PAGE_TURNER.getScanMacs();
      deviceNames = BLE_PAGE_TURNER.getScanNames();
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (deviceMacs.empty()) {
      // No devices — trigger a scan
      deviceMacs.clear();
      deviceNames.clear();
      scanning = true;
      BLE_PAGE_TURNER.startScan(SCAN_DURATION_SECS);
      requestUpdate();
    } else {
      // Select the highlighted device
      const std::string& mac = deviceMacs[selectedIndex];
      strncpy(SETTINGS.blePageTurnerMac, mac.c_str(), sizeof(SETTINGS.blePageTurnerMac) - 1);
      SETTINGS.blePageTurnerMac[sizeof(SETTINGS.blePageTurnerMac) - 1] = '\0';
      SETTINGS.saveToFile();
      BLE_PAGE_TURNER.setTargetMac(mac);
      onBack();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (!deviceMacs.empty()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : static_cast<int>(deviceMacs.size()) - 1;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      selectedIndex = (selectedIndex + 1) % static_cast<int>(deviceMacs.size());
      requestUpdate();
    }
  }

  // Allow re-scan when viewing results
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    deviceMacs.clear();
    deviceNames.clear();
    scanning = true;
    BLE_PAGE_TURNER.startScan(SCAN_DURATION_SECS);
    requestUpdate();
  }
}

void BlePageTurnerActivity::render(Activity::RenderLock&&) {
  renderer.clearScreen();
  const int pageWidth = renderer.getScreenWidth();

  // Title
  renderer.drawCenteredText(UI_12_FONT_ID, 15, tr(STR_BLE_PAGE_TURNER), true, EpdFontFamily::BOLD);

  // Connection status
  const char* statusStr = BLE_PAGE_TURNER.isConnected() ? tr(STR_BLE_CONNECTED) : tr(STR_BLE_DISCONNECTED);
  const bool hasMac = SETTINGS.blePageTurnerMac[0] != '\0';
  if (!hasMac) {
    statusStr = tr(STR_BLE_NOT_CONFIGURED);
  }
  renderer.drawCenteredText(UI_10_FONT_ID, 45, statusStr);

  // Separator
  renderer.drawLine(10, 65, pageWidth - 10, 65);

  if (scanning) {
    renderer.drawCenteredText(UI_10_FONT_ID, 90, tr(STR_BLE_SCANNING));
    // Show devices found so far
    constexpr int startY = 115;
    constexpr int lineH = 28;
    for (size_t i = 0; i < deviceNames.size(); i++) {
      renderer.drawText(UI_10_FONT_ID, 20, startY + static_cast<int>(i) * lineH, deviceNames[i].c_str());
    }
  } else if (deviceMacs.empty()) {
    // Prompt to scan
    renderer.drawCenteredText(UI_10_FONT_ID, 90, tr(STR_BLE_SCAN_FOR_DEVICES));
    renderer.drawCenteredText(SMALL_FONT_ID, 115, "Press OK to scan");
  } else {
    // Show scan results list
    constexpr int startY = 75;
    constexpr int lineH = 28;
    for (size_t i = 0; i < deviceMacs.size(); i++) {
      const int y = startY + static_cast<int>(i) * lineH;
      const bool selected = (static_cast<int>(i) == selectedIndex);
      if (selected) {
        renderer.fillRect(0, y, pageWidth - 1, lineH, true);
      }
      renderer.drawText(UI_10_FONT_ID, 20, y, deviceNames[i].c_str(), !selected);
    }
  }

  // Footer hints
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), scanning ? "" : tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
