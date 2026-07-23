#include "QuickSyncActivity.h"

#include <ArduinoJson.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdlib>

#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "WifiCredentialStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/StringUtils.h"

namespace {
constexpr char CONFIG_PATH[] = "/quicksync.json";
constexpr char CURSOR_PATH[] = "/quicksync_cursor.txt";
// The manifest is capped server-side at 25 entries; reserving that up front
// keeps the vector from reallocating (and fragmenting DRAM) while parsing.
constexpr size_t MAX_ITEMS = 25;
}  // namespace

bool QuickSyncActivity::isConfigured() { return Storage.exists(CONFIG_PATH); }

// ---------------------------------------------------------------- lifecycle

void QuickSyncActivity::onEnter() {
  Activity::onEnter();

  state = SyncState::CONNECTING;
  items.clear();
  candidateSsids.clear();
  candidateIndex = 0;
  itemIndex = 0;
  downloadedCount = 0;
  downloadProgress = 0;
  downloadTotal = 0;
  errorMessage.clear();
  newestPath.clear();
  newestTitle.clear();
  openNewestOnExit = false;
  statusMessage = tr(STR_CONNECTING);
  requestUpdate();

  if (!loadConfig()) {
    return;  // loadConfig() has already moved us to ERROR
  }

  // Already on a network (e.g. entered right after the file transfer screen)?
  // Skip straight to the manifest.
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    wifiActivated = true;
    state = SyncState::FETCHING;
    statusMessage = tr(STR_QS_CHECKING);
    requestUpdate();
    return;
  }

  buildCandidateList();
  if (candidateSsids.empty()) {
    failWith(tr(STR_QS_NO_SAVED_NETWORKS));
    return;
  }

  wifiActivated = true;
  startNextCandidate();
}

void QuickSyncActivity::onExit() {
  Activity::onExit();
  items.clear();
  candidateSsids.clear();

  if (wifiActivated || WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    // Same heap-defrag reboot the other WiFi activities perform on the way out.
    // Routing it at the reader is how we "open" the freshly downloaded book:
    // main.cpp resumes into APP_STATE.openEpubPath on a reader-target restart.
    if (openNewestOnExit) {
      silentRestartToReader();
    } else {
      silentRestart();
    }
  }
}

// ------------------------------------------------------------------- config

bool QuickSyncActivity::loadConfig() {
  String raw;
  {
    // SD and the panel share the SPI bus - hold the render lock while reading.
    RenderLock lock(*this);
    if (!Storage.exists(CONFIG_PATH)) {
      lock.unlock();
      failWith(tr(STR_QS_NO_CONFIG));
      return false;
    }
    raw = Storage.readFile(CONFIG_PATH);
    cursor = loadCursor();
  }

  if (raw.length() == 0) {
    failWith(tr(STR_QS_BAD_CONFIG));
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, raw.c_str());
  if (error) {
    LOG_ERR("QSYNC", "Config parse failed: %s", error.c_str());
    failWith(tr(STR_QS_BAD_CONFIG));
    return false;
  }

  if (!doc["manifest_url"].is<const char*>()) {
    LOG_ERR("QSYNC", "Config has no manifest_url");
    failWith(tr(STR_QS_BAD_CONFIG));
    return false;
  }
  manifestUrl = doc["manifest_url"].as<std::string>();
  if (manifestUrl.empty()) {
    failWith(tr(STR_QS_BAD_CONFIG));
    return false;
  }

  // Optional: download destination. Defaults to the SD root, which is where the
  // OPDS browser puts its downloads and where the file browser opens.
  if (doc["dest_dir"].is<const char*>()) {
    destDir = doc["dest_dir"].as<std::string>();
  }
  if (destDir.empty()) {
    destDir = "/";
  }
  if (destDir.front() != '/') {
    destDir.insert(destDir.begin(), '/');
  }
  while (destDir.size() > 1 && destDir.back() == '/') {
    destDir.pop_back();
  }

  LOG_DBG("QSYNC", "Config loaded, dest=%s cursor=%lu", destDir.c_str(), static_cast<unsigned long>(cursor));
  return true;
}

uint32_t QuickSyncActivity::loadCursor() const {
  // Caller holds the render lock.
  if (!Storage.exists(CURSOR_PATH)) {
    return 0;
  }
  char buf[16] = {0};
  const size_t read = Storage.readFileToBuffer(CURSOR_PATH, buf, sizeof(buf));
  if (read == 0) {
    return 0;
  }
  const long value = strtol(buf, nullptr, 10);
  return value > 0 ? static_cast<uint32_t>(value) : 0;
}

void QuickSyncActivity::saveCursor(const uint32_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
  RenderLock lock(*this);
  if (!Storage.writeFile(CURSOR_PATH, String(buf))) {
    LOG_ERR("QSYNC", "Failed to persist cursor %s", buf);
  }
}

// --------------------------------------------------------------------- wifi

void QuickSyncActivity::buildCandidateList() {
  {
    RenderLock lock(*this);
    WIFI_STORE.loadFromFile();
  }

  const std::string lastSsid = WIFI_STORE.getLastConnectedSsid();
  const auto& saved = WIFI_STORE.getCredentials();
  candidateSsids.reserve(saved.size());

  // Last connected first, then every other saved network as a fallback.
  if (!lastSsid.empty() && WIFI_STORE.findCredential(lastSsid) != nullptr) {
    candidateSsids.push_back(lastSsid);
  }
  for (const auto& cred : saved) {
    if (cred.ssid != lastSsid) {
      candidateSsids.push_back(cred.ssid);
    }
  }
}

void QuickSyncActivity::startNextCandidate() {
  if (candidateIndex >= candidateSsids.size()) {
    failWith(tr(STR_WIFI_CONN_FAILED));
    return;
  }

  const std::string& ssid = candidateSsids[candidateIndex];
  const auto* cred = WIFI_STORE.findCredential(ssid);
  if (!cred) {
    candidateIndex++;
    startNextCandidate();
    return;
  }

  LOG_DBG("QSYNC", "Joining %s (%u/%u)", ssid.c_str(), static_cast<unsigned>(candidateIndex + 1),
          static_cast<unsigned>(candidateSsids.size()));

  state = SyncState::CONNECTING;
  statusMessage = ssid;
  connectStartTime = millis();
  requestUpdate();

  // Same join sequence as WifiSelectionActivity::attemptConnection().
  WiFi.persistent(false);  // Credentials live in WifiCredentialStore, not NVS
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  const String hostname = "CrossPoint-Reader-" + mac;
  WiFi.setHostname(hostname.c_str());

  if (!cred->password.empty()) {
    WiFi.begin(ssid.c_str(), cred->password.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }
}

void QuickSyncActivity::checkConnectionStatus() {
  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    LOG_DBG("QSYNC", "Connected to %s", candidateSsids[candidateIndex].c_str());
    {
      RenderLock lock(*this);
      WIFI_STORE.setLastConnectedSsid(candidateSsids[candidateIndex]);
    }
    state = SyncState::FETCHING;
    statusMessage = tr(STR_QS_CHECKING);
    requestUpdate();
    return;
  }

  const bool failed = (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL);
  if (failed || millis() - connectStartTime > CONNECT_TIMEOUT_MS) {
    LOG_DBG("QSYNC", "Join failed for %s, trying next", candidateSsids[candidateIndex].c_str());
    WiFi.disconnect();
    candidateIndex++;
    startNextCandidate();
  }
}

void QuickSyncActivity::launchWifiSelection() {
  state = SyncState::WIFI_SELECTION;
  wifiActivated = true;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void QuickSyncActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    failWith(tr(STR_WIFI_CONN_FAILED));
    return;
  }
  state = SyncState::FETCHING;
  statusMessage = tr(STR_QS_CHECKING);
  requestUpdate(true);
}

// ----------------------------------------------------------------- manifest

void QuickSyncActivity::fetchManifest() {
  std::string url = manifestUrl;
  // manifest_url already carries ?token=..., but don't assume it.
  url += (url.find('?') == std::string::npos) ? '?' : '&';
  url += "since=";
  url += std::to_string(cursor);

  LOG_DBG("QSYNC", "Fetching manifest (since=%lu)", static_cast<unsigned long>(cursor));

  std::string body;
  if (!HttpDownloader::fetchUrl(url, body)) {
    failWith(tr(STR_QS_FETCH_FAILED));
    return;
  }

  // Filter down to the four fields we use so the parser never materialises the
  // rest of the manifest (added/size/type) in DRAM.
  JsonDocument filter;
  filter["items"][0]["id"] = true;
  filter["items"][0]["seq"] = true;
  filter["items"][0]["title"] = true;
  filter["items"][0]["url"] = true;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  body.clear();
  body.shrink_to_fit();
  if (error) {
    LOG_ERR("QSYNC", "Manifest parse failed: %s", error.c_str());
    failWith(tr(STR_QS_PARSE_FAILED));
    return;
  }

  if (!doc["items"].is<JsonArray>()) {
    LOG_ERR("QSYNC", "Manifest has no items array");
    failWith(tr(STR_QS_PARSE_FAILED));
    return;
  }

  const JsonArray array = doc["items"].as<JsonArray>();
  items.reserve(std::min(static_cast<size_t>(array.size()), MAX_ITEMS));
  for (JsonObject entry : array) {
    if (items.size() >= MAX_ITEMS) {
      LOG_ERR("QSYNC", "Manifest exceeded %u items, ignoring the rest", static_cast<unsigned>(MAX_ITEMS));
      break;
    }
    SyncItem item;
    item.id = entry["id"].is<const char*>() ? entry["id"].as<std::string>() : std::string();
    item.title = entry["title"].is<const char*>() ? entry["title"].as<std::string>() : std::string();
    item.url = entry["url"].is<const char*>() ? entry["url"].as<std::string>() : std::string();
    item.seq = entry["seq"].as<uint32_t>();
    if (item.url.empty() || item.id.empty()) {
      LOG_ERR("QSYNC", "Skipping manifest entry with no id/url");
      continue;
    }
    if (item.title.empty()) {
      item.title = item.id;
    }
    items.push_back(std::move(item));
  }

  LOG_DBG("QSYNC", "Manifest returned %u new item(s)", static_cast<unsigned>(items.size()));

  if (items.empty()) {
    state = SyncState::DONE;
    doneTime = millis();
    requestUpdate();
    return;
  }

  // Make sure the destination directory exists before the first write.
  if (destDir != "/") {
    RenderLock lock(*this);
    if (!Storage.exists(destDir.c_str())) {
      Storage.mkdir(destDir.c_str());
    }
  }

  itemIndex = 0;
  state = SyncState::DOWNLOADING;
  statusMessage = items[0].title;
  requestUpdate();
}

// ---------------------------------------------------------------- downloads

void QuickSyncActivity::downloadNextItem() {
  if (itemIndex >= items.size()) {
    state = SyncState::DONE;
    doneTime = millis();
    requestUpdate();
    return;
  }

  const SyncItem& item = items[itemIndex];
  statusMessage = item.title;
  downloadProgress = 0;
  downloadTotal = 0;
  requestUpdate(true);

  std::string path = destDir;
  if (path.back() != '/') {
    path += '/';
  }
  const std::string base = StringUtils::sanitizeFilename(item.title);
  path += base + ".epub";
  if (Storage.exists(path.c_str())) {
    // Distinct items can share a title (e.g. two saves of the same article);
    // the server id is unique, so fall back to it rather than clobbering.
    path = destDir;
    if (path.back() != '/') {
      path += '/';
    }
    path += base + " (" + StringUtils::sanitizeFilename(item.id) + ").epub";
  }

  const auto result =
      HttpDownloader::downloadToFile(item.url, path, [this](const size_t downloaded, const size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
      });

  if (result != HttpDownloader::OK) {
    LOG_ERR("QSYNC", "Download failed for %s", item.id.c_str());
    // Stop here rather than skipping ahead: the cursor is a high-water mark, so
    // advancing past a failed item would drop it permanently. The next sync
    // retries from this seq.
    failWith(tr(STR_DOWNLOAD_FAILED));
    return;
  }

  LOG_DBG("QSYNC", "Downloaded %s -> %s", item.id.c_str(), path.c_str());

  // Drop any stale layout cache keyed to this path (same guard the OPDS
  // browser applies after a download).
  {
    Epub epub(path, "/.crosspoint");
    epub.clearCache();
  }

  downloadedCount++;
  newestPath = path;
  newestTitle = item.title;
  if (item.seq > cursor) {
    cursor = item.seq;
    // Persist per item: an interrupted sync then resumes rather than
    // re-downloading everything it already wrote.
    saveCursor(cursor);
  }

  itemIndex++;
  if (itemIndex >= items.size()) {
    state = SyncState::DONE;
    doneTime = millis();
  }
  requestUpdate();
}

// -------------------------------------------------------------------- flow

void QuickSyncActivity::openNewestBook() {
  if (newestPath.empty()) {
    onGoHome();
    return;
  }
  LOG_DBG("QSYNC", "Opening %s", newestPath.c_str());
  {
    RenderLock lock(*this);
    APP_STATE.openEpubPath = newestPath;
    APP_STATE.saveToFile();
  }
  openNewestOnExit = true;
  // onExit() drops WiFi and restarts straight into the reader.
  onGoHome();
}

void QuickSyncActivity::failWith(const char* message) {
  errorMessage = message;
  state = SyncState::ERROR;
  requestUpdate();
}

void QuickSyncActivity::loop() {
  switch (state) {
    case SyncState::WIFI_SELECTION:
      // Driven by the WifiSelectionActivity subactivity.
      return;

    case SyncState::CONNECTING:
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        onGoHome();
        return;
      }
      checkConnectionStatus();
      return;

    case SyncState::FETCHING:
      fetchManifest();
      return;

    case SyncState::DOWNLOADING:
      downloadNextItem();
      return;

    case SyncState::DONE:
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        onGoHome();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        openNewestBook();
        return;
      }
      // One-tap flow: after a readable pause, open the newest book on its own.
      if (downloadedCount > 0 && millis() - doneTime > AUTO_OPEN_DELAY_MS) {
        openNewestBook();
      }
      return;

    case SyncState::ERROR:
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        onGoHome();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (manifestUrl.empty()) {
          // The config never loaded - there is nothing to retry, only to fix on
          // the SD card. Don't bring WiFi up for a sync we can't address.
          onGoHome();
        } else if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
          // Online already - just re-run the manifest step.
          state = SyncState::FETCHING;
          statusMessage = tr(STR_QS_CHECKING);
          requestUpdate();
        } else {
          launchWifiSelection();
        }
      }
      return;
  }
}

// ------------------------------------------------------------------ render

void QuickSyncActivity::render(RenderLock&&) {
  if (state == SyncState::WIFI_SELECTION) {
    return;  // The subactivity owns the screen
  }

  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto maxTextWidth = pageWidth - 40;

  renderer.drawCenteredText(UI_12_FONT_ID, 15, tr(STR_QUICK_SYNC), true, EpdFontFamily::BOLD);

  switch (state) {
    case SyncState::CONNECTING: {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_CONNECTING));
      const auto ssid = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), maxTextWidth);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, ssid.c_str());
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      break;
    }

    case SyncState::FETCHING:
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
      break;

    case SyncState::DOWNLOADING: {
      char counter[32];
      snprintf(counter, sizeof(counter), "%u/%u", static_cast<unsigned>(itemIndex + 1),
               static_cast<unsigned>(items.size()));
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60, tr(STR_DOWNLOADING));
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, counter);
      const auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), maxTextWidth);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, title.c_str());
      if (downloadTotal > 0) {
        const int barWidth = pageWidth - 100;
        constexpr int barHeight = 20;
        constexpr int barX = 50;
        const int barY = pageHeight / 2 + 30;
        GUI.drawProgressBar(renderer, Rect{barX, barY, barWidth, barHeight}, downloadProgress, downloadTotal);
      }
      break;
    }

    case SyncState::DONE: {
      if (downloadedCount == 0) {
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_QS_UP_TO_DATE));
        const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      } else {
        char summary[48];
        if (downloadedCount == 1) {
          snprintf(summary, sizeof(summary), "%s", tr(STR_QS_ONE_NEW_ITEM));
        } else {
          snprintf(summary, sizeof(summary), tr(STR_QS_NEW_ITEMS), downloadedCount);
        }
        renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 30, summary, true, EpdFontFamily::BOLD);
        const auto title = renderer.truncatedText(UI_10_FONT_ID, newestTitle.c_str(), maxTextWidth);
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, title.c_str());
        renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 45, tr(STR_QS_OPENING));
        const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), "", "");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      }
      break;
    }

    case SyncState::ERROR: {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
      const auto message = renderer.truncatedText(UI_10_FONT_ID, errorMessage.c_str(), maxTextWidth);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, message.c_str());
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      break;
    }

    case SyncState::WIFI_SELECTION:
      break;
  }

  renderer.displayBuffer();
}
