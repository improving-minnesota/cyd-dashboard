// ota.ino - Firmware OTA updates from the project's public GitHub releases.
//
// Queries the GitHub API for the latest release, compares the semver tag to the
// running kVersion, and when newer downloads the raw app .bin and flashes it to
// the inactive OTA slot with the arduino-esp32 Update library, then reboots.
//
// TLS: both the release-metadata API call and the firmware download are verified
// against the global trust store (kRootCAs, see cyd-dashboard.ino). If the
// bundled roots have passed their expiry (OTA_CA_EXPIRY) the OTA path falls back
// to setInsecure(true); data fetches never do. There is NO insecure retry on a
// handshake failure -- that would let a man-in-the-middle defeat certificate
// validation. Firmware integrity is additionally pinned by comparing the
// streamed image's SHA-256 to the asset digest returned by the GitHub API.

#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "mbedtls/sha256.h"

// OTA verifies TLS against the global trust store (kRootCAs in
// cyd-dashboard.ino), with a time-gated setInsecure() fallback once those
// roots expire (see OTA_CA_EXPIRY) so a root rotation can't block updates.

#define OTA_REPO    "improving-minnesota/cyd-dashboard"
#define OTA_ASSET   "cyd-dashboard.ino.bin"
#define OTA_API_URL "https://api.github.com/repos/" OTA_REPO "/releases/latest"

// ---- semver helpers -------------------------------------------------------
int compareVersions(const String& a, const String& b) {
  int ai = 0, bi = 0;
  while (ai < (int)a.length() || bi < (int)b.length()) {
    int av = 0, bv = 0;
    while (ai < (int)a.length() && a[ai] != '.') av = av * 10 + (a[ai++] - '0');
    ai++;  // skip '.'
    while (bi < (int)b.length() && b[bi] != '.') bv = bv * 10 + (b[bi++] - '0');
    bi++;
    if (av != bv) return av > bv ? 1 : -1;
  }
  return 0;
}
String stripV(const String& s) { return (s.length() && s[0] == 'v') ? s.substring(1) : s; }
// Strip any non-numeric suffix (e.g. the "-dev" in a local build's version)
// before comparing: compareVersions() reads each dot-separated component as
// consecutive digit characters, so a trailing "-dev" on the last numeric
// component would otherwise be parsed as digits and corrupt the comparison.
String bareVersion(const String& s) {
  int i = 0;
  while (i < (int)s.length() && (isDigit(s[i]) || s[i] == '.')) i++;
  return s.substring(0, i);
}
bool isNewerThanRunning(const String& tagVersion) {
  String tag = stripV(tagVersion);
  int c = compareVersions(tag, bareVersion(kVersion));
  if (c > 0) return true;
  // A release is also an upgrade over a -dev build of the same version
  // (e.g. 1.2.4 offered to a device running 1.2.4-dev), so the device can move
  // off a dev build onto the equivalent release build.
  return c == 0 && strstr(kVersion, "-dev") != NULL;
}

// ---- GitHub latest-release fetch -----------------------------------------
bool fetchLatestRelease(String& versionOut, String& assetUrlOut, String& sha256Out) {
  if (WiFi.status() != WL_CONNECTED) return false;
  // Retry a transient TLS connect/GET (same class of failure as performOTA, so
  // an update check that drops after prolonged uptime doesn't fail immediately).
  NetworkClientSecure sec;
  HTTPClient http;
  http.setConnectTimeout(5000);   // bound the TCP connect/TLS handshake, not just the read
  http.setTimeout(10000);
  // Shared verified-TLS helper; allowInsecure only falls back to setInsecure()
  // past OTA_CA_EXPIRY so a root rotation can't block updates.
  const char* ghHdrs[] = { "Accept", "application/vnd.github+json", nullptr };
  int code = httpsRequestRetry(http, sec, OTA_API_URL, HTTPS_METHOD_GET, "", ghHdrs, /*allowInsecure=*/true);
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  const char* tag = doc["tag_name"] | "";
  if (!tag || !tag[0]) return false;
  String url, digest;
  for (JsonObject a : doc["assets"].as<JsonArray>()) {
    if (String((const char*)(a["name"] | "")) == OTA_ASSET) {
      url = (const char*)(a["browser_download_url"] | "");
      digest = (const char*)(a["digest"] | "");   // "sha256:<hex>"; "" if GitHub omitted it
      break;
    }
  }
  if (url.length() == 0) return false;
  versionOut = String(tag);
  assetUrlOut = url;
  sha256Out = digest;
  return true;
}

// Populate the About-page update state from the GitHub API. Runs on the net
// task so it never blocks drawing.
void checkForUpdate() {
  String ver, url, digest;
  if (!fetchLatestRelease(ver, url, digest)) { g_updateState = 4; return; }   // error
  if (isNewerThanRunning(ver)) {
    g_updateState = 2; g_updateLatest = stripV(ver); g_updateAsset = url; g_updateDigest = digest;
  } else {
    g_updateState = 3;                                                 // none
  }
}

// ---- daily auto-update scan ----------------------------------------------
// Called once after WiFi + NTP sync on boot. Scans at most once per calendar
// day (tracked in NVS); on a newer version with Auto-Update ON, starts the OTA.
void maybeAutoUpdate() {
  time_t now = time(nullptr);
  unsigned long day = (unsigned long)(now / 86400UL);
  if (!g_autoUpdate) return;
  if (now < 1600000000L) return;                 // NTP not synced yet
  if (g_lastScanDay == day) return;              // already scanned today
  // Only announce "Scanning" once we know a network check will actually
  // happen (all the skip-checks above have passed). The deadline generously
  // covers fetchLatestRelease()'s own timeouts (5s connect + 10s read) so the
  // status can never outlive the scan it describes.
  g_autoUpdStatus = 1; g_autoUpdStatusUntil = millis() + 20000;   // Scanning...
  String ver, url, digest;
  if (!fetchLatestRelease(ver, url, digest)) {
    g_autoUpdStatus = 4; g_autoUpdStatusUntil = millis() + 4000;   // Check Failed
    return;                                      // transient; try next boot
  }
  g_lastScanDay = day;                           // only mark after a good scan
  prefs.begin("flight", false); prefs.putULong("lastscan", day); prefs.end();
  if (isNewerThanRunning(ver)) {
    g_autoUpdStatus = 3; g_autoUpdStatusUntil = millis() + 4000;   // Updating...
    g_otaVersion = stripV(ver);
    g_otaUrl = url;
    g_otaSha256 = digest;
    g_otaActive = true;                          // loop() performs the update
  } else {
    g_autoUpdStatus = 2; g_autoUpdStatusUntil = millis() + 4000;   // No Updates
  }
}

// Runs the daily auto-update scan on the net task (NOT the main loop). The
// synchronous GitHub TLS fetch + JSON parse overflows the small loopTask stack,
// and running it on the loop would also freeze the UI. Waits briefly for NTP
// time first so the once-per-day clock is meaningful.
void autoScanOnce() {
  unsigned long t0 = millis();
  while (time(nullptr) < 1600000000L && millis() - t0 < 8000) delay(100);
  maybeAutoUpdate();
}

// ---- OTA execution --------------------------------------------------------
// Last-drawn progress-bar fill width, so drawOtaProgress() can redraw only the
// newly-grown segment instead of erasing + redrawing the whole bar (which
// caused visible flicker while the bar grew).
static int s_otaFill = 0;

void drawOtaHeader(const String& version) {
  s_otaFill = 0;
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY); tft.setTextFont(2);
  tft.setCursor(8, 6); tft.print("Updating");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK); tft.setTextFont(2);
  tft.setCursor(8, 52); tft.print("Updating to v" + version + "...");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(8, 84); tft.print("Do not power off device");
  tft.drawRect(10, 120, 300, 22, TFT_WHITE);
}

void drawOtaProgress(int total, size_t got) {
  if (total <= 0) return;
  int fill = (int)((long)got * 296 / total); if (fill > 296) fill = 296;
  if (fill < s_otaFill) fill = s_otaFill;
  if (fill > s_otaFill) {
    tft.fillRect(12 + s_otaFill, 122, fill - s_otaFill, 18, TFT_GREEN);
    s_otaFill = fill;
  }
  char pct[16]; snprintf(pct, sizeof pct, "%d%%", (int)((long)got * 100 / total));
  tft.fillRect(80, 146, 160, 18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextFont(2);
  tft.setCursor(120, 148); tft.print(pct);
}

void drawOtaRestart() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextFont(2);
  tft.setCursor(60, 110); tft.print("Restarting...");
}

void drawOtaError(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextFont(2);
  tft.setCursor(20, 100); tft.print("Update Failed");
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK); tft.setTextFont(1);
  tft.setCursor(20, 124); tft.print(msg);
  delay(3000);
}

// Download + flash. Runs on a dedicated task with a large stack (see
// otaTaskEntry) because the mbedtls TLS handshake overflows the small loop task.
// Owns the display. Never returns on success (reboots). Returns false only after
// showing an error screen.
static bool sha256Matches(const uint8_t hash[32], const String& expected) {
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(&hex[i * 2], 3, "%02x", hash[i]);
  hex[64] = 0;
  String exp = expected;
  if (exp.startsWith("sha256:")) exp = exp.substring(7);
  exp.trim();
  return exp.equalsIgnoreCase(hex);
}

bool performOTA(const String& url, const String& version, const String& expectedSha256) {
  // The TLS connect/download to the release host can transiently fail after the
  // device has been up a while (a fresh connect to the asset host sometimes gets
  // dropped until a reboot clears the socket state). We therefore retry the
  // WHOLE download -- connect + GET + stream + checksum -- a few times with a
  // clean socket teardown and a short pause between attempts. Retrying just the
  // GET (as this code used to) left the streaming body unguarded: if the fresh
  // connection dropped mid-download the update failed immediately even though a
  // reboot (or simply another try) would have succeeded. The drop can surface as
  // a failed connect, a non-200, or a truncated body on any attempt, so every
  // stage is retried; a genuine 404 merely costs a couple of extra attempts
  // before failing.
  const char* failReason = "Download failed";
  for (int attempt = 1; attempt <= HTTPS_RETRY_ATTEMPTS; attempt++) {
    drawOtaHeader(version);          // reset screen + progress bar each attempt
    if (attempt > 1) delay(HTTPS_RETRY_DELAY_MS);

    NetworkClientSecure sec;
    HTTPClient http;
    http.setUserAgent(appUserAgent());   // persistent across begin()/end()
    // Shared verified-TLS helper; allowInsecure falls back past OTA_CA_EXPIRY.
    if (!httpsBegin(http, sec, url.c_str(), /*allowInsecure=*/true)) continue;   // connect failed -> retry
    http.setConnectTimeout(5000);        // bound the TCP connect/TLS handshake, not just the read
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); continue; }   // dropped connect/non-200 -> retry

    int total = http.getSize();
    bool haveTotal = (total > 0);
    if (!Update.begin(haveTotal ? total : OTA_SIZE_UNKNOWN, U_FLASH)) { http.end(); drawOtaError("Flash failed"); return false; }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient* stream = http.getStreamPtr();
    size_t got = 0;
    uint8_t buf[4096];
    int lastPct = -1;
    bool done = false;
    while (!done) {
      if (stream->available() == 0) {
        if (!http.connected()) { done = true; break; }
        delay(1); continue;
      }
      int n = stream->readBytes(buf, min(sizeof buf, (size_t)stream->available()));
      if (n <= 0) { done = true; break; }
      mbedtls_sha256_update(&sha, buf, n);
      Update.write(buf, n); got += n;
      // No esp_task_wdt_reset() here: performOTA runs on its own task that is not
      // watchdog-subscribed, and the HTTP timeouts bound the download.
      int pct = haveTotal ? (int)((long)got * 100 / total) : -1;
      if (pct != lastPct && pct >= 0) { lastPct = pct; drawOtaProgress(total, got); }
      if (haveTotal && got >= (size_t)total) done = true;
    }
    http.end();

    if (haveTotal && got < (size_t)total) { Update.abort(); failReason = "Download failed"; continue; }   // dropped mid-stream -> retry

    uint8_t hash[32];
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    // Enforce the digest only when GitHub supplied one; if the response carried no
    // digest we skip the check so a digest-less asset can't brick the update.
    if (!expectedSha256.isEmpty() && !sha256Matches(hash, expectedSha256)) {
      Update.abort(); failReason = "Checksum mismatch"; continue;   // corrupted transfer -> retry
    }

    if (!Update.end()) { drawOtaError("Flash failed"); return false; }
    drawOtaRestart();
    delay(500);
    ESP.restart();
    return true;
  }
  Serial.printf("[OTA] update failed: %s\n", failReason);   // one line for post-mortem diagnosis
  drawOtaError(failReason);
  return false;
}

// ---- rollback safeguard ---------------------------------------------------
// Called once after a successful boot grace period: cancels any pending
// rollback so a freshly-OTA'd slot stays active.
void markAppValidBoot() {
  esp_ota_img_states_t st;
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running && esp_ota_get_state_partition(running, &st) == ESP_OK) {
    if (st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
    }
  }
}
