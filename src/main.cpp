#include <Arduino.h>
#include <Epub.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <HalTiltSensor.h>
#include <I18n.h>
#include <Logging.h>
#include <SPI.h>
#include <WiFi.h>
#include <builtinFonts/all.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "SdCardFontSystem.h"
#include "WifiCredentialStore.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "activities/boot_sleep/SleepActivity.h"
#include "activities/settings/SdFirmwareUpdateActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/LoadingIcon.h"
#include "network/HttpDownloader.h"
#include "network/OtaUpdater.h"
#include "util/ButtonNavigator.h"
#include "util/ScreenshotUtil.h"

MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
ActivityManager activityManager(renderer, mappedInputManager);
FontDecompressor fontDecompressor;
SdCardFontSystem sdFontSystem;
FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts());
static unsigned long allowSleepAt = 0;

// Fonts
EpdFont notoserif14RegularFont(&notoserif_14_regular);
EpdFont notoserif14BoldFont(&notoserif_14_bold);
EpdFont notoserif14ItalicFont(&notoserif_14_italic);
EpdFont notoserif14BoldItalicFont(&notoserif_14_bolditalic);
EpdFontFamily notoserif14FontFamily(&notoserif14RegularFont, &notoserif14BoldFont, &notoserif14ItalicFont,
                                    &notoserif14BoldItalicFont);
#ifndef OMIT_FONTS
EpdFont notoserif12RegularFont(&notoserif_12_regular);
EpdFont notoserif12BoldFont(&notoserif_12_bold);
EpdFont notoserif12ItalicFont(&notoserif_12_italic);
EpdFont notoserif12BoldItalicFont(&notoserif_12_bolditalic);
EpdFontFamily notoserif12FontFamily(&notoserif12RegularFont, &notoserif12BoldFont, &notoserif12ItalicFont,
                                    &notoserif12BoldItalicFont);
EpdFont notoserif16RegularFont(&notoserif_16_regular);
EpdFont notoserif16BoldFont(&notoserif_16_bold);
EpdFont notoserif16ItalicFont(&notoserif_16_italic);
EpdFont notoserif16BoldItalicFont(&notoserif_16_bolditalic);
EpdFontFamily notoserif16FontFamily(&notoserif16RegularFont, &notoserif16BoldFont, &notoserif16ItalicFont,
                                    &notoserif16BoldItalicFont);
EpdFont notoserif18RegularFont(&notoserif_18_regular);
EpdFont notoserif18BoldFont(&notoserif_18_bold);
EpdFont notoserif18ItalicFont(&notoserif_18_italic);
EpdFont notoserif18BoldItalicFont(&notoserif_18_bolditalic);
EpdFontFamily notoserif18FontFamily(&notoserif18RegularFont, &notoserif18BoldFont, &notoserif18ItalicFont,
                                    &notoserif18BoldItalicFont);

EpdFont notosans12RegularFont(&notosans_12_regular);
EpdFont notosans12BoldFont(&notosans_12_bold);
EpdFont notosans12ItalicFont(&notosans_12_italic);
EpdFont notosans12BoldItalicFont(&notosans_12_bolditalic);
EpdFontFamily notosans12FontFamily(&notosans12RegularFont, &notosans12BoldFont, &notosans12ItalicFont,
                                   &notosans12BoldItalicFont);
EpdFont notosans14RegularFont(&notosans_14_regular);
EpdFont notosans14BoldFont(&notosans_14_bold);
EpdFont notosans14ItalicFont(&notosans_14_italic);
EpdFont notosans14BoldItalicFont(&notosans_14_bolditalic);
EpdFontFamily notosans14FontFamily(&notosans14RegularFont, &notosans14BoldFont, &notosans14ItalicFont,
                                   &notosans14BoldItalicFont);
EpdFont notosans16RegularFont(&notosans_16_regular);
EpdFont notosans16BoldFont(&notosans_16_bold);
EpdFont notosans16ItalicFont(&notosans_16_italic);
EpdFont notosans16BoldItalicFont(&notosans_16_bolditalic);
EpdFontFamily notosans16FontFamily(&notosans16RegularFont, &notosans16BoldFont, &notosans16ItalicFont,
                                   &notosans16BoldItalicFont);
EpdFont notosans18RegularFont(&notosans_18_regular);
EpdFont notosans18BoldFont(&notosans_18_bold);
EpdFont notosans18ItalicFont(&notosans_18_italic);
EpdFont notosans18BoldItalicFont(&notosans_18_bolditalic);
EpdFontFamily notosans18FontFamily(&notosans18RegularFont, &notosans18BoldFont, &notosans18ItalicFont,
                                   &notosans18BoldItalicFont);

#endif  // OMIT_FONTS

EpdFont smallFont(&notosans_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

EpdFont ui10RegularFont(&ubuntu_10_regular);
EpdFont ui10BoldFont(&ubuntu_10_bold);
EpdFontFamily ui10FontFamily(&ui10RegularFont, &ui10BoldFont);

EpdFont ui12RegularFont(&ubuntu_12_regular);
EpdFont ui12BoldFont(&ubuntu_12_bold);
EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);

// measurement of power button press duration calibration value
unsigned long t1 = 0;
unsigned long t2 = 0;

// Definitions for SilentRestart.h. RTC_NOINIT survives ESP.restart() but not power loss.
RTC_NOINIT_ATTR uint32_t silentRebootMagic;
RTC_NOINIT_ATTR uint32_t silentRebootTarget;
constexpr uint32_t SILENT_REBOOT_MAGIC = 0xC1EAB007;
constexpr uint32_t SILENT_REBOOT_TARGET_HOME = 0;
constexpr uint32_t SILENT_REBOOT_TARGET_READER = 1;

// How the device is coming back to life, resolved once at boot. Both resume
// flows suppress the splash and leave the panel holding its pre-boot frame; a
// plain boot shows the splash. See setup() for the resolution.
enum class BootResume : uint8_t {
  Splash,       // cold boot, flash, panic, or plain reboot
  Silent,       // heap-defrag ESP.restart() (RTC flag; lost on power loss)
  QuickResume,  // wake from a quick-resume deep sleep (SD flag; survives power loss)
};

// Latched true once enterDeepSleep() commits to sleeping, before it tears down
// the current activity. WiFi activities call silentRestart() in onExit() to
// clear heap fragmentation on the way out, but deep sleep is a full chip reset
// on wake and already clears the heap, so rebooting here would just power the
// device back up against the user's sleep gesture. Never cleared:
// startDeepSleep() does not return, so a set latch only ends at the wakeup reset.
static bool deepSleepInProgress = false;

void silentRestart() {
  if (deepSleepInProgress) return;  // sleeping supersedes the heap-defrag reboot
  silentRebootTarget = SILENT_REBOOT_TARGET_HOME;
  silentRebootMagic = SILENT_REBOOT_MAGIC;
  LOG_DBG("MAIN", "Silent restart (target=home)");
  // E-ink retains the previous frame until Home's first paint lands (~2-3s).
  // Without an overlay, users don't see the reboot and fire input through to
  // Home. Select on the default selectorIndex=0 then opens the most-recent
  // book, looking like a trampoline back to the reader they just exited.
  GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  delay(50);
  ESP.restart();
}

void silentRestartToReader() {
  if (deepSleepInProgress) return;  // sleeping supersedes the heap-defrag reboot
  silentRebootTarget = SILENT_REBOOT_TARGET_READER;
  silentRebootMagic = SILENT_REBOOT_MAGIC;
  LOG_DBG("MAIN", "Silent restart (target=reader)");
  GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  delay(50);
  ESP.restart();
}

// Verify power button press duration on wake-up from deep sleep
// Pre-condition: isWakeupByPowerButton() == true
void verifyPowerButtonDuration() {
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) {
    // Fast path for short press
    // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
    return;
  }

  // Give the user up to 1000ms to start holding the power button, and must hold for SETTINGS.getPowerButtonDuration()
  const auto start = millis();
  bool abort = false;
  // Subtract the current time, because inputManager only starts counting the HeldTime from the first update()
  // This way, we remove the time we already took to reach here from the duration,
  // assuming the button was held until now from millis()==0 (i.e. device start time).
  const uint16_t calibration = start;
  const uint16_t calibratedPressDuration =
      (calibration < SETTINGS.getPowerButtonDuration()) ? SETTINGS.getPowerButtonDuration() - calibration : 1;

  gpio.update();
  // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
  while (!gpio.isPressed(HalGPIO::BTN_POWER) && millis() - start < 1000) {
    delay(10);  // only wait 10ms each iteration to not delay too much in case of short configured duration.
    gpio.update();
  }

  t2 = millis();
  if (gpio.isPressed(HalGPIO::BTN_POWER)) {
    do {
      delay(10);
      gpio.update();
    } while (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getPowerButtonHeldTime() < calibratedPressDuration);
    abort = gpio.getPowerButtonHeldTime() < calibratedPressDuration;
  } else {
    abort = true;
  }

  if (abort) {
    // Button released too early. Returning to sleep.
    // IMPORTANT: Re-arm the wakeup trigger before sleeping again
    powerManager.startDeepSleep(gpio);
  }
}
void waitForPowerRelease() {
  gpio.update();
  while (gpio.isPressed(HalGPIO::BTN_POWER)) {
    delay(50);
    gpio.update();
  }
}

constexpr char SLEEP_FRAME_FILE[] = "/.crosspoint/sleep_frame.bin";

static void saveSleepFrameBuffer() {
  HalFile file;
  if (!Storage.openFileForWrite("SLP", SLEEP_FRAME_FILE, file)) return;
  file.write(renderer.getFrameBuffer(), renderer.getBufferSize());
  file.close();
}

static bool loadSleepFrameBuffer() {
  HalFile file;
  if (!Storage.openFileForRead("SLP", SLEEP_FRAME_FILE, file)) return false;
  const size_t bufferSize = display.getBufferSize();
  const size_t bytesRead = file.read(display.getFrameBuffer(), bufferSize);
  file.close();
  if (bytesRead != bufferSize) {
    Storage.remove(SLEEP_FRAME_FILE);
    return false;
  }
  Storage.remove(SLEEP_FRAME_FILE);
  return true;
}

// ── Dashboard auto-sync ──────────────────────────────────────────────────────
// Optional: if the SD card root holds a "/dashboard.url" file, briefly bring up
// WiFi (last saved network), download that URL into /sleep.bmp, then tear WiFi
// back down. The file's presence is the on/off switch (SleepActivity renders
// /sleep.bmp whenever the file exists — the user's configured sleep screen mode
// is never rewritten), and editing it re-points the source with no firmware
// rebuild.
//
// File format (see docs/dashboard-sync.md):
//   line 1: http(s) URL of a BMP image
//   line 2: optional refresh interval in minutes (default 60, 0 = only at sleep entry)
//   any later line "battery": also refresh on battery, using light sleep
//
// Invoked from enterDeepSleep() AFTER goToSleep() has torn down the outgoing
// activity: the reader holds ~65KB (Epub + Section) that the WiFi stack and a
// TLS handshake need (the KOReader sync flow frees both before connecting for
// the same reason). Never invoked for quick-resume sleeps — there the sleep
// screen is the user's live session frame and a downloaded image is not shown.
//
// While USB-powered, an RTC timer wake re-runs the fetch every `refreshMinutes`
// so the sleeping screen keeps tracking the source. On battery the MCU is
// powered off during deep sleep (see HalPowerManager::startDeepSleep), so the
// periodic refresh is only available there when the file opts into the
// light-sleep loop with "battery" — which keeps the MCU alive and costs battery.
namespace {
constexpr uint32_t DASHBOARD_DEFAULT_REFRESH_MINUTES = 60;
constexpr uint32_t DASHBOARD_MIN_REFRESH_MINUTES = 5;
constexpr uint32_t DASHBOARD_MAX_REFRESH_MINUTES = 24 * 60;

struct DashboardConfig {
  std::string url;
  uint32_t refreshMinutes = DASHBOARD_DEFAULT_REFRESH_MINUTES;
  bool refreshOnBattery = false;
};

std::string trimWhitespace(const std::string& s) {
  const size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return {};
  const size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}
}  // namespace

static bool readDashboardConfig(DashboardConfig& out) {
  if (!Storage.exists(DASHBOARD_URL_FILE)) return false;
  HalFile f;
  if (!Storage.openFileForRead("SYNC", DASHBOARD_URL_FILE, f)) return false;
  char buf[256] = {0};
  const int n = f.read(buf, sizeof(buf) - 1);  // f auto-closes at scope exit
  if (n <= 0) return false;

  const std::string text(buf, n);
  const size_t eol = text.find_first_of("\r\n");
  out.url = trimWhitespace(text.substr(0, eol));
  if (out.url.rfind("http", 0) != 0) return false;  // sanity: must look like a URL

  out.refreshMinutes = DASHBOARD_DEFAULT_REFRESH_MINUTES;
  out.refreshOnBattery = false;
  // Remaining lines: a bare number sets the interval, "battery" opts into the
  // light-sleep refresh loop. Order does not matter; unknown lines are ignored.
  size_t pos = eol;
  while (pos != std::string::npos) {
    const size_t next = text.find_first_of("\r\n", pos + 1);
    const std::string line = trimWhitespace(text.substr(pos, next == std::string::npos ? next : next - pos));
    pos = next;
    if (line.empty()) continue;
    if (strcasecmp(line.c_str(), "battery") == 0) {
      out.refreshOnBattery = true;
      continue;
    }
    char* endp = nullptr;
    const unsigned long minutes = strtoul(line.c_str(), &endp, 10);
    if (endp != line.c_str()) {
      out.refreshMinutes = minutes == 0 ? 0
                                        : std::max(DASHBOARD_MIN_REFRESH_MINUTES,
                                                   std::min<uint32_t>(minutes, DASHBOARD_MAX_REFRESH_MINUTES));
    }
  }
  return true;
}

// Join the best saved network in range: scan, rank saved SSIDs by signal, and
// try them strongest-first. An idle phone hotspot stops beaconing and will not
// show up in a scan, so the last connected network is always tried as a final
// fallback even when the scan missed it. Leaves WiFi in STA mode either way;
// the caller tears it down.
static bool connectToSavedNetwork() {
  WIFI_STORE.loadFromFile();
  const auto& saved = WIFI_STORE.getCredentials();
  if (saved.empty()) {
    LOG_DBG("SYNC", "No saved WiFi; skipping dashboard sync");
    return false;
  }

  WiFi.persistent(false);  // creds owned by WifiCredentialStore; suppress SDK NVS
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  struct Candidate {
    const WifiCredential* cred;
    int32_t rssi;
  };
  constexpr int32_t RSSI_UNSEEN = -1000;  // sorts after any real reading
  std::vector<Candidate> candidates;
  candidates.reserve(saved.size());

  const int16_t found = WiFi.scanNetworks();  // blocking, a few seconds
  for (int16_t i = 0; i < found; i++) {
    const WifiCredential* cred = WIFI_STORE.findCredential(WiFi.SSID(i).c_str());
    if (cred == nullptr) continue;
    const int32_t rssi = WiFi.RSSI(i);
    auto it = std::find_if(candidates.begin(), candidates.end(), [cred](const Candidate& c) { return c.cred == cred; });
    if (it == candidates.end()) {
      candidates.push_back({cred, rssi});
    } else if (rssi > it->rssi) {
      it->rssi = rssi;  // same SSID on several APs: keep the strongest
    }
  }
  WiFi.scanDelete();
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.rssi > b.rssi; });

  const std::string& lastSsid = WIFI_STORE.getLastConnectedSsid();
  const WifiCredential* last = lastSsid.empty() ? nullptr : WIFI_STORE.findCredential(lastSsid);
  if (last != nullptr &&
      std::none_of(candidates.begin(), candidates.end(), [last](const Candidate& c) { return c.cred == last; })) {
    candidates.push_back({last, RSSI_UNSEEN});
  }
  if (candidates.empty()) {
    LOG_DBG("SYNC", "No saved WiFi in range; skipping dashboard sync");
    return false;
  }

  constexpr unsigned long CONNECT_TIMEOUT_MS = 7000;
  constexpr size_t MAX_ATTEMPTS = 3;  // bounds the worst case at ~21s of radio time
  for (size_t n = 0; n < candidates.size() && n < MAX_ATTEMPTS; n++) {
    const WifiCredential* cred = candidates[n].cred;
    LOG_DBG("SYNC", "Dashboard sync: connecting to %s (rssi %ld)", cred->ssid.c_str(),
            static_cast<long>(candidates[n].rssi));
    if (cred->password.empty()) {
      WiFi.begin(cred->ssid.c_str());
    } else {
      WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
    }
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT_MS) {
      if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) break;
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.disconnect(true, true);
    delay(100);
  }
  LOG_DBG("SYNC", "WiFi connect timed out; skipping dashboard sync");
  return false;
}

// ── Firmware self-update ─────────────────────────────────────────────────────
// Dashboard refreshes are the one moment the reader is online with no activity
// alive (largest possible heap), so they double as the update check. Rate
// limited to roughly one GitHub query per FIRMWARE_CHECK_INTERVAL_MINUTES via a
// refresh counter kept in RTC memory: it survives the deep sleeps between USB
// timer wakes and the light sleeps of the battery loop, and the magic guards
// the garbage a cold boot leaves there. After a successful install the reader
// drops a marker file and restarts; setup() sees the marker and goes straight
// back into the dashboard sleep loop instead of booting to the UI.
namespace {
constexpr char DASHBOARD_RESUME_MARKER[] = "/.crosspoint/dashboard_resume";
constexpr uint32_t FIRMWARE_CHECK_INTERVAL_MINUTES = 6 * 60;
constexpr uint32_t DASHBOARD_REFRESH_MAGIC = 0xDA5B0A2D;
}  // namespace
RTC_NOINIT_ATTR uint32_t dashboardRefreshMagic;
RTC_NOINIT_ATTR uint32_t dashboardRefreshCount;

// Pre-condition: WiFi is connected. Returns only when there is nothing to install.
static void maybeSelfUpdate(uint32_t refreshMinutes) {
  if (dashboardRefreshMagic != DASHBOARD_REFRESH_MAGIC) {
    dashboardRefreshMagic = DASHBOARD_REFRESH_MAGIC;
    dashboardRefreshCount = 0;
  }
  const uint32_t checkEvery =
      std::max<uint32_t>(1, FIRMWARE_CHECK_INTERVAL_MINUTES / std::max<uint32_t>(1, refreshMinutes));
  const bool due = (dashboardRefreshCount % checkEvery) == 0;
  dashboardRefreshCount++;
  if (!due) return;

  OtaUpdater updater;
  if (updater.checkForUpdate() != OtaUpdater::OK || !updater.isUpdateNewer()) {
    LOG_DBG("OTA", "Auto-update: nothing newer than " CROSSPOINT_VERSION);
    return;
  }
  LOG_INF("OTA", "Auto-update: installing %s", updater.getLatestVersion().c_str());
  if (updater.installUpdate() != OtaUpdater::OK) {
    LOG_ERR("OTA", "Auto-update failed; staying on " CROSSPOINT_VERSION);
    return;
  }
  {
    HalFile marker;
    if (Storage.openFileForWrite("OTA", DASHBOARD_RESUME_MARKER, marker)) {
      marker.write("1", 1);  // marker auto-closes at scope exit
    }
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  ESP.restart();
}

// Returns true when a fresh image landed in /sleep.bmp so the caller can repaint with it.
// allowSelfUpdate: also run the firmware update check while the network is up (see above).
static bool syncSleepImageFromUrl(const DashboardConfig& dashboard, bool allowSelfUpdate) {
  bool updated = false;
  if (connectToSavedNetwork()) {
    // Download to a temp path and swap on success, so a failed fetch keeps the
    // last good image (downloadToFile deletes its destination before writing).
    const HttpDownloader::DownloadError r = HttpDownloader::downloadToFile(dashboard.url, "/sleep.bmp.tmp");
    if (r == HttpDownloader::OK) {
      if (Storage.exists("/sleep.bmp")) Storage.remove("/sleep.bmp");
      updated = Storage.rename("/sleep.bmp.tmp", "/sleep.bmp");
      LOG_DBG("SYNC", "Sleep image updated from %s", dashboard.url.c_str());
    } else {
      LOG_DBG("SYNC", "Dashboard download failed (err %d)", static_cast<int>(r));
    }
    if (allowSelfUpdate) {
      maybeSelfUpdate(dashboard.refreshMinutes);  // restarts on success
    }
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return updated;
}

// Battery-powered dashboard refresh loop. Deep sleep on battery powers the MCU
// off, so instead stay in light sleep (RAM and the battery latch intact) and
// repeat fetch + repaint in place. Entered with WiFi off and the panel and tilt
// sensor already asleep. Never returns: a power-button wake restarts the chip so
// the regular boot path (splash, home/reader resume) runs exactly as it would
// after a deep-sleep wake. Costs the light-sleep idle draw of the whole board
// for as long as the reader sleeps, so it is opt-in via "battery" in the file.
[[noreturn]] static void runBatteryDashboardLoop(const DashboardConfig& dashboard, uint64_t refreshUs) {
  LOG_DBG("SYNC", "Battery dashboard loop: refresh every %lu min",
          static_cast<unsigned long>(dashboard.refreshMinutes));
  for (;;) {
    if (powerManager.lightSleep(gpio, refreshUs) == HalPowerManager::LightSleepWake::PowerButton) {
      LOG_DBG("SYNC", "Power button during battery dashboard loop; restarting");
      ESP.restart();
    }
    // Timer wake. The panel is in its own deep sleep, which only a hardware reset
    // leaves; begin() does that without touching the image it is holding.
    display.begin(/*seamless=*/true);
    if (syncSleepImageFromUrl(dashboard, /*allowSelfUpdate=*/true)) {
      activityManager.goToSleep(/*fromTimeout=*/false);  // repaint with the fresh image
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
    display.deepSleep();
    if (gpio.isUsbConnected()) {
      // Plugged in meanwhile: the deep-sleep timer path is cheaper, hand over to it.
      powerManager.startDeepSleep(gpio, refreshUs);
    }
  }
}

// Enter deep sleep mode.
// dashboardRefresh: this boot was a timer wake whose only job is to re-fetch the
// dashboard image and go back to sleep. No foreground activity exists, so the
// user's real sleep context (reader vs. home) and the saved app state are left
// untouched for the next power-button wake.
void enterDeepSleep(bool fromTimeout = false, bool dashboardRefresh = false) {
  HalPowerManager::Lock powerLock;  // Ensure we are at normal CPU frequency for sleep preparation
  if (!dashboardRefresh) {
    APP_STATE.lastSleepFromReader = activityManager.isReaderActivity();
  }

  const bool isQuickResumeSleep =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME ||
      (fromTimeout &&
       SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT);
  APP_STATE.showBootScreen = !isQuickResumeSleep;

  // A refresh boot changes no app state; skip the SPIFFS write (hourly sleeps would otherwise burn erase cycles).
  if (!dashboardRefresh) {
    APP_STATE.saveToFile();
  }

  // Commit to sleeping before goToSleep() runs the outgoing activity's onExit():
  // a WiFi activity would otherwise silentRestart() here and reboot instead.
  deepSleepInProgress = true;
  // On a refresh boot the panel already shows the previous dashboard; defer the
  // single paint until after the fetch so the screen doesn't flash twice.
  if (!dashboardRefresh) {
    activityManager.goToSleep(fromTimeout);
  }

  DashboardConfig dashboard;
  const bool hasDashboard = readDashboardConfig(dashboard);
  if (isQuickResumeSleep) {
    // Quick resume is the user's saved session: persist the frame untouched and
    // skip the dashboard sync (its image would not be shown, and WiFi would only
    // delay sleep and drain the battery).
    saveSleepFrameBuffer();
  } else if (hasDashboard) {
    // Self-update only on refresh boots: a user-initiated sleep should not
    // stall for a firmware download.
    const bool updated = syncSleepImageFromUrl(dashboard, /*allowSelfUpdate=*/dashboardRefresh);
    // A fresh dashboard landed in /sleep.bmp: repaint so this sleep shows it
    // (the paint above showed the previous image while WiFi was up). A refresh
    // boot has not painted yet, so it paints here even with the last good image.
    if (updated || dashboardRefresh) {
      activityManager.goToSleep(fromTimeout);
    }
  }

  // Tear down WiFi so the modem power domain isn't held alive across deep sleep.
  // Wake from deep sleep is effectively a chip reset, so no state needs to survive.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  halTiltSensor.deepSleep();
  display.deepSleep();
  LOG_DBG("MAIN", "Entering deep sleep");

  const bool periodicRefresh = hasDashboard && !isQuickResumeSleep && dashboard.refreshMinutes > 0;
  const uint64_t refreshUs = static_cast<uint64_t>(dashboard.refreshMinutes) * 60ULL * 1000000ULL;

  if (periodicRefresh && !gpio.isUsbConnected() && dashboard.refreshOnBattery) {
    runBatteryDashboardLoop(dashboard, refreshUs);  // does not return
  }

  // Periodic dashboard refresh on USB: arm an RTC timer wake so the next boot
  // re-fetches the image. Only useful on USB power, where the MCU stays alive
  // through deep sleep; on battery (without "battery" above) the refresh
  // happens at the next sleep entry instead.
  uint64_t timerWakeupUs = 0;
  if (periodicRefresh && gpio.isUsbConnected()) {
    timerWakeupUs = refreshUs;
    LOG_DBG("SYNC", "Dashboard refresh armed: %lu min", static_cast<unsigned long>(dashboard.refreshMinutes));
  }
  powerManager.startDeepSleep(gpio, timerWakeupUs);
}

void setupDisplayAndFonts(bool seamless = false) {
  display.begin(seamless);
  renderer.begin();
  activityManager.begin();
  LOG_DBG("MAIN", "Display initialized");

  // Initialize font decompressor for compressed reader fonts
  if (!fontDecompressor.init()) {
    LOG_ERR("MAIN", "Font decompressor init failed");
  }
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);
  renderer.insertFont(NOTOSERIF_14_FONT_ID, notoserif14FontFamily);
#ifndef OMIT_FONTS
  renderer.insertFont(NOTOSERIF_12_FONT_ID, notoserif12FontFamily);
  renderer.insertFont(NOTOSERIF_16_FONT_ID, notoserif16FontFamily);
  renderer.insertFont(NOTOSERIF_18_FONT_ID, notoserif18FontFamily);

  renderer.insertFont(NOTOSANS_12_FONT_ID, notosans12FontFamily);
  renderer.insertFont(NOTOSANS_14_FONT_ID, notosans14FontFamily);
  renderer.insertFont(NOTOSANS_16_FONT_ID, notosans16FontFamily);
  renderer.insertFont(NOTOSANS_18_FONT_ID, notosans18FontFamily);
#endif  // OMIT_FONTS
  renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);

  // Discover and load SD card fonts
  sdFontSystem.begin(renderer);

  LOG_DBG("MAIN", "Fonts setup");
}

void setup() {
  t1 = millis();

#ifdef ENABLE_SERIAL_LOG
  // Earliest possible Serial setup. The 250 ms stall before begin() lets the
  // USB Serial/JTAG peripheral finish power-on and lets the host complete USB
  // enumeration before we touch the CDC state — otherwise cold boot races
  // and the host has to be physically replugged for logs to flow. Warm reboot
  // worked without the delay because USB was already enumerated.
  delay(250);
  Serial.begin(115200);
  logSerial.setTxTimeoutMs(1);  // This is a load-bearing 1. Do not modify.
#endif

  HalSystem::begin();

  // Read-and-clear so a panic later in setup() doesn't loop into silent reboot.
  // Bound the target range too — RTC_NOINIT memory is uninitialized on cold boot.
  const bool isSilentReboot = (silentRebootMagic == SILENT_REBOOT_MAGIC);
  const uint32_t snapshotTarget =
      (isSilentReboot && silentRebootTarget <= SILENT_REBOOT_TARGET_READER) ? silentRebootTarget : 0;
  silentRebootMagic = 0;
  silentRebootTarget = 0;

  gpio.begin();
  powerManager.begin();
  halTiltSensor.begin();
  halClock.begin();

  LOG_INF("MAIN", "Hardware detect: %s", gpio.deviceIsX3() ? "X3" : "X4");

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD card initialization failed");
    setupDisplayAndFonts(isSilentReboot);
    activityManager.goToFullScreenMessage("SD card error", EpdFontFamily::BOLD);
    return;
  }

  HalSystem::checkPanic();

  SETTINGS.loadFromFile();
  APP_STATE.loadFromFile();
  RECENT_BOOKS.loadFromFile();
  I18N.setLanguage(static_cast<Language>(SETTINGS.language));
  KOREADER_STORE.loadFromFile();
  OPDS_STORE.loadFromFile();
  UITheme::getInstance().reload();
  ButtonNavigator::setMappedInputManager(mappedInputManager);

  const auto wakeupReason = gpio.getWakeupReason();
  switch (wakeupReason) {
    case HalGPIO::WakeupReason::PowerButton:
      LOG_DBG("MAIN", "Verifying power button press duration");
      gpio.verifyPowerButtonWakeup(SETTINGS.getPowerButtonDuration(),
                                   SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP);
      break;
    case HalGPIO::WakeupReason::AfterUSBPower:
      // If USB power caused a cold boot, go back to sleep
      LOG_DBG("MAIN", "Wakeup reason: After USB Power");
      powerManager.startDeepSleep(gpio);
      break;
    case HalGPIO::WakeupReason::Timer:
      // USB-powered dashboard refresh: bring up the display without the splash,
      // re-fetch and re-render the sleep image, then sleep again with the timer
      // re-armed. No UI is entered, so the power button still wakes normally.
      LOG_DBG("MAIN", "Wakeup reason: Timer (dashboard refresh)");
      setupDisplayAndFonts(/*seamless=*/true);
      enterDeepSleep(/*fromTimeout=*/false, /*dashboardRefresh=*/true);  // does not return
      break;
    case HalGPIO::WakeupReason::AfterFlash:
      // After flashing, just proceed to boot
    case HalGPIO::WakeupReason::Other:
    default:
      break;
  }

  // Reboot after a firmware self-update (see maybeSelfUpdate): resume the
  // dashboard sleep loop instead of booting to the UI. The marker is consumed
  // on any boot so a stale one can never hijack a later power-button wake.
  if (Storage.exists(DASHBOARD_RESUME_MARKER)) {
    Storage.remove(DASHBOARD_RESUME_MARKER);
    if (wakeupReason != HalGPIO::WakeupReason::PowerButton) {
      LOG_INF("MAIN", "Resuming dashboard after firmware update (now " CROSSPOINT_VERSION ")");
      setupDisplayAndFonts(/*seamless=*/true);
      enterDeepSleep(/*fromTimeout=*/false, /*dashboardRefresh=*/true);  // does not return
    }
  }

  // Recovery firmware mode: hold left side button (BTN_UP) together with the power button at
  // boot to skip directly to the SD-card firmware update screen. Useful on devices where USB
  // flashing has been locked down (e.g. recent X3 firmware).
  bool recoveryFirmwareMode = false;
  if (wakeupReason == HalGPIO::WakeupReason::PowerButton) {
    // Refresh the cached button state a few times — isPressed() needs ~half a second to settle
    // after boot per the HalGPIO contract. Use a millis-based deadline so we always wait the full
    // settle window even if the loop body takes longer than expected on slow boots.
    const unsigned long settleStart = millis();
    while (millis() - settleStart < 500) {
      gpio.update();
      delay(10);
    }
    if (gpio.isPressed(HalGPIO::BTN_UP)) {
      recoveryFirmwareMode = true;
      LOG_INF("MAIN", "Recovery firmware mode (UP + POWER held at boot)");
    }
  }

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  LOG_DBG("MAIN", "Starting CrossPoint version " CROSSPOINT_VERSION);

  // Resolve the single boot-presentation decision. Skipping the splash also
  // skips the panel-clearing pass and the X3 initial-full-sync arming (see
  // HalDisplay::begin), so the first paint is FAST_REFRESH (~500ms) over the
  // retained frame and input dispatches against a visible UI.
  const BootResume resume = isSilentReboot              ? BootResume::Silent
                            : !APP_STATE.showBootScreen ? BootResume::QuickResume
                                                        : BootResume::Splash;

  setupDisplayAndFonts(resume != BootResume::Splash);

  switch (resume) {
    case BootResume::Silent:
      // Splash skipped: the routing block below picks the target activity; the
      // panel keeps showing the pre-reboot popup until that first paint lands.
      break;
    case BootResume::QuickResume:
      // One-shot flag: re-arm the splash for the next non-quick-resume boot. Save
      // before any painting so a hang in the blocking paint path can't strand
      // us in a quick-resume-with-no-frame loop on the next boot.
      APP_STATE.showBootScreen = true;
      APP_STATE.saveToFile();
      if (loadSleepFrameBuffer()) {
        // Frame restored: swap the sleep moon for the loading icon.
        const auto pageHeight = renderer.getScreenHeight();
        renderer.drawImage(LoadingIcon, 0, pageHeight - LOADINGICON_HEIGHT, LOADINGICON_WIDTH, LOADINGICON_HEIGHT);
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      } else {
        activityManager.goToBoot();  // frame file missing, fall back to the splash
      }
      break;
    case BootResume::Splash:
      activityManager.goToBoot();
      break;
  }

  if (recoveryFirmwareMode) {
    // Skip normal home/reader routing: jump straight into the SD firmware picker.
    activityManager.replaceActivity(
        std::make_unique<SdFirmwareUpdateActivity>(renderer, mappedInputManager, /*recoveryMode=*/true));
  } else if (HalSystem::isRebootFromPanic()) {
    // If we rebooted from a panic, go to crash report screen to show the panic info
    activityManager.goToCrashReport();
  } else if (resume == BootResume::Silent && snapshotTarget == SILENT_REBOOT_TARGET_READER &&
             !APP_STATE.openEpubPath.empty()) {
    activityManager.goToReader(APP_STATE.openEpubPath);
  } else if (resume == BootResume::Silent) {
    // target == home (or reader with no open book): land on home — don't fall
    // through to the sleep-wake "resume reader" logic, which fires on stale
    // openEpubPath + lastSleepFromReader from a prior session.
    activityManager.goHome();
  } else if (APP_STATE.openEpubPath.empty() || !APP_STATE.lastSleepFromReader ||
             mappedInputManager.isPressed(MappedInputManager::Button::Back) || APP_STATE.readerActivityLoadCount > 0) {
    // Boot to home screen if no book is open, last sleep was not from reader, back button is held, or reader activity
    // crashed (indicated by readerActivityLoadCount > 0)
    activityManager.goHome();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.readerActivityLoadCount++;
    APP_STATE.saveToFile();
    activityManager.goToReader(path);
  }

  if (resume == BootResume::Silent) {
    // Block until the first paint physically completes. refreshDisplay()
    // waits on the panel BUSY pin so when this returns the user can see the
    // new activity. Without the wait, an edge captured by gpio.update()
    // during boot dispatches against an invisible Home and the default
    // selectorIndex=0 opens the most-recent book.
    activityManager.requestUpdateAndWait();
    // Absorb any button held at this point into currentState as a non-edge:
    // two gpio.update() calls separated by > InputManager's 5ms debounce
    // transition the held bit through lastDebounceTime into currentState
    // without setting pressedEvents, so the first loop()'s own gpio.update()
    // sees state == currentState and emits nothing.
    gpio.update();
    delay(10);
    gpio.update();
  }

  // Ensure we're not still holding the power button before leaving setup
  waitForPowerRelease();
  allowSleepAt = millis() + 2000;
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  gpio.update();
  halTiltSensor.update(SETTINGS.tiltPageTurn, SETTINGS.orientation, activityManager.isReaderActivity());

  renderer.setFadingFix(SETTINGS.fadingFix);

  if (Serial && millis() - lastMemPrint >= 10000) {
    LOG_INF("MEM", "Free: %d bytes, Total: %d bytes, Min Free: %d bytes, MaxAlloc: %d bytes", ESP.getFreeHeap(),
            ESP.getHeapSize(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    lastMemPrint = millis();
  }

  // Handle incoming serial commands,
  // nb: we use logSerial from logging to avoid deprecation warnings
  if (logSerial.available() > 0) {
    String line = logSerial.readStringUntil('\n');
    if (line.startsWith("CMD:")) {
      String cmd = line.substring(4);
      cmd.trim();
      if (cmd == "SCREENSHOT") {
        const uint32_t bufferSize = display.getBufferSize();
        logSerial.printf("SCREENSHOT_START:%d\n", bufferSize);
        uint8_t* buf = display.getFrameBuffer();
        logSerial.write(buf, bufferSize);
        logSerial.printf("SCREENSHOT_END\n");
      }
    }
  }

  // Check for any user activity (button press or release) or active background work
  static unsigned long lastActivityTime = millis();
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || halTiltSensor.hadActivity() ||
      activityManager.preventAutoSleep()) {
    lastActivityTime = millis();         // Reset inactivity timer
    powerManager.setPowerSaving(false);  // Restore normal CPU frequency on user activity
  }

  static bool screenshotButtonsReleased = true;
  static bool screenshotComboActive = false;
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.isPressed(HalGPIO::BTN_DOWN)) {
    screenshotComboActive = true;
    if (screenshotButtonsReleased) {
      screenshotButtonsReleased = false;
      {
        RenderLock lock;
        ScreenshotUtil::takeScreenshot(renderer);
      }
    }
    return;
  }
  if (screenshotComboActive) {
    if (gpio.isPressed(HalGPIO::BTN_POWER)) return;
    if (gpio.wasReleased(HalGPIO::BTN_POWER)) {
      screenshotButtonsReleased = true;
      screenshotComboActive = false;
      return;
    }
    screenshotButtonsReleased = true;
    screenshotComboActive = false;
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (sleepTimeoutMs > 0 && millis() - lastActivityTime >= sleepTimeoutMs) {
    LOG_DBG("SLP", "Auto-sleep triggered after %lu ms of inactivity", sleepTimeoutMs);
    enterDeepSleep(true);
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  if (millis() >= allowSleepAt && gpio.isPressed(HalGPIO::BTN_POWER) &&
      gpio.getPowerButtonHeldTime() > SETTINGS.getPowerButtonDuration()) {
    // If the screenshot combination is potentially being pressed, don't sleep
    if (gpio.isPressed(HalGPIO::BTN_DOWN)) {
      return;
    }
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  // Refresh screen when power button is short-pressed with FORCE_REFRESH setting.
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::FORCE_REFRESH &&
      mappedInputManager.wasReleased(MappedInputManager::Button::Power)) {
    LOG_DBG("MAIN", "Manual screen refresh triggered");
    RenderLock lock;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  }

  // Refresh the battery icon when USB is plugged or unplugged.
  // Placed after sleep guards so we never queue a render that won't be processed.
  if (gpio.wasUsbStateChanged()) {
    activityManager.requestUpdate();
  }

  const unsigned long activityStartTime = millis();
  activityManager.loop();
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      LOG_DBG("LOOP", "New max loop duration: %lu ms (activity: %lu ms)", maxLoopDuration, activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if (activityManager.skipLoopDelay()) {
    powerManager.setPowerSaving(false);  // Make sure we're at full performance when skipLoopDelay is requested
    yield();                             // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    if (millis() - lastActivityTime >= HalPowerManager::IDLE_POWER_SAVING_MS) {
      // If we've been inactive for a while, increase the delay to save power
      powerManager.setPowerSaving(true);  // Lower CPU frequency after extended inactivity
      delay(50);
    } else {
      // Short delay to prevent tight loop while still being responsive
      delay(10);
    }
  }
}
