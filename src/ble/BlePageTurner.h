#pragma once
#include <NimBLEDevice.h>

#include <atomic>
#include <string>
#include <vector>

class BlePageTurner : public NimBLEClientCallbacks, public NimBLEAdvertisedDeviceCallbacks {
  std::atomic<bool> _nextPressed{false};
  std::atomic<bool> _prevPressed{false};
  std::atomic<bool> _connected{false};
  std::atomic<bool> _scanning{false};

  std::string _targetMac;
  NimBLEClient* _client = nullptr;

  // Scan results for pairing UI
  std::vector<std::string> _scanMacs;
  std::vector<std::string> _scanNames;

  void onConnect(NimBLEClient* client) override;
  void onDisconnect(NimBLEClient* client) override;
  void onResult(NimBLEAdvertisedDevice* device) override;

  bool connectToTarget();
  void subscribeToHid();
  static void notifyCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t len,
                             bool isNotify);
  static void connectTask(void* param);

 public:
  void begin();
  void setTargetMac(const std::string& mac);
  std::string getTargetMac() const { return _targetMac; }
  bool isConnected() const { return _connected; }

  // Called by MappedInputManager every frame — returns true once per press, clears flag
  bool wasNextPressed() {
    bool val = _nextPressed.load();
    if (val) _nextPressed = false;
    return val;
  }
  bool wasPrevPressed() {
    bool val = _prevPressed.load();
    if (val) _prevPressed = false;
    return val;
  }

  // Scanning for pairing UI
  void startScan(int durationSecs = 8);
  void stopScan();
  bool isScanning() const { return _scanning; }
  const std::vector<std::string>& getScanMacs() const { return _scanMacs; }
  const std::vector<std::string>& getScanNames() const { return _scanNames; }
};

extern BlePageTurner BLE_PAGE_TURNER;
