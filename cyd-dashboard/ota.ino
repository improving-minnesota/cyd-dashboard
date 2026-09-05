// ota.ino - Firmware OTA updates from the project's public GitHub releases.
//
// Queries the GitHub API for the latest release, compares the semver tag to the
// running kVersion, and when newer downloads the raw app .bin and flashes it to
// the inactive OTA slot with the arduino-esp32 Update library, then reboots.
//
// TLS: both the release-metadata API call and the firmware download are verified
// against a minimal root-CA bundle covering GitHub (github.com / api.github.com
// via the Sectigo chain -> USERTrust ECC, and the release asset host
// objects.githubusercontent.com via Let's Encrypt -> ISRG Root X1). If the
// bundled roots have passed their expiry (OTA_CA_EXPIRY) we fall back to
// setInsecure(true). There is NO insecure retry on a handshake failure -- that
// would let a man-in-the-middle defeat certificate validation. Firmware
// integrity is additionally pinned by comparing the streamed image's SHA-256 to
// the asset digest returned by the GitHub API.

#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "mbedtls/sha256.h"

// Minimal root CA bundle. USERTrust ECC Certification Authority verifies the
// Sectigo chain used by github.com / api.github.com; ISRG Root X1 verifies the
// Let's Encrypt chain used by objects.githubusercontent.com. Earliest root
// expires 2035-06-04 (ISRG Root X1).
static const char* const kGithubRootCAs =
  "-----BEGIN CERTIFICATE-----\n"
  "MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL\n"
  "MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n"
  "eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n"
  "JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx\n"
  "MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n"
  "Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg\n"
  "VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm\n"
  "aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo\n"
  "I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng\n"
  "o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G\n"
  "A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD\n"
  "VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB\n"
  "zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW\n"
  "RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n";

// Bundle expiry (earliest root notAfter, ISRG Root X1 = 2035-06-04). We pick a
// conservative 2035-01-01. Past this, stop trusting the bundle and use
// setInsecure(true) so updates keep working after the roots rotate out.
#define OTA_CA_EXPIRY 2051222400UL

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
  int code = -1;
  for (int attempt = 1; attempt <= HTTPS_RETRY_ATTEMPTS; attempt++) {
    http.end();                  // release the previous attempt's connection
    sec.stop();                  // close the TLS socket cleanly
    if (attempt > 1) delay(HTTPS_RETRY_DELAY_MS);
    sec = makeSecureClient();
    if (!http.begin(sec, OTA_API_URL)) continue;   // connect failed -> retry
    // addHeader() after begin(): HTTPClient clears its headers on begin/end.
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "cyd-dashboard-ota");
    code = http.GET();
    if (code >= 0) break;        // server responded (non-200): don't retry
  }
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
  if (!g_autoUpdate) return;
  time_t now = time(nullptr);
  if (now < 1600000000L) return;                 // NTP not synced yet
  unsigned long day = (unsigned long)(now / 86400UL);
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
static NetworkClientSecure makeSecureClient() {
  NetworkClientSecure c;
  // Time-gated fallback ONLY: once the bundled roots have passed OTA_CA_EXPIRY,
  // accept any cert so a root rotation can't block updates. We never retry an
  // insecure handshake after a validation failure -- that would let a MITM
  // defeat certificate verification.
  if (time(nullptr) >= (time_t)OTA_CA_EXPIRY) c.setInsecure();
  else c.setCACert(kGithubRootCAs);
  return c;
}

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
  drawOtaHeader(version);

  // The TLS connect/download to the release host can transiently fail after the
  // device has been up a while (a fresh connect to the asset host sometimes gets
  // dropped until a reboot clears the socket state). Retry the connect + GET a
  // few times with a clean teardown and a short pause between attempts rather
  // than failing immediately; a real server response (non-200, e.g. 404) is not
  // retried.
  NetworkClientSecure sec;
  HTTPClient http;
  int code = -1;
  for (int attempt = 1; attempt <= 3; attempt++) {
    http.end();                  // release the previous attempt's connection
    sec.stop();                  // close the TLS socket cleanly
    if (attempt > 1) delay(1500);
    sec = makeSecureClient();    // verified (unless roots expired)
    if (!http.begin(sec, url)) { continue; }   // connect failed -> retry
    http.setConnectTimeout(5000);   // bound the TCP connect/TLS handshake, not just the read
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    code = http.GET();
    if (code == HTTP_CODE_OK) break;
    if (code >= 0) break;        // server responded (non-200): don't retry
  }
  if (code != HTTP_CODE_OK) { http.end(); drawOtaError("Download failed"); return false; }
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

  if (haveTotal && got < (size_t)total) { Update.abort(); drawOtaError("Download failed"); return false; }

  uint8_t hash[32];
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);

  // Enforce the digest only when GitHub supplied one; if the response carried no
  // digest we skip the check so a digest-less asset can't brick the update.
  if (!expectedSha256.isEmpty() && !sha256Matches(hash, expectedSha256)) {
    Update.abort();
    drawOtaError("Checksum mismatch");
    return false;
  }

  if (!Update.end()) { drawOtaError("Flash failed"); return false; }
  drawOtaRestart();
  delay(500);
  ESP.restart();
  return true;
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
