#include "BlePageTurner.h"

#include <Logging.h>

// HID keycodes sent by common BLE page turners
// Consumer Control keycodes (used by volume-key-style clickers)
static constexpr uint16_t HID_VOLUME_UP = 0x00E9;
static constexpr uint16_t HID_VOLUME_DOWN = 0x00EA;
// Keyboard keycodes (used by some arrow-key-style clickers)
static constexpr uint8_t HID_KEY_RIGHT = 0x4F;
static constexpr uint8_t HID_KEY_LEFT = 0x50;
static constexpr uint8_t HID_KEY_PAGE_DOWN = 0x4E;
static constexpr uint8_t HID_KEY_PAGE_UP = 0x4B;

// HID service and characteristic UUIDs
static const NimBLEUUID HID_SERVICE_UUID("1812");
static const NimBLEUUID HID_REPORT_CHAR_UUID("2A4D");

BlePageTurner BLE_PAGE_TURNER;

void BlePageTurner::begin() {
  NimBLEDevice::init("CrossPoint");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::getScan()->setAdvertisedDeviceCallbacks(this, false);

  if (!_targetMac.empty()) {
    xTaskCreate(connectTask, "BleConnect", 4096, this, 1, nullptr);
  }
}

void BlePageTurner::setTargetMac(const std::string& mac) { _targetMac = mac; }

void BlePageTurner::startScan(int durationSecs) {
  _scanMacs.clear();
  _scanNames.clear();
  _scanning = true;

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  // Run scan async; use completion callback to clear flag
  scan->start(durationSecs, [](NimBLEScanResults) { BLE_PAGE_TURNER.stopScan(); }, false);
}

void BlePageTurner::stopScan() {
  NimBLEDevice::getScan()->stop();
  _scanning = false;
}

void BlePageTurner::onResult(NimBLEAdvertisedDevice* device) {
  const std::string mac = device->getAddress().toString();
  const std::string name = device->haveName() ? device->getName() : "Unknown";

  // Deduplicate
  for (const auto& existing : _scanMacs) {
    if (existing == mac) return;
  }

  _scanMacs.push_back(mac);
  _scanNames.push_back(name);
  LOG_DBG("BLE", "Found device: %s (%s)", name.c_str(), mac.c_str());

  // If this is our target, stop scan and connect
  if (!_targetMac.empty() && mac == _targetMac) {
    NimBLEDevice::getScan()->stop();
    xTaskCreate(connectTask, "BleConnect", 4096, this, 1, nullptr);
  }
}


void BlePageTurner::connectTask(void* param) {
  auto* self = static_cast<BlePageTurner*>(param);
  // Retry loop — keep trying to connect until successful
  while (!self->_connected) {
    if (self->connectToTarget()) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
  vTaskDelete(nullptr);
}

bool BlePageTurner::connectToTarget() {
  if (_targetMac.empty()) return false;

  NimBLEAddress address(_targetMac);
  if (_client == nullptr) {
    _client = NimBLEDevice::createClient();
    _client->setClientCallbacks(this, false);
    _client->setConnectionParams(12, 12, 0, 51);
  }

  LOG_DBG("BLE", "Connecting to %s", _targetMac.c_str());
  if (!_client->connect(address)) {
    LOG_DBG("BLE", "Connection failed");
    return false;
  }

  subscribeToHid();
  return true;
}

void BlePageTurner::subscribeToHid() {
  NimBLERemoteService* hidService = _client->getService(HID_SERVICE_UUID);
  if (!hidService) {
    LOG_DBG("BLE", "HID service not found");
    _client->disconnect();
    return;
  }

  // Subscribe to all HID Report characteristics (page turners may have multiple)
  auto characteristics = hidService->getCharacteristics(true);
  bool subscribed = false;
  for (auto* chr : *characteristics) {
    if (chr->getUUID() == HID_REPORT_CHAR_UUID && chr->canNotify()) {
      chr->subscribe(true, notifyCallback);
      subscribed = true;
      LOG_DBG("BLE", "Subscribed to HID report");
    }
  }

  if (!subscribed) {
    LOG_DBG("BLE", "No notifiable HID report found");
    _client->disconnect();
  }
}

void BlePageTurner::notifyCallback(NimBLERemoteCharacteristic* /*characteristic*/, uint8_t* data,
                                   size_t len, bool /*isNotify*/) {
  if (len == 0) return;

  // Consumer Control report (2 bytes, little-endian keycode)
  if (len >= 2) {
    uint16_t keycode = data[0] | (static_cast<uint16_t>(data[1]) << 8);
    if (keycode == HID_VOLUME_UP) {
      BLE_PAGE_TURNER._nextPressed = true;
      return;
    }
    if (keycode == HID_VOLUME_DOWN) {
      BLE_PAGE_TURNER._prevPressed = true;
      return;
    }
  }

  // Keyboard report: modifier(1) + reserved(1) + keycodes(6)
  if (len >= 3) {
    for (size_t i = 2; i < len; i++) {
      switch (data[i]) {
        case HID_KEY_RIGHT:
        case HID_KEY_PAGE_DOWN:
          BLE_PAGE_TURNER._nextPressed = true;
          return;
        case HID_KEY_LEFT:
        case HID_KEY_PAGE_UP:
          BLE_PAGE_TURNER._prevPressed = true;
          return;
        default:
          break;
      }
    }
  }
}

void BlePageTurner::onConnect(NimBLEClient* /*client*/) {
  _connected = true;
  LOG_DBG("BLE", "Page turner connected");
}

void BlePageTurner::onDisconnect(NimBLEClient* /*client*/) {
  _connected = false;
  LOG_DBG("BLE", "Page turner disconnected, will retry");
  // Reconnect in background
  xTaskCreate(connectTask, "BleReconnect", 4096, this, 1, nullptr);
}
