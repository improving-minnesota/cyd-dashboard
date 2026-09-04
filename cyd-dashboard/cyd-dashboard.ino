// cyd-dashboard: standalone dashboard for the ESP32-2432S028
// (2.8" ILI9341 TFT with XPT2046 resistive touch).
//
// Fetches live aircraft positions from the OpenSky API over WiFi, filters
// for planes near/overhead your location, draws a dashboard on the TFT, and
// lets you adjust the distance radius and altitude ceiling from a touch
// settings screen (persisted to NVS so they survive reboots).
//
// SETUP:
//   1. WiFi/OpenSky/Govee credentials are provisioned to NVS over USB serial
//      (see README + wifi_config.ino); no credentials are compiled in.
//   2. The TFT/touch pins come from TFT_eSPI User_Setups/
//      Setup_ESP32_2432S028_CYD.h (correct pinout for the 2432S028R: TFT on
//      HSPI, XPT2046 touch on VSPI, touch IRQ on GPIO 36).
//
// Display rotates to landscape 320x240.

#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// Airline logos are loaded at runtime from a dedicated LittleFS partition (see
// logos.ino), never compiled into the firmware. Declare the type and symbols
// here so every .ino file can use them regardless of Arduino's alphabetical
// concatenation order.
struct RuntimeLogo { char icao[4]; uint16_t* data; int w; int h; };
bool logosInit();
const RuntimeLogo* findAirlineLogo(const char* callsign);
void logoRelease(const RuntimeLogo* logo);
// Forward-declare Plane so Arduino's auto-generated function prototypes (which
// are inserted before the sketch body) can reference drawFlightInfo(Plane& p).
struct Plane;

// Serial NVS provisioning (see wifi_config.ino). Defined HERE (not in
// wifi_config.ino) because Arduino concatenates .ino files alphabetically, and
// this file uses the macro before wifi_config.ino is reached. Set to 0 to
// strip out the temporary provisioning listener. Guarded with #ifndef so the
// build can enable it with -DENABLE_SERIAL_PROVISION=1 (an unconditional
// #define here would otherwise silently override that command-line flag).
#ifndef ENABLE_SERIAL_PROVISION
#define ENABLE_SERIAL_PROVISION 0
#endif

// Pool temperature feature (Govee). Reads the selected thermometer's current
// temperature via the Govee Open API POST /router/api/v1/device/state
// (developer.govee.com) and logs it to flash for the history graphs. Whether
// this board's H5310 actually appears in the /user/devices list depends on
// Govee's developer-API support for that model; if it isn't listed (or a fetch
// fails), the pool value just shows "--" and a red border - a graceful
// fallback, not a crash. Set to 1 to enable the settings menu entry, idle
// display, history graph, and polling.
#define POOL_FEATURE 1

// Deep-sleep wake interval while inside the sleep window. The device wakes
// briefly to log the pool temperature, then returns to low-power deep sleep.
#define SLEEP_POOL_INTERVAL_US (5ULL * 60ULL * 1000000ULL)   // 5 minutes

// ---- Touch-wake from deep sleep ----
// VERIFIED on this unit (see the touch-irq-test sketch): the XPT2046 touch IRQ
// is on GPIO 36 (active-low: HIGH idle, LOW on touch) and wakes the chip from
// deep sleep via EXT0. For the wake to fire, the touch controller must stay
// selected during sleep, so enterDeepSleep() holds its CS line low.
// GPIO 36 is input-only (no internal pull-up), so no INPUT_PULLUP is used.
#define TOUCH_IRQ_ENABLED 1
#define TOUCH_IRQ_PIN     36
#define TOUCH_CS_PIN      33

// ---- XPT2046 touch on VSPI (separate bus from the TFT's HSPI) ----
// TFT_eSPI's getTouch() reads the touch controller on the SAME SPI bus as the
// display, so it cannot talk to the XPT2046 here (it lives on VSPI). We drive
// it directly over VSPI instead; see touchReadXY().
#define TOUCH_SPI VSPI
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// Print raw + mapped touch coordinates to USB serial on every press (and the
// calibration params at boot). Enable temporarily to diagnose touch/panel
// calibration. Set to 0 to remove the prints.
#define TOUCH_DEBUG 0

// ---------------- CONFIG (edit these) ----------------
// OpenSky bbox will be computed from g_lat/g_lon at runtime.
const int OPENSKY_TOTAL_CREDITS = 4000;
// Build/version shown on the About page. CI overrides APP_VERSION at build
// time with the release version via -DAPP_VERSION=<ver> (see
// .github/workflows/release.yml). Local/dev builds should pass
// -DAPP_VERSION="$(cat version.txt)-dev" (see DEVELOPER.md) so About shows an
// accurate version derived from version.txt; the literal below is only a
// fallback for builds that don't set the flag (e.g. the Arduino IDE) and can
// drift out of date - it exists solely so any "-dev"-suffixed string is
// present for isDevBuild() to detect. A "-dev" build never auto-updates (see
// g_autoUpdate handling).
#define STRINGIZE_INNER(x) #x
#define STRINGIZE(x) STRINGIZE_INNER(x)
#ifndef APP_VERSION
  #define APP_VERSION "0.0.0-dev"
  const char* const kVersion = APP_VERSION;
#else
  const char* const kVersion = STRINGIZE(APP_VERSION);
#endif
bool isDevBuild() { return strstr(kVersion, "-dev") != NULL; }
// ------------------------------------------------------

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// A busy airspace's /states/all response has no fixed size cap, and
// ArduinoJson's default JsonDocument grows on the heap without limit. This
// allocator caps how much memory a single parse can use, so a large response
// fails the parse cleanly (handled like any other JSON error) instead of
// risking exhausting the heap.
class BoundedAllocator : public ArduinoJson::Allocator {
 public:
  explicit BoundedAllocator(size_t maxBytes) : limit_(maxBytes), used_(0) {}
  void* allocate(size_t size) override {
    if (used_ + size > limit_) return nullptr;
    void* p = malloc(size);
    if (p) used_ += size;
    return p;
  }
  void deallocate(void* p) override { free(p); }
  void* reallocate(void* p, size_t newSize) override { return realloc(p, newSize); }
 private:
  size_t limit_, used_;
};

struct Plane {
  char callsign[9];
  char icao24[7];   // transponder hex ID, used for route lookups
  float distMi;
  int   altFt;
  int   spdKt;
  float dxMi;   // east  offset in miles
  float dyMi;   // north offset in miles
};

const int MAXP = 6;
Plane planes[MAXP];
int planeCount = 0;

// Snapshot of the most recent overhead flight, kept so the "N aircraft" tap on
// the dashboard can recall the same details that were last shown even after the
// plane has left the radius. Filled in the flight-processing block whenever a
// plane is overhead; valid=false until the first overhead flight is recorded.
struct FlightSnap {
  bool  valid;            // false until at least one overhead flight is recorded
  char  callsign[9];
  char  icao24[7];
  int   altFt;
  int   spdKt;
  float distMi;
  float dxMi;             // east offset (for the radar blip)
  float dyMi;             // north offset (for the radar blip)
  char  origin[6];        // route ICAO codes ("" = unknown)
  char  dest[6];
  bool  routeFetched;
};
FlightSnap g_lastFlight;
unsigned long g_flightDetailUntil = 0;   // millis() at which the detail page auto-returns

// Settings (persisted)
float g_radiusMi = 3.5;
int   g_ceilingFt = 15000;
int   g_pollSec = 60;
bool  g_trackEnabled = true;   // flight tracking on/off
bool  g_blinkForFlight = true; // flash the LED when a noteworthy flight is overhead
bool  g_metric = false;        // false = imperial (ft/mi/mph), true = metric (m/km/kts)
bool  g_showTimer = false;     // show/update the dashboard countdown bar (Flight Tracker)
bool  g_autoUpdate = true;     // auto-check/install firmware updates once/day (General)
unsigned long g_lastScanDay = 0; // epoch day of last auto-update scan (0 = never)

// OTA / firmware update state
int    g_updateState = 0;      // 0 idle, 1 checking, 2 available, 3 none, 4 error
String g_updateLatest;         // latest version label when an update is available
String g_updateAsset;          // download URL when an update is available
String g_updateDigest;         // "sha256:<hex>" of the available asset ("" if absent)
bool   g_otaActive = false;    // loop() should run the pending OTA
String g_otaVersion, g_otaUrl; // pending OTA target
String g_otaSha256;            // digest of the pending OTA target
bool   g_rollbackMarked = false; // OTA rollback safeguard applied once post-boot
TaskHandle_t g_otaTask = NULL;   // dedicated task running performOTA
volatile bool g_otaRunning = false; // OTA task owns the display; loop() yields
// Auto-update status shown at the bottom-left of the dashboard (reuses the
// idle screen's status line, see drawAutoUpdateStatus()).
int    g_autoUpdStatus = 0;       // 0 none, 1 scanning, 2 no updates, 3 updating, 4 check failed
unsigned long g_autoUpdStatusUntil = 0; // millis() deadline to keep showing the transient status
int   g_ftPage = 0;            // Flight Tracker settings page (0 or 1)
float g_lat = 0.0f;           // location; loaded from NVS, or guessed from IP on first boot
float g_lon = 0.0f;
int   g_creditsRemaining = 0; // OpenSky X-Rate-Limit-Remaining
bool  g_creditsKnown = false; // false until the first successful fetch
bool  g_creditsExhausted = false;  // true when at/near the daily credit limit
const int LOW_CREDIT_THRESHOLD = 0;         // "at the limit"
const unsigned long CREDIT_RECOVERY_MS = 15UL * 60UL * 1000UL;  // retry cadence while exhausted

// OpenSky auth state for the red border indicator
enum AuthState { AUTH_OK = 0, AUTH_ANON, AUTH_BAD };
int g_authState = AUTH_ANON;   // defaults to anonymous (blank creds)
bool g_authChecked = false;    // true after the first OpenSky fetch attempt
// True when an OpenSky TLS handshake/cert validation failed (transport-level
// error, negative HTTPClient code). Used to turn such failures into a hard
// error instead of silently falling back to anonymous.
bool g_osHandshakeFailed = false;

// Sleep Mode settings (persisted)
bool g_sleepOn = true;
int  g_sleepStartH = 22, g_sleepStartM = 0;   // default 10:00 PM
int  g_sleepEndH   = 8,  g_sleepEndM   = 0;   // default 8:00 AM
int  g_wakeMin     = 10;                        // wake duration after a touch

// Sleep runtime state
bool g_displayOff = false;
unsigned long wakeUntil = 0;                  // 0 = not in a user-triggered wake

// Pool Temp / Govee integration (persisted + runtime)
String g_goveeKey = "";       // Govee Open API key
String g_poolDeviceId = "";   // selected thermometer device id (MAC)
String g_poolModel = "";      // selected device model, e.g. "H5075"
String g_poolName = "";       // selected device name
struct GoveeDev { char id[64]; char model[16]; char name[40]; };
#define MAX_GOVEE 10
GoveeDev g_goveeDevs[MAX_GOVEE];
int g_goveeCount = 0;         // how many thermometers found on the account
int g_goveeSel = 0;           // selected index
float g_poolTemp = 0;         // latest temperature (F)
char  g_poolUnit = 'F';       // unit of the fetched value
bool  g_poolValid = false;    // true when a temp was fetched successfully
bool  g_poolEnabled = false;  // Pool Temp feature on/off (defaults off)
unsigned long g_lastPool = 0;
const unsigned long POOL_REFRESH_MS = 300UL * 1000UL;  // 5 min (respect Govee API limits)

// Pool temp history: tiered storage so Month/Year graphs have real range without
// unbounded memory/flash use.
//   - raw (5-min samples): covers Day/Week views, ~8.7 days retained
//   - hourly rollups (avg per hour): covers Month view, ~33 days retained
//   - daily rollups (avg per day): covers Year view, >1 year retained
#define MAX_POOL_LOG 2048
float g_poolLogTemp[MAX_POOL_LOG];
unsigned long g_poolLogTime[MAX_POOL_LOG];
int g_poolLogNext = 0;
int g_poolLogCount = 0;

#define MAX_POOL_HOUR 800
float g_poolHourTemp[MAX_POOL_HOUR];
unsigned long g_poolHourTime[MAX_POOL_HOUR];
int g_poolHourNext = 0;
int g_poolHourCount = 0;
long g_curHourBucket = -1;   // epoch/3600 of the in-progress hour
float g_curHourSum = 0;
int   g_curHourN = 0;

#define MAX_POOL_DAY 400
float g_poolDayTemp[MAX_POOL_DAY];
unsigned long g_poolDayTime[MAX_POOL_DAY];
int g_poolDayNext = 0;
int g_poolDayCount = 0;
long g_curDayBucket = -1;    // epoch/86400 of the in-progress day
float g_curDaySum = 0;
int   g_curDayN = 0;

// Pool graph screen state
enum PoolTF { TF_DAY, TF_WEEK, TF_MONTH, TF_YEAR };
int g_poolTF = TF_WEEK;
unsigned long g_graphUntil = 0;   // 0 = graph not showing

String g_savedSsid = "";   // WiFi loaded from NVS
String g_savedPass = "";
String g_osClientId = "";       // OpenSky OAuth2 client id (blank = anonymous)
String g_osClientSecret = "";   // OpenSky OAuth2 client secret
String g_osToken = "";          // cached OpenSky bearer token
unsigned long g_osTokenExpiry = 0;  // millis() at which g_osToken expires
bool   g_osTokenValid = false;
String g_addrSearch = "";  // address search buffer for location geocoding
String g_lastPlace = "";   // human-readable place name from the last successful geocode
// Address-search result code shown on the search status screen (wifiSub 12):
// "empty", "no-match", or a friendly human-readable error message ("" = none).
String g_addrErr = "";
String g_latLonStr = "";   // "lat,lon" edit buffer for the Location page
String g_sleepStartStr = "2200";  // HHMM for editing in settings
String g_sleepEndStr   = "0800";
String g_wakeStr       = "10";

enum Screen { SCR_DASH, SCR_SETTINGS, SCR_GENERAL, SCR_ABOUT, SCR_HELP, SCR_WIFI, SCR_RESET, SCR_SLEEP, SCR_FTRACKER, SCR_POOL, SCR_POOLGRAPH, SCR_LOCATION, SCR_CALIB, SCR_FLIGHTDETAIL };
Screen g_screen = SCR_DASH;
int g_helpScroll = 0;   // Help page vertical scroll offset (px)
int g_resetConfirm = 0;  // Reset screen sub-state: 0=choose, 1=confirm All, 2=confirm Settings

extern int g_wifiSub;   // defined in wifi_config.ino

// ---- Touch calibration state (see calibration.ino) ----
// Defined here (not calibration.ino) because Arduino concatenates .ino files
// alphabetically, and the touch code in this file uses these before
// calibration.ino would be reached.
enum CalState { CAL_NONE, CAL_LOADING, CAL_TARGET, CAL_DONE };
CalState g_calState = CAL_NONE;
// Factory defaults, measured on this unit from a full-range read sweep:
//   rawY 592..3678 -> dispX 0..320, rawX 379..3396 -> dispY 0..240
// disp = (raw - offset) * 1000 / scale. Overridden by the on-device
// calibration (saved to NVS) if the user runs it.
int  g_calScaleX = 9646;   // (3678-592)*1000/320
long g_calOffX   = 592;    // rawY offset
int  g_calScaleY = 12571;  // (3396-379)*1000/240
long g_calOffY   = 379;    // rawX offset
unsigned long g_calLongPressStart = 0;  // millis() when a press began (any screen)

bool dirty = true;      // force a redraw
bool connected = false;
char lastErr[40] = "connecting...";

unsigned long lastPoll = 0;
unsigned long lastClockDraw = 0;
unsigned long g_lastWeather = 0;
const unsigned long WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;

// First WiFi connect after boot: refresh everything needing the network right
// away instead of waiting for the poll/countdown timers (matters when WiFi is
// slow to connect, since setup() couldn't fetch anything).
bool g_wifiConnectedOnce = false;   // true once WiFi has been up since boot
bool g_bootFetched = false;         // true if setup() already ran the boot fetches
bool g_firstBoot = false;           // true on first boot (no saved location yet)

// First-boot wizard: if no touch calibration is saved we run it at boot, then
// if no WiFi credentials are saved we gather those. BOOT_DONE means normal boot
// (straight to the dashboard). This runs once per power-up; setup() decides the
// starting stage and the calibration/wifi completion paths advance it.
enum BootStage { BOOT_NONE, BOOT_CALIB, BOOT_WIFI, BOOT_DONE };
BootStage g_bootStage = BOOT_NONE;

// Non-blocking WiFi reconnect state
bool wifiTrying = false;
unsigned long wifiTryStart = 0;

// When set, the dashboard keeps showing the idle screen even if a flight is
// overhead (user dismissed it via the countdown bar). Cleared when a new
// overhead flight is found.
bool g_suppressFlight = false;

// One-shot LED flash queued when an overhead flight is found. Performed in
// loop() AFTER the flight view is drawn, so the user sees the data first and
// then the flash notification (not the other way around).
bool g_pendingFlash = false;
bool g_flashDeparting = false;
bool g_flashIncoming  = false;
bool g_flashTop50     = false;
bool g_flashWhite     = false;

// Route details for the overhead flight (origin/destination), fetched lazily
// only when the user taps "Details" and cached per plane. Defined in
// flight_details.ino.
String g_routeOrigin = "";      // "KDAL"
String g_routeDest   = "";      // "KDFW"
bool   g_routeFetched = false;  // true once we've tried (success or not)
volatile bool g_routeBusy = false;  // true while a route fetch is in flight (cross-task)
String g_homeAirport = "";      // home airport to hide from route data ("" = show all)

// ---- small helpers ----
float hav(float lat1, float lon1, float lat2, float lon2) {
  const float R = 3958.8f; // miles
  float p1 = lat1 * PI / 180.0f, p2 = lat2 * PI / 180.0f;
  float dp = (lat2 - lat1) * PI / 180.0f;
  float dl = (lon2 - lon1) * PI / 180.0f;
  float a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  return 2 * R * asin(sqrt(a));
}

void sortPlanes() {
  // insertion sort by distance (ascending)
  for (int i = 1; i < planeCount; i++) {
    Plane tmp = planes[i];
    int j = i - 1;
    while (j >= 0 && planes[j].distMi > tmp.distMi) { planes[j + 1] = planes[j]; j--; }
    planes[j + 1] = tmp;
  }
}

// ---- WiFi ----
bool tryConnect(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ---- OpenSky OAuth2 client-credentials auth (Basic auth removed March 2026) ----
const char* kOsTokenUrl = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
const int   kOsTokenLifetimeMs = 30 * 60 * 1000;   // OpenSky tokens expire after ~30 min

// Root CA bundle used to verify TLS for every ISRG/Let's Encrypt-signed host
// this firmware talks to: OpenSky (auth.opensky-network.org,
// opensky-network.org) and open-meteo (api.open-meteo.com). All currently
// chain up to ISRG Root X1 (leaf <- Let's Encrypt intermediate (e.g. YR1/YR2)
// <- Root YR <- ISRG Root X1) and send their full intermediate chain, so a
// root-only trust anchor suffices. ISRG Root X2 is also included as a second
// Let's Encrypt root for future-proofing if any host rotates to an ECDSA
// chain. Earliest root expires 2035-06-04 (ISRG Root X1). Unlike the OTA
// bundle there is deliberately NO setInsecure() fallback: a failed validation
// is a hard error so a MITM can't silently downgrade auth.
static const char* const kISRGRootCAs =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
  "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
  "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
  "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
  "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
  "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
  "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
  "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
  "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
  "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
  "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
  "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
  "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
  "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
  "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
  "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
  "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
  "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
  "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
  "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
  "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
  "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
  "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
  "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
  "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
  "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
  "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
  "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
  "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
  "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
  "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
  "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
  "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
  "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
  "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
  "/q4AaOeMSQ+2b1tbFfLn\n"
  "-----END CERTIFICATE-----\n";

// Root CA bundle for hosts NOT served by Let's Encrypt. Govee's Open API
// (openapi.api.govee.com) chains leaf <- Amazon RSA 2048 M04 <- Amazon Root
// CA 1 and sends its full intermediate chain, so a root-only trust anchor
// suffices. Amazon Root CA 1 expires 2038-01-17. No insecure fallback, same
// as kISRGRootCAs.
static const char* const kAmazonRootCA1 =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
  "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
  "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
  "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
  "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
  "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
  "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
  "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
  "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
  "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
  "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
  "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
  "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
  "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
  "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
  "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
  "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
  "rqXRfboQnoZsG4q5WTP468SQvvG5\n"
  "-----END CERTIFICATE-----\n";

// Shared helper: attach the given root bundle to a NetworkClientSecure and
// start a verified-TLS request with the passed HTTPClient. Returns false when
// the client/URL can't be set up (caller should treat as a hard error). The
// caller must keep `sec` alive for the lifetime of the request.
bool httpsBegin(HTTPClient& http, NetworkClientSecure& sec, const char* url, const char* roots) {
  sec.setCACert(roots);
  return http.begin(sec, url);
}

// Percent-encode a string for an application/x-www-form-urlencoded body.
String urlEncode(const String& s) {
  String out;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof buf, "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

// Ensure g_osToken holds a valid bearer token, fetching one from the OpenSky
// token endpoint when needed. Returns true when authenticated. Returns false
// when no client is configured (fall back to anonymous) OR the token exchange
// failed (client credentials are invalid - callers should flag AUTH_BAD).
bool openskyEnsureToken() {
  g_osHandshakeFailed = false;   // fresh for each call (cached-token path keeps it false)
  if (g_osTokenValid && g_osToken.length() > 0 && millis() < g_osTokenExpiry) {
    return true;
  }
  g_osToken = "";
  g_osTokenValid = false;
  if (g_osClientId.length() == 0 || g_osClientSecret.length() == 0) {
    return false;  // no client configured -> anonymous
  }
  // TLS is verified against the bundled ISRG roots. Unlike the OTA path there
  // is no insecure fallback, so a handshake/validation failure (e.g. expired
  // CA or a MITM) is a hard error, never a silent downgrade to anonymous.
  NetworkClientSecure sec;
  sec.setCACert(kISRGRootCAs);
  HTTPClient http;
  if (!http.begin(sec, kOsTokenUrl)) { g_osHandshakeFailed = true; return false; }
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = "grant_type=client_credentials&client_id=" + urlEncode(g_osClientId) +
                "&client_secret=" + urlEncode(g_osClientSecret);
  int code = http.POST(body);
  String payload = http.getString();
  http.end();
  if (code < 0) { g_osHandshakeFailed = true; return false; }  // TLS/transport failure
  if (code != HTTP_CODE_OK) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  const char* tok = doc["access_token"];
  if (!tok || !tok[0]) return false;
  g_osToken = tok;
  int expiresIn = doc["expires_in"] | (kOsTokenLifetimeMs / 1000);
  int margin = (expiresIn > 30) ? 30 : 5;   // refresh shortly before expiry
  g_osTokenExpiry = millis() + (unsigned long)(expiresIn - margin) * 1000UL;
  g_osTokenValid = true;
  return true;
}

// ---- Async network task ----
// Network I/O (HTTPClient) is synchronous and can block for seconds while a
// fetch times out, which freezes the main loop (touch + drawing). So network
// fetches run on a separate FreeRTOS task on the other core; the loop only sets
// a flag and stays responsive. One fetch runs at a time.
volatile bool netWantFlights      = false;
volatile bool netWantWeather      = false;
volatile bool netWantLocation     = false;
volatile bool netWantPool         = false;   // refresh the current pool temp
volatile bool netWantPoolDevices  = false;   // list Govee thermometers
volatile bool netWantUpdateCheck  = false;   // query GitHub for the latest release
volatile bool netWantAutoScan     = false;   // daily auto-update scan (runs off the loop task)
volatile bool netBusy             = false;   // a fetch is currently running
volatile bool netUpdated          = false;   // set when a fetch finishes

void netTask(void* p) {
  for (;;) {
    vTaskDelay(30 / portTICK_PERIOD_MS);
    if (WiFi.status() != WL_CONNECTED) continue;
    // While an OTA is downloading, pause all net fetches so the OTA task's HTTP
    // doesn't overlap with ours (concurrent lwIP use can trigger a FreeRTOS
    // xTaskPriorityDisinherit assert).
    if (g_otaRunning) { vTaskDelay(30); continue; }
    // User-initiated update check gets priority so the About page responds
    // quickly even while background fetches (flights/weather/pool) are queued.
    if (netWantUpdateCheck) { netWantUpdateCheck=false; netBusy=true; checkForUpdate(); netBusy=false; netUpdated=true; }
    else if (netWantFlights)     { netWantFlights = false;     netBusy = true; fetchFlights();      netBusy = false; netUpdated = true; }
    else if (netWantLocation){ netWantLocation = false;   netBusy = true; fetchIpLocation();    netBusy = false; netUpdated = true; }
    else if (netWantWeather) { netWantWeather = false;    netBusy = true; fetchWeather();       netBusy = false; netUpdated = true; }
    else if (netWantPoolDevices){ netWantPoolDevices = false; netBusy = true; fetchGoveeDevices(); netBusy = false; netUpdated = true; }
    else if (netWantPool)    { netWantPool = false;       netBusy = true; fetchGoveeTemp();     netBusy = false; netUpdated = true; }
    else if (netWantAutoScan)   { netWantAutoScan=false;   netBusy=true; autoScanOnce();     netBusy=false; netUpdated=true; }
  }
}

// ---- OpenSky fetch + parse ----
void fetchFlights() {
  planeCount = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (g_savedSsid.length() && !tryConnect(g_savedSsid.c_str(), g_savedPass.c_str())) {
      snprintf(lastErr, sizeof lastErr, "no wifi"); dirty = true; return;
    }
    if (WiFi.status() != WL_CONNECTED) { snprintf(lastErr, sizeof lastErr, "no wifi"); dirty = true; return; }
    connected = true;
  }

  // Flight-data request uses verified TLS against the bundled ISRG roots, the
  // same as the token exchange (no insecure fallback).
  NetworkClientSecure sec;
  sec.setCACert(kISRGRootCAs);
  HTTPClient http;
  char osurl[240];
  // dynamic bbox around current location (0.5 deg ~ 35 mi)
  float d = 0.45f;
  snprintf(osurl, sizeof osurl,
    "https://opensky-network.org/api/states/all?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
    g_lat - d, g_lon - d, g_lat + d, g_lon + d);
  if (!http.begin(sec, osurl)) {
    g_osHandshakeFailed = true;
    snprintf(lastErr, sizeof lastErr, "tls setup");
    http.end();
    dirty = true;
    return;
  }
  http.setTimeout(5000);
  // Tell HTTPClient which response headers to capture. Without this it discards
  // everything except a small built-in set, so X-Rate-Limit-Remaining (the
  // OpenSky credit balance) would never be available.
  const char* hdrKeys[] = { "X-Rate-Limit-Remaining" };
  http.collectHeaders(hdrKeys, 1);
  // Authenticate via OAuth2 client-credentials for the higher 4000-credit/day
  // rate. If no client is configured, openskyEnsureToken() returns false and we
  // fall back to anonymous (400 credits/day). A TLS/handshake failure on the
  // token exchange is NOT a fallback: it's flagged via g_osHandshakeFailed and
  // handled below as a hard error so a MITM can't silently downgrade auth.
  bool authed = openskyEnsureToken();
  if (authed) {
    http.addHeader("Authorization", "Bearer " + g_osToken);
  }
  int code = http.GET();
  g_authChecked = true;   // we've made a real OpenSky attempt; auth state is now meaningful
  // Negative code = TLS handshake / transport failure (e.g. cert rejected or
  // expired CA). Treat as a hard error, never fall back to anonymous.
  if (code < 0 || g_osHandshakeFailed) {
    g_osHandshakeFailed = true;
    if (g_osClientId.length() > 0) g_authState = AUTH_BAD;
    snprintf(lastErr, sizeof lastErr, "tls fail %d", code);
    http.end();
    dirty = true;
    return;
  }
  if (code == HTTP_CODE_UNAUTHORIZED) {
    // Token was rejected/expired; drop it so the next poll refreshes.
    g_osTokenValid = false;
    g_authState = AUTH_BAD;
    snprintf(lastErr, sizeof lastErr, "401");
    http.end();
    dirty = true;
    return;
  }
  if (code == HTTP_CODE_TOO_MANY_REQUESTS) {
    // Rate/credit limited - treat as exhausted regardless of the last known
    // header value, and back off to the recovery cadence.
    g_creditsKnown = true;
    g_creditsRemaining = 0;
    g_creditsExhausted = true;
    snprintf(lastErr, sizeof lastErr, "429 no credits");
    http.end();
    dirty = true;
    return;
  }
  if (code != HTTP_CODE_OK) {
    snprintf(lastErr, sizeof lastErr, "http %d", code);
    http.end();
    dirty = true;
    return;
  }
  // Auth state reflects what OpenSky actually accepted, so invalid credentials
  // can't silently run as anonymous. A configured client whose token exchange
  // failed is bad credentials (warn); otherwise OK when a token was used, or
  // ANON when the request went out unauthenticated.
  if (g_osClientId.length() > 0) g_authState = (authed ? AUTH_OK : AUTH_BAD);
  else g_authState = AUTH_ANON;
  // Capture remaining credits from the rate-limit header (collected via
  // http.collectHeaders() above).
  String rem = http.header("X-Rate-Limit-Remaining");
  if (rem.length()) {
    g_creditsRemaining = rem.toInt();
    g_creditsKnown = true;
    g_creditsExhausted = (g_creditsRemaining <= LOW_CREDIT_THRESHOLD);
  }
  String payload = http.getString();
  http.end();

  // Cap parse memory at 48KB; a busy airspace can return a very large
  // states array and we'd rather fail this fetch than risk an OOM.
  BoundedAllocator openskyAlloc(49152);
  JsonDocument doc(&openskyAlloc);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    snprintf(lastErr, sizeof lastErr, "json %s", err.c_str());
    dirty = true;
    return;
  }

  JsonArray states = doc["states"];
  for (JsonVariant st : states) {
    if (planeCount >= MAXP) break;
    if (st.isNull()) continue;

    JsonVariant csV = st[1];
    JsonVariant lonV = st[5];
    JsonVariant latV = st[6];
    JsonVariant altV = st[7];
    JsonVariant ogV = st[8];
    JsonVariant velV = st[9];

    if (csV.isNull() || lonV.isNull() || latV.isNull() || ogV.isNull()) continue;
    bool onGround = ogV.as<bool>();
    if (onGround) continue;

    float lat = latV.as<float>();
    float lon = lonV.as<float>();
    float altM = altV.isNull() ? 0.0f : altV.as<float>();

    // altitude ceiling filter
    if (g_ceilingFt > 0 && altM * 3.28084f > g_ceilingFt) continue;

    // rough radar range: keep planes within 2x radius (or 15 mi) for the display
    float distMi = hav(g_lat, g_lon, lat, lon);
    float radarRange = max(g_radiusMi * 2.0f, 8.0f);
    if (distMi > radarRange) continue;

    Plane &p = planes[planeCount++];
    JsonVariant icaoV = st[0];
    strncpy(p.icao24, icaoV.isNull() ? "" : icaoV.as<const char*>(), 6);
    p.icao24[6] = 0;
    strncpy(p.callsign, csV.as<const char*>(), 8);
    p.callsign[8] = 0;
    p.distMi = distMi;
    p.altFt = (int)round(altM * 3.28084f);
    p.spdKt = velV.isNull() ? 0 : (int)round(velV.as<float>() * 1.94384f);
    // dx = east miles, dy = north miles
    float dlat = (lat - g_lat);
    float dlon = (lon - g_lon) * cos(g_lat * PI / 180.0f);
    p.dyMi = dlat * 69.0f;
    p.dxMi = dlon * 69.0f;
  }
  sortPlanes();
  // A freshly found overhead flight re-shows the flight view even if the user
  // had dismissed it earlier via the countdown bar. If the overhead plane's
  // identity changed, reset any route details so they are re-fetched for the
  // new plane (the route cache belongs to the previous plane).
  static char lastOverheadIcao[7] = "";
  static char ledFlashedIcao[7] = "";   // so we flash only once per new plane
  if (planeCount > 0 && planes[0].distMi <= g_radiusMi) {
    g_suppressFlight = false;
    // Snapshot the overhead flight so the "N aircraft" tap can recall the same
    // details later, even after the plane leaves the radius. Update it every
    // poll so alt/speed/distance and the (lazily fetched) route stay current.
    g_lastFlight.valid = true;
    strncpy(g_lastFlight.callsign, planes[0].callsign, 8); g_lastFlight.callsign[8] = 0;
    strncpy(g_lastFlight.icao24, planes[0].icao24, 6);     g_lastFlight.icao24[6]   = 0;
    g_lastFlight.altFt  = planes[0].altFt;
    g_lastFlight.spdKt  = planes[0].spdKt;
    g_lastFlight.distMi = planes[0].distMi;
    g_lastFlight.dxMi   = planes[0].dxMi;
    g_lastFlight.dyMi   = planes[0].dyMi;
    if (!g_routeBusy) {
      g_lastFlight.origin[0] = 0; strncpy(g_lastFlight.origin, g_routeOrigin.c_str(), 5); g_lastFlight.origin[5] = 0;
      g_lastFlight.dest[0]   = 0; strncpy(g_lastFlight.dest,   g_routeDest.c_str(),   5); g_lastFlight.dest[5]   = 0;
      g_lastFlight.routeFetched = g_routeFetched;
    }
    if (strncmp(planes[0].icao24, lastOverheadIcao, 6) != 0) {
      strncpy(lastOverheadIcao, planes[0].icao24, 6);
      lastOverheadIcao[6] = 0;
      g_routeFetched = false;
      g_routeOrigin = "";
      g_routeDest = "";
      ledFlashedIcao[0] = 0;
    }
    // Fetch the route automatically the first time this plane is overhead, and
    // cache it (fetchRoute sets g_routeFetched=true, so it runs once per plane).
    if (!g_routeFetched) fetchRoute(planes[0].icao24);
    // Queue an LED flash for this new overhead flight. Red = departing DFW,
    // green = incoming to DFW, extra blue = top-50 airport. A flight with NO
    // route data (origin and destination both unknown) flashes white instead.
    // The flash itself is deferred to loop() so the flight view draws first.
    if (strncmp(planes[0].icao24, ledFlashedIcao, 6) != 0) {
      bool noData = (g_routeOrigin.length() == 0 && g_routeDest.length() == 0);
      g_flashDeparting = g_flashIncoming = g_flashTop50 = false;
      g_flashWhite = noData;
      bool shouldFlash = noData;
      if (!noData) {
        g_flashDeparting = (g_routeOrigin == "KDFW");
        g_flashIncoming  = (g_routeDest   == "KDFW");
        g_flashTop50     = isTopAirport(g_routeOrigin.c_str()) || isTopAirport(g_routeDest.c_str());
        shouldFlash = (g_flashDeparting || g_flashIncoming || g_flashTop50);
      }
      if (shouldFlash) g_pendingFlash = true;
      strncpy(ledFlashedIcao, planes[0].icao24, 6);
      ledFlashedIcao[6] = 0;
    }
  } else {
    lastOverheadIcao[0] = 0;
  }
  snprintf(lastErr, sizeof lastErr, "%d aircraft", planeCount);
  dirty = true;
}

// ---- IP geolocation for default location (free, no key: ip-api.com) ----
void fetchIpLocation() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin("http://ip-api.com/json/");  // plain HTTP is allowed by this endpoint
  http.setTimeout(5000);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      g_lat = doc["lat"] | g_lat;
      g_lon = doc["lon"] | g_lon;
      fetchWeather();   // refresh weather immediately for the new location
    }
  }
  http.end();
}

// ---- Address geocoding (free, no key: Nominatim / OpenStreetMap) ----
// Returns true on a match and updates g_lat / g_lon. On failure sets lastErr to
// a short code the caller maps to a friendly message.
bool geocodeAddress() {
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(lastErr, sizeof lastErr, "geo 0");   // no connection
    return false;
  }
  String url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q="
             + urlEncode(g_addrSearch);

  // Nominatim is Let's Encrypt signed, verified against the same ISRG roots.
  NetworkClientSecure sec;
  HTTPClient http;
  if (!httpsBegin(http, sec, url.c_str(), kISRGRootCAs)) {
    snprintf(lastErr, sizeof lastErr, "geo tls");
    return false;
  }
  http.addHeader("User-Agent", "cyd-dashboard/1.0 (contact: user@localhost)");
  http.setTimeout(5000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    snprintf(lastErr, sizeof lastErr, "geo %d", code);
    return false;
  }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    snprintf(lastErr, sizeof lastErr, "geo json");
    return false;
  }
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) {
    snprintf(lastErr, sizeof lastErr, "no match");
    g_lastPlace = "";
    return false;
  }
  g_lat = atof(arr[0]["lat"] | "0");
  g_lon = atof(arr[0]["lon"] | "0");
  // Keep a short human-readable confirmation (city/state/country or the full
  // display name) so the user can verify the search resolved correctly.
  const char* disp = arr[0]["display_name"] | "";
  g_lastPlace = disp;
  if (g_lastPlace.length() > 40) g_lastPlace = g_lastPlace.substring(0, 40);
  saveFloat("lat", g_lat);
  saveFloat("lon", g_lon);
  fetchWeather();   // refresh weather immediately for the new location
  snprintf(lastErr, sizeof lastErr, "ok");
  return true;
}

// Govee Open API (Pool Temp): selectGoveeDevice(), fetchGoveeDevices(),
// fetchGoveeTemp() live in pool.ino.

// ---- drawing ----
// Header bar: date, time, credits. The time is rendered larger (FONT2, text
// size 2 = 32px) so it stands out; the header band is widened to fit it.
#define HEADER_H 36
// Header theme: the user-selectable clock color (g_clockCol, default blue) when
// there is no critical issue; red/maroon when a critical dashboard issue is
// showing so an ongoing error is always obvious. Non-critical warnings (e.g.
// running OpenSky anonymously) keep the normal clock color. Persisted as
// "clkcol".
#define DEFAULT_CLOCK_COL TFT_BLUE
uint16_t g_clockCol = DEFAULT_CLOCK_COL;
#define HEADER_ACCENT  TFT_YELLOW

void drawHeaderBand() {
  bool err = (dashboardCriticalLabel() != nullptr);
  uint16_t bg = err ? TFT_MAROON : g_clockCol;
  tft.fillRect(0, 0, 320, HEADER_H, bg);
  // date (left) + credits (right) in FONT2
  tft.setTextColor(TFT_WHITE, bg);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setCursor(4, 11);
  tft.print(fmtDate());
  tft.setTextColor(HEADER_ACCENT, bg);
  tft.setCursor(198, 11);
  if (g_creditsKnown) tft.printf("C%d", g_creditsRemaining);
  else tft.print("C?");
  // bigger, bolder clock: FONT2 doubled
  tft.setTextFont(2);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, bg);
  tft.setCursor(112, 1);
  tft.print(fmtClock());
  tft.setTextSize(1);
}

// Incremental 1-second update for the dashboard. Instead of clearing and
// redrawing the whole screen (which flickers), it only redraws the header band
// (the clock) and the countdown bar. The rest of the screen is unchanged. Full
// redraws still happen when the underlying data (flights/weather/pool) changes.
void updateDashboard() {
  // Incremental 1-second update for the dashboard and the flight-detail page;
  // both are the only screens with a header band.
  if (g_screen != SCR_DASH && g_screen != SCR_FLIGHTDETAIL) return;
  // The header background color depends on WiFi; if it flipped, a full redraw is
  // cleaner than patching (the border color changes too).
  static bool lastConnected = false;
  if (lastConnected != connected) {
    lastConnected = connected;
    dirty = true;
    return;
  }
  // Only redraw the header when the clock text actually changes (once a minute).
  // Redrawing the whole header band every second caused visible flicker even
  // though the time/date text does not change that often.
  static String lastClock;
  String curClock = fmtClock();
  if (curClock != lastClock) {
    lastClock = curClock;
    drawHeaderBand();
    // The header redraw covers the Back button (it sits inside the header band),
    // so restore it. On the dashboard only when a flight is overhead; the
    // flight-detail page always shows it.
    bool onDetail = (g_screen == SCR_FLIGHTDETAIL);
    bool overhead = !onDetail && g_trackEnabled && !g_suppressFlight
                    && (planeCount > 0 && planes[0].distMi <= g_radiusMi);
    if (overhead || onDetail) drawFlightBackButton();
  }
  if (g_screen == SCR_DASH && g_showTimer && g_trackEnabled) drawCountdownBar(); // updates the bar in place
  drawAutoUpdateStatus();
}

// Draw the auto-update status line at the bottom-left of the dashboard.
// "Scanning" shows while a scan is in flight; the others persist for a few
// seconds (g_autoUpdStatusUntil) so the outcome is readable on screen.
void drawAutoUpdateStatus() {
  if (g_autoUpdStatus == 0) return;
  // Every status (including "Scanning") expires after its deadline, so a
  // status can never get stuck on screen if a code path forgets to transition
  // it away (see g_autoUpdStatusUntil at each set site).
  if ((long)(millis() - g_autoUpdStatusUntil) > 0) { g_autoUpdStatus = 0; return; }
  const char* msg = "";
  uint16_t col = TFT_LIGHTGREY;
  switch (g_autoUpdStatus) {
    case 1: msg = "Update: Scanning...";  col = TFT_YELLOW; break;
    case 2: msg = "Update: No Updates"; col = TFT_LIGHTGREY; break;
    case 3: msg = "Update: Updating..."; col = TFT_GREEN; break;
    case 4: msg = "Update: Check Failed"; col = TFT_RED; break;
    default: return;
  }
  const int x = 8, y = 224, w = 180, h = 12;   // reuse the bottom-left status line (lastErr)
  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y + 2);
  tft.print(msg);
}

void drawDashboard() {
  tft.fillScreen(TFT_BLACK);

  drawHeaderBand();

  // right-side countdown bar (only when tracking is enabled and the timer is shown)
  if (g_showTimer && g_trackEnabled) drawCountdownBar();

  // settings cog under the countdown bar
  drawCog();

  // main content: flight overhead or idle weather. Honor g_suppressFlight so a
  // dismissed flight stays dismissed until a new overhead flight is found.
  bool overhead = g_trackEnabled && !g_suppressFlight
                  && (planeCount > 0 && planes[0].distMi <= g_radiusMi);
  if (overhead) {
    // Route details are drawn inline by drawFlightInfo (above the Details
    // button); they only appear if the user tapped Details to fetch them.
    drawFlightInfo(planes[0]);
    drawRadar();
  } else {
    drawIdle();
  }

  // Red border while anonymous or when OpenSky credentials are rejected
  drawAuthBorder();
}

// Returns the label for the dashboard's current critical issue, or nullptr when
// there is none. Priority: No WiFi > invalid creds > no flight credits > pool
// temp unavailable. Critical issues draw a red border and tint the clock bar
// red/maroon. The "anonymous" OpenSky case is deliberately NOT here - it's a
// non-critical warning (flights still work anonymously), handled by
// dashboardWarningLabel() below.
const char* dashboardCriticalLabel() {
  if (WiFi.status() != WL_CONNECTED) return "No WIFI";
  if (g_trackEnabled) {
    if (g_authState == AUTH_BAD) return "Invalid Credentials";
    if (g_creditsKnown && g_creditsExhausted) return "No Flight Credits";
  }
#if POOL_FEATURE
  if (g_poolEnabled && !g_poolValid) return "Pool Temp Data Unavailable";
#endif
  return nullptr;
}

// Non-critical warning label, or nullptr. Running OpenSky anonymously is a
// warning (lower rate limit), not an error, so it draws a yellow border and the
// clock bar keeps its normal color. Only called out after the first fetch has
// actually determined the auth state (otherwise it'd show on startup before
// WiFi/OpenSky is tried).
const char* dashboardWarningLabel() {
  if (g_trackEnabled && g_authChecked && g_authState == AUTH_ANON) return "ANONYMOUS";
  return nullptr;
}

// Status border around the screen showing the current issue/warning. Critical
// issues are red; the anonymous warning is yellow.
void drawAuthBorder() {
  const char* crit = dashboardCriticalLabel();
  const char* warn = dashboardWarningLabel();
  if (crit) drawStatusBorder(crit, TFT_RED);
  else if (warn) drawStatusBorder(warn, TFT_YELLOW);
}

void drawStatusBorder(const char* label, uint16_t col) {
  // 2px frame around the screen
  tft.drawRect(0, 0, 320, 240, col);
  tft.drawRect(1, 1, 318, 238, col);

  // tag on the bottom-right corner of the frame
  tft.fillRect(150, 232, 170, 8, col);
  // Black text is legible on the yellow anonymous-warning border; white on red.
  tft.setTextColor((col == TFT_YELLOW) ? TFT_BLACK : TFT_WHITE, col);
  tft.setTextFont(1);
  tft.setCursor(154, 233);
  tft.print(label);
}

void drawCountdownBar() {
  int x = 303, w = 14, topY = HEADER_H + 2, botY = 200;
  int h = botY - topY;
  unsigned long now = millis();
  unsigned long elapsed = (now >= lastPoll) ? (now - lastPoll) : 0;
  unsigned long period = (unsigned long)g_pollSec * 1000UL;
  float frac = 1.0f;
  if (period > 0) frac = 1.0f - (float)elapsed / (float)period;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  int fill = (int)(h * frac);
  tft.fillRect(x, topY, w, h, TFT_DARKGREY);              // track
  uint16_t col = TFT_GREEN;
  if (frac < 0.33f) col = TFT_RED;
  else if (frac < 0.66f) col = TFT_YELLOW;
  tft.fillRect(x, botY - fill, w, fill, col);             // fill
  tft.drawRect(x, topY, w, h, TFT_WHITE);                 // border
}

void drawCog() {
  int cx = 310, cy = 215, r = 9;
  tft.fillCircle(cx, cy, r, TFT_DARKGREY);
  for (int i = 0; i < 6; i++) {
    float a = i * PI / 3.0f;
    int x1 = cx + (int)(r * 0.6f * cos(a));
    int y1 = cy + (int)(r * 0.6f * sin(a));
    int x2 = cx + (int)(r * 1.3f * cos(a));
    int y2 = cy + (int)(r * 1.3f * sin(a));
    tft.drawLine(x1, y1, x2, y2, TFT_LIGHTGREY);
  }
  tft.fillCircle(cx, cy, 4, TFT_BLACK);
}

// CYD RGB LED pins (active-low). Used to flash when a noteworthy flight is overhead.
#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE  17

// Blink `pin` (active-low) `times` times, `ms` per phase.
void blinkLedPin(int pin, int times, int ms) {
  pinMode(pin, OUTPUT);
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, LOW);   // ON (active-low)
    delay(ms);
    digitalWrite(pin, HIGH);  // OFF
    delay(ms);
  }
}

// Flash the onboard LED to signal an overhead flight:
//   red   = departing from DFW (origin KDFW)
//   green = incoming to DFW   (dest KDFW)
//   blue  = extra flash after the color if the flight involves a top-50 airport
//   white = any overhead flight that does not qualify for red/green/blue
// We turn ALL LEDs off first so a stale LOW on a previous color does not bleed.
void flashLed(bool departingDFW, bool incomingDFW, bool top50, bool white) {
  pinMode(CYD_LED_RED, OUTPUT);   digitalWrite(CYD_LED_RED, HIGH);
  pinMode(CYD_LED_GREEN, OUTPUT); digitalWrite(CYD_LED_GREEN, HIGH);
  pinMode(CYD_LED_BLUE, OUTPUT);  digitalWrite(CYD_LED_BLUE, HIGH);
  if (white) {
    // White = all three LEDs on together (active-low).
    for (int i = 0; i < 3; i++) {
      digitalWrite(CYD_LED_RED, LOW); digitalWrite(CYD_LED_GREEN, LOW); digitalWrite(CYD_LED_BLUE, LOW);
      delay(240);
      digitalWrite(CYD_LED_RED, HIGH); digitalWrite(CYD_LED_GREEN, HIGH); digitalWrite(CYD_LED_BLUE, HIGH);
      delay(240);
    }
  } else {
    if (departingDFW)      blinkLedPin(CYD_LED_RED,   5, 240);
    else if (incomingDFW)  blinkLedPin(CYD_LED_GREEN, 5, 240);
    if (top50)             blinkLedPin(CYD_LED_BLUE,  5, 240);
  }
  digitalWrite(CYD_LED_RED, HIGH); digitalWrite(CYD_LED_GREEN, HIGH); digitalWrite(CYD_LED_BLUE, HIGH);
}

// Back button (upper-right) that dismisses the overhead flight back to idle.
void drawFlightBackButton() {
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.setTextFont(2);
  tft.setCursor(274, 7);
  tft.print("Back");
}

// Draw an RGB565 bitmap (in RAM, not PROGMEM) upscaled by `scale`
// (nearest-neighbor), skipping pixels equal to `transp`. Uses fillRect per
// pixel so no large scratch buffer is needed on the stack. Airline logos are
// stored at their on-screen size (see convert_logos.py) and drawn with
// scale=1 to preserve detail; `scale` stays generic in case a future caller
// needs to upscale.
void drawScaledBitmap(int x, int y, const uint16_t* data, int w, int h,
                      int scale, uint16_t transp) {
  for (int sy = 0; sy < h; sy++) {
    for (int sx = 0; sx < w; sx++) {
      uint16_t c = data[sy * w + sx];
      if (c == transp) continue;
      tft.fillRect(x + sx * scale, y + sy * scale, scale, scale, c);
    }
  }
}

void drawFlightInfo(Plane& p) {
  drawFlightBackButton();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(4);
  tft.setCursor(8, 40);
  tft.print(p.callsign);
  tft.setTextFont(2);

  // Airline name below the callsign (from the ICAO code in the callsign).
  String alName; uint16_t alColor;
  bool hasAirline = airlineInfo(p.callsign, alName, alColor);
  (void)alColor;   // color was only used by the removed badge fallback
  if (hasAirline) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(8, 74);
    tft.print(alName.substring(0, 26));
  }

  tft.setCursor(8, 94);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if (g_metric) {
    tft.printf("%dm  %dkts  %.1fkm", (int)round(p.altFt * 0.3048f), p.spdKt, p.distMi * 1.60934f);
  } else {
    tft.printf("%dft  %dmph  %.1fmi", p.altFt, (int)round(p.spdKt * 1.15078f), p.distMi);
  }

  // Origin/destination (route fetched automatically once per plane and cached).
  // Each endpoint shows the airport code and the city on separate lines, in
  // FONT2 (same as alt/speed/distance). An airport set in the "Home airport"
  // setting (e.g. the local home base) is hidden from both ends; when it's
  // empty (default) every airport is shown. The city line shows "--" for
  // airports not in the lookup table rather than echoing the code.
  // The route is fetched on the network task; while it's being written, don't
  // read g_routeOrigin/g_routeDest (String isn't thread-safe) - just show no
  // route for that frame.
  const char* origin = g_routeBusy ? "" : g_routeOrigin.c_str();
  const char* dest   = g_routeBusy ? "" : g_routeDest.c_str();
  const char* ign    = g_homeAirport.c_str();
  bool ignoring = (ign[0] != 0);
  bool origKnown = origin[0] != 0 && !(ignoring && strcmp(origin, ign) == 0);
  bool destKnown = dest[0] != 0 && !(ignoring && strcmp(dest, ign) == 0);
  bool destEmpty = (dest[0] == 0);
  int y = 114;
  tft.setTextFont(2);
  if (origKnown) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(8, y); tft.print("Origin");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, y + 16); tft.print(origin);        // airport code
    tft.setCursor(8, y + 32); tft.print(airportCity(origin));  // city
    y += 54;
  }
  if (destKnown) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(8, y); tft.print("Destination");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, y + 16); tft.print(dest);           // airport code
    tft.setCursor(8, y + 32); tft.print(airportCity(dest));     // city
  } else if (g_routeFetched && destEmpty && origKnown) {
    // Origin known but OpenSky had no destination for this in-flight aircraft
    // (it doesn't populate the arrival airport until after landing). Show it
    // explicitly as unknown so it can't be misread as origin == destination.
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(8, y); tft.print("Destination");
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, y + 16); tft.print("--");
  }
  // If neither side had route data, say so so it is clear the feature is there.
  if (g_routeFetched && !origKnown && !destKnown) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, y);
    tft.print("No route data");
  }

  // Airline logo in the top-right corner, if one exists (drawn at its native
  // stored size - see convert_logos.py - so no blocky upscaling). No fallback
  // badge - just the logo (or nothing).
  const RuntimeLogo* logo = findAirlineLogo(p.callsign);
  if (logo) {
    drawScaledBitmap(226, 40, logo->data, logo->w, logo->h, 1, 0xF81F);
    logoRelease(logo);
  }
}

void drawRadar() {
  int cx = 235, cy = 155, r = 48;
  tft.drawCircle(cx, cy, r, TFT_DARKGREY);
  tft.drawCircle(cx, cy, r / 2, TFT_DARKGREY);
  tft.drawLine(cx - r, cy, cx + r, cy, TFT_DARKGREY);
  tft.drawLine(cx, cy - r, cx, cy + r, TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(cx - 12, cy + r + 6);
  if (g_metric) tft.printf("%dkm", (int)round(g_radiusMi * 1.60934f));
  else tft.printf("%dmi", (int)round(g_radiusMi));

  float scale = r / g_radiusMi;
  for (int i = 0; i < planeCount; i++) {
    Plane &p = planes[i];
    int px = cx + (int)(p.dxMi * scale);
    int py = cy - (int)(p.dyMi * scale);
    if (px < 0 || px > 319 || py < 0 || py > 239) continue;
    uint16_t col = (p.distMi <= g_radiusMi) ? TFT_RED : TFT_GREEN;
    tft.fillCircle(px, py, 3, col);
  }
  tft.fillCircle(cx, cy, 3, TFT_WHITE); // you
}

// Full-screen page that recalls the last overhead flight (see SCR_FLIGHTDETAIL).
// Mirrors the overhead view layout; if no overhead flight has been recorded yet
// it shows dash placeholders. Reached by tapping the "N aircraft" status on the
// dashboard. Auto-returns to the dashboard after ~30s (see loop()).
void drawFlightDetailPage() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderBand();
  drawFlightBackButton();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(4);
  tft.setCursor(8, 40);
  if (g_lastFlight.valid) tft.print(g_lastFlight.callsign);
  else tft.print("-No Data-");
  tft.setTextFont(2);

  if (g_lastFlight.valid) {
    String alName; uint16_t alColor;
    if (airlineInfo(g_lastFlight.callsign, alName, alColor)) {
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(8, 74);
      tft.print(alName.substring(0, 26));
    }

    tft.setCursor(8, 94);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (g_metric) {
      tft.printf("%dm  %dkts  %.1fkm", (int)round(g_lastFlight.altFt * 0.3048f),
                 g_lastFlight.spdKt, g_lastFlight.distMi * 1.60934f);
    } else {
      tft.printf("%dft  %dmph  %.1fmi", g_lastFlight.altFt,
                 (int)round(g_lastFlight.spdKt * 1.15078f), g_lastFlight.distMi);
    }

    // Origin/destination from the snapshot (same rules as drawFlightInfo).
    const char* origin = g_lastFlight.origin;
    const char* dest   = g_lastFlight.dest;
    const char* ign    = g_homeAirport.c_str();
    bool ignoring = (ign[0] != 0);
    bool origKnown = origin[0] != 0 && !(ignoring && strcmp(origin, ign) == 0);
    bool destKnown = dest[0] != 0 && !(ignoring && strcmp(dest, ign) == 0);
    int y = 114;
    if (origKnown) {
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(8, y); tft.print("Origin");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(8, y + 16); tft.print(origin);
      tft.setCursor(8, y + 32); tft.print(airportCity(origin));
      y += 54;
    }
    if (destKnown) {
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(8, y); tft.print("Destination");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(8, y + 16); tft.print(dest);
      tft.setCursor(8, y + 32); tft.print(airportCity(dest));
    } else if (g_lastFlight.routeFetched && !destKnown && origKnown) {
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(8, y); tft.print("Destination");
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setCursor(8, y + 16); tft.print("--");
    }
    if (g_lastFlight.routeFetched && !origKnown && !destKnown) {
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setCursor(8, y); tft.print("No route data");
    }

    // Airline logo (top-right), if one exists.
    const RuntimeLogo* logo = findAirlineLogo(g_lastFlight.callsign);
    if (logo) {
      drawScaledBitmap(226, 40, logo->data, logo->w, logo->h, 1, 0xF81F);
      logoRelease(logo);
    }
  } else {
    // No overhead flight recorded yet: show dashes for the fields.
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(8, 94);
    tft.print("--  --  --");   // alt / speed / distance
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, 130);
    tft.print("No overhead flight recorded.");
  }

  // Radar showing the recalled plane's position (red = was within radius).
  {
    int cx = 235, cy = 155, r = 48;
    tft.drawCircle(cx, cy, r, TFT_DARKGREY);
    tft.drawCircle(cx, cy, r / 2, TFT_DARKGREY);
    tft.drawLine(cx - r, cy, cx + r, cy, TFT_DARKGREY);
    tft.drawLine(cx, cy - r, cx, cy + r, TFT_DARKGREY);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(cx - 12, cy + r + 6);
    if (g_metric) tft.printf("%dkm", (int)round(g_radiusMi * 1.60934f));
    else tft.printf("%dmi", (int)round(g_radiusMi));
    if (g_lastFlight.valid) {
      float scale = r / g_radiusMi;
      int px = cx + (int)(g_lastFlight.dxMi * scale);
      int py = cy - (int)(g_lastFlight.dyMi * scale);
      uint16_t col = (g_lastFlight.distMi <= g_radiusMi) ? TFT_RED : TFT_GREEN;
      tft.fillCircle(px, py, 3, col);
    }
    tft.fillCircle(cx, cy, 3, TFT_WHITE); // you
  }
}

// Settings screen + sub-screens (Flight Tracker, Sleep Mode, Reset confirm)
// and their small helpers: drawSettings(), drawFtracker(), handleFtrackerTouch(),
// drawReset(), drawSleep(), drawEditRow(), handleSleepTouch(), drawSlider()
// live in settings.ino.

// ---- touch ----
bool inRect(int x, int y, int x0, int y0, int x1, int y1) {
  return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

bool rowMinus(int x, int y, int rowY) { return inRect(x, y, 170, rowY, 204, rowY + 24); }
bool rowPlus(int x, int y, int rowY)  { return inRect(x, y, 238, rowY, 272, rowY + 24); }

void saveFloat(const char* key, float v) {
  prefs.begin("flight", false); prefs.putFloat(key, v); prefs.end();
  dirty = true;
}
void saveInt(const char* key, int v) {
  prefs.begin("flight", false); prefs.putInt(key, v); prefs.end();
  dirty = true;
}

// Read a single XPT2046 12-bit channel (0xD0 = X, 0x90 = Y) over VSPI.
// IMPORTANT: after sending the command byte we must WAIT ~200us for the ADC to
// convert before clocking out the 12-bit result. Reading immediately catches the
// conversion mid-flight and yields wildly unstable values.
static uint16_t xptChannel(uint8_t cmd, SPIClass& spi) {
  spi.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS_PIN, LOW);
  spi.transfer(cmd);                 // command byte
  delayMicroseconds(200);            // let the ADC convert
  uint16_t tmp = spi.transfer(0);    // first 8 data bits
  delayMicroseconds(200);
  tmp = (tmp << 5);
  tmp |= 0x1f & (spi.transfer(0) >> 3);   // last 8 data bits
  digitalWrite(TOUCH_CS_PIN, HIGH);
  spi.endTransaction();
  return tmp & 0x0FFF;
}

// Read the touch position directly from the XPT2046 on VSPI and map it into
// the 320x240 display coordinate space (same space the touch UI uses). Returns
// false when no press is detected.
//
// Gated on the hardware IRQ (GPIO 36, active-low): the XPT2046 only asserts it
// during an actual physical touch, so checking it first avoids ever sampling
// (and mis-trusting) the ADC while untouched. This matters because the raw
// X/Y channels pick up enough noise while idle - especially with WiFi active -
// that a value-based threshold alone produces frequent false "touches".
bool touchReadXY(uint16_t& outX, uint16_t& outY, uint16_t* rawX = nullptr, uint16_t* rawY = nullptr) {
  static SPIClass tspi(TOUCH_SPI);
  static bool inited = false;
  if (!inited) {
    tspi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS_PIN);
    pinMode(TOUCH_CS_PIN, OUTPUT);
    digitalWrite(TOUCH_CS_PIN, HIGH);
    pinMode(TOUCH_IRQ_PIN, INPUT);   // input-only pin, no pull-up available
    inited = true;
  }

  if (digitalRead(TOUCH_IRQ_PIN) != LOW) return false;  // not touched

  uint16_t rx = xptChannel(0xD0, tspi);   // raw X
  uint16_t ry = xptChannel(0x90, tspi);   // raw Y
  if (rawX) *rawX = rx;
  if (rawY) *rawY = ry;

  // Map raw -> display using the NVS-calibrated linear transform
  // (see calibration.ino). disp = (raw - offset) * 1000 / scale.
  long dx = ((long)ry - g_calOffX) * 1000L / g_calScaleX;
  long dy = ((long)rx - g_calOffY) * 1000L / g_calScaleY;
  outX = (uint16_t)constrain(dx, 0, 319);
  outY = (uint16_t)constrain(dy, 0, 239);

  return true;
}

void handleTouch() {
  uint16_t x, y, rx, ry;
  static bool prevPressed = false;

  bool pressed = touchReadXY(x, y, &rx, &ry);
#if TOUCH_DEBUG
  // Print once per tap (rising edge only), not every iteration while held.
  if (pressed && !prevPressed) Serial.printf("touch raw(%u,%u) map(%u,%u)\n", rx, ry, x, y);
#endif

  // While calibration is active, handleTouch defers entirely to calPoll() so it
  // doesn't consume or mis-handle calibration taps.
  if (g_calState != CAL_NONE) { prevPressed = pressed; return; }

  // Long-press (10s) on any screen enters touch calibration. Track the press in
  // real time so a held finger is caught before it's released.
  if (g_calState == CAL_NONE) {
    if (pressed && !prevPressed) {
      g_calLongPressStart = millis();          // press just began
    } else if (pressed && prevPressed) {
      // still held - check for the 10s threshold
      if ((long)(millis() - g_calLongPressStart) >= 10000L) {
        calBegin();
        return;
      }
    }
    // (released: prevPressed goes false; nothing to do here)
  }

  // Only act on the rising edge of a press (like a tap), not continuous holds.
  if (!(pressed && !prevPressed)) { prevPressed = pressed; return; }
  prevPressed = true;   // we've acted on this press; ignore until it's released

  // Draw a crosshair at the touch point so the user sees where the touch is
  // being registered. The screen redraws within ~1s, clearing it.
  tft.drawFastHLine(x - 8, y, 16, TFT_RED);
  tft.drawFastVLine(x, y - 8, 16, TFT_RED);

  if (g_screen == SCR_WIFI) { handleWifiTouch(x, y); return; }

  if (g_screen == SCR_GENERAL) { handleGeneralTouch(x, y); return; }

  if (g_screen == SCR_ABOUT) { handleAboutTouch(x, y); return; }

  if (g_screen == SCR_HELP) { handleHelpTouch(x, y); return; }

  if (g_screen == SCR_SLEEP) { handleSleepTouch(x, y); return; }

  if (g_screen == SCR_FTRACKER) { handleFtrackerTouch(x, y); return; }

  if (g_screen == SCR_POOL) { handlePoolTouch(x, y); return; }

  if (g_screen == SCR_POOLGRAPH) { handlePoolGraphTouch(x, y); return; }

  if (g_screen == SCR_RESET) {
    if (g_resetConfirm == 0) {
      // Step 1: choose what to reset.
      if (inRect(x, y, 8, 180, 104, 214)) { g_resetConfirm = 1; dirty = true; }          // All
      else if (inRect(x, y, 112, 180, 208, 214)) { g_resetConfirm = 2; dirty = true; }   // Settings
      else if (inRect(x, y, 216, 180, 312, 214)) { g_resetConfirm = 0; g_screen = SCR_SETTINGS; dirty = true; }  // Cancel
      return;
    }
    // Step 2: confirmation prompt.
    if (inRect(x, y, 30, 180, 140, 214)) {  // Yes -> wipe + reboot
      prefs.begin("flight", false);
      prefs.clear();
      // "Settings" (g_resetConfirm == 2) clears settings & credentials only, so
      // it keeps the touch calibration - it's hardware-specific and lives in
      // the same NVS namespace we're clearing; without it the panel reverts to
      // factory defaults that don't match this unit and the dashboard looks
      // unresponsive. "All" (g_resetConfirm == 1) is a full factory reset, so
      // it clears calibration too; the touchscreen falls back to defaults and
      // must be recalibrated on the next boot (hold anywhere 10s). Every other
      // key - including the clock color "clkcol" - is removed by prefs.clear()
      // above, so it reverts to its default after either reset.
      if (g_resetConfirm == 2) {
        prefs.putInt("calsx", g_calScaleX); prefs.putLong("calox", g_calOffX);
        prefs.putInt("calsy", g_calScaleY); prefs.putLong("caloy", g_calOffY);
      }
      prefs.end();
      if (g_resetConfirm == 1) poolfsWipe();   // All also deletes pool temp history files
      // Brief on-screen feedback so the tap visibly registers, and a short
      // pause so NVS/LittleFS finish flushing before the reboot. Say exactly
      // what is being wiped.
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextFont(2);
      if (g_resetConfirm == 1) {                      // All
        tft.drawCentreString("Resetting All Data...", 160, 108, 2);
      } else {                                        // Settings
        tft.drawCentreString("Resetting Stored", 160, 100, 2);
        tft.drawCentreString("Settings...", 160, 118, 2);
      }
      delay(1500);
      ESP.restart();
    } else if (inRect(x, y, 180, 180, 290, 214)) {  // No -> back to choose
      g_resetConfirm = 0;
      dirty = true;
    }
    return;
  }

  if (g_screen == SCR_SETTINGS) {
    // 2-column grid; geometry mirrors drawSettings() in settings.ino.
    const int n = 10, rowH = 34, step = 38;
    const int colX[2] = { 10, 164 };
    const int colW = 146;
    const int y0 = 42;
    if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_DASH; dirty = true; return; }   // Back
    int row = (y >= y0) ? (y - y0) / step : -1;
    int col = (x >= 164) ? 1 : 0;
    int idx = (row < 0) ? -1 : row * 2 + col;   // flat index, same order as drawSettings
    if (idx < 0 || idx >= n) return;            // empty grid cell or out of range
    // Confirm the tap is actually inside this button (not a row/column gap).
    if (!inRect(x, y, colX[col], y0 + row * step, colX[col] + colW - 1, y0 + row * step + rowH - 1)) return;
    if (idx == 0) { g_screen = SCR_ABOUT; dirty = true; g_updateState = 1; netWantUpdateCheck = true; return; }  // About
    if (idx == 1) { calBegin(); return; }                                // Calibrate Touch
    if (idx == 2) { g_ftPage = 0; g_screen = SCR_FTRACKER; dirty = true; return; }   // Flight Tracker
    if (idx == 3) { g_screen = SCR_GENERAL; dirty = true; return; }      // General
    if (idx == 4) { g_screen = SCR_HELP; g_helpScroll = 0; dirty = true; return; }  // Help
    if (idx == 5) { g_screen = SCR_LOCATION; dirty = true; return; }     // Location
    if (idx == 6) { g_screen = SCR_POOL; dirty = true; return; }         // Pool Temp
    if (idx == 7) { g_screen = SCR_SLEEP; dirty = true; return; }        // Sleep Mode
    if (idx == 8) { g_resetConfirm = 0; g_screen = SCR_RESET; dirty = true; return; }   // Reset
    if (idx == 9) { enterWifiScreen(); return; }                         // WiFi
    return;
  }

  if (g_screen == SCR_LOCATION) {
    if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }  // Back
    if (inRect(x, y, 240, 40, 312, 64)) {  // Set lat/lon
      char ll[32];
      snprintf(ll, sizeof ll, "%.6f,%.6f", g_lat, g_lon);
      g_latLonStr = ll;
      g_screen = SCR_WIFI;
      g_wifiSub = 10;
      dirty = true;
      return;
    }
    if (inRect(x, y, 10, 96, 310, 130)) {  // Search Address
      // Keep g_addrSearch so a failed/previous search can be edited rather
      // than retyped from scratch.
      g_screen = SCR_WIFI;
      g_wifiSub = 5;
      dirty = true;
      return;
    }
    if (inRect(x, y, 10, 140, 310, 174)) {  // Find by IP
      fetchIpLocation();
      saveFloat("lat", g_lat);
      saveFloat("lon", g_lon);
      dirty = true;
      return;
    }
    return;
  }

  if (g_screen == SCR_CALIB) {
    // Calibration runs via calPoll(); this screen is only shown during it.
    return;
  }

  if (g_screen == SCR_DASH) { // SCR_DASH
    bool overhead = g_trackEnabled && !g_suppressFlight
                    && (planeCount > 0 && planes[0].distMi <= g_radiusMi);

    // settings cog -> settings. Generous tap zone so it's easy to hit even with
    // a small touch-calibration offset (the cog itself is only ~28x24).
    if (inRect(x, y, 278, 184, 320, 234)) { g_screen = SCR_SETTINGS; dirty = true; return; }

    // Back button (upper-right) or the countdown bar dismisses the overhead
    // flight back to idle. The countdown-bar tap zone only applies while the
    // timer is shown (so there is no invisible region when it is hidden).
    if (overhead &&
        ((x >= 265 && y >= 4 && x <= 315 && y <= 24) ||  // Back button
         (g_showTimer && x >= 296 && y >= HEADER_H + 2 && y <= 200))) {  // countdown bar
      g_suppressFlight = true;
      dirty = true;
      return;
    }

    // tapping the pool icon (idle, pool enabled) opens the history graph
    if (!overhead && g_poolEnabled && inRect(x, y, 4, 146, 100, 170)) {
      g_screen = SCR_POOLGRAPH;
      g_graphUntil = millis() + 30000UL;
      dirty = true;
      return;
    }

    // tapping the lower-left status line (e.g. "6 aircraft") while idle recalls
    // the last overhead flight's details (dashes if none has been seen yet).
    if (!overhead && inRect(x, y, 4, 216, 170, 236)) {
      g_screen = SCR_FLIGHTDETAIL;
      g_flightDetailUntil = millis() + 30000UL;
      dirty = true;
      return;
    }
  }

  if (g_screen == SCR_FLIGHTDETAIL) {
    // Any tap keeps the page open (resets the 30s auto-return); the upper-right
    // Back button returns to the dashboard.
    g_flightDetailUntil = millis() + 30000UL;
    if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_DASH; dirty = true; return; }
    return;
  }
}

void setup() {
  Serial.begin(115200);
  prefs.begin("flight", false);
#if ENABLE_SERIAL_PROVISION
  serialProvision();   // optional: import KEY=VALUE credentials from USB serial into NVS
#endif
  calLoad();           // load touch calibration parameters (defaults if none)
#if TOUCH_DEBUG
  Serial.printf("cal: scaleX=%d offX=%ld scaleY=%d offY=%ld\n",
                g_calScaleX, (long)g_calOffX, g_calScaleY, (long)g_calOffY);
#endif
  g_radiusMi = prefs.getFloat("radius", 3.5f);
  g_ceilingFt = prefs.getInt("ceiling", 15000);
  g_pollSec = prefs.getInt("poll", 60);
  g_savedSsid = prefs.getString("ssid", "");
  g_savedPass = prefs.getString("pass", "");
  g_osClientId = prefs.getString("oscid", "");
  g_osClientSecret = prefs.getString("ocssec", "");
  g_lat = prefs.getFloat("lat", 0.0f);
  g_lon = prefs.getFloat("lon", 0.0f);
  g_sleepOn = prefs.getBool("sleepon", true);
  g_sleepStartH = prefs.getInt("sleepsH", 22);
  g_sleepStartM = prefs.getInt("sleepsM", 0);
  g_sleepEndH = prefs.getInt("sleepeH", 8);
  g_sleepEndM = prefs.getInt("sleepeM", 0);
  g_wakeMin = prefs.getInt("wake", 10);
  // The Sleep Mode screen edits these as HHMM/minute text buffers, but they're
  // only ever written when the user actually edits a field (see
  // handleKeyboardTouch()/commitSleepTime() in wifi_config.ino) - they default
  // to hardcoded strings at declaration, so without this they'd keep showing
  // "22:00"/"08:00"/"10 min" on the Settings screen after every reboot (OTA,
  // deep sleep, power cycle) even though the loaded ints above (and the actual
  // sleep behavior) are correct. Sync them from the just-loaded values now.
  {
    char buf[8];
    snprintf(buf, sizeof buf, "%02d%02d", g_sleepStartH, g_sleepStartM);
    g_sleepStartStr = buf;
    snprintf(buf, sizeof buf, "%02d%02d", g_sleepEndH, g_sleepEndM);
    g_sleepEndStr = buf;
    g_wakeStr = String(g_wakeMin);
  }
  g_trackEnabled = prefs.getBool("track", true);
  g_blinkForFlight = prefs.getBool("blinkf", true);
  g_metric = prefs.getBool("metric", false);
  g_showTimer = prefs.getBool("timer", false);
  g_autoUpdate = prefs.getBool("autoupd", true);
  g_lastScanDay = prefs.getULong("lastscan", 0);
  // Dev builds (version ending in "-dev") never auto-update: force it OFF for
  // this boot so a dev flash can't silently upgrade. Do NOT persist it to NVS -
  // the user's saved Auto-Update preference should survive a dev flash, so it's
  // still ON when they later install a release build.
  if (isDevBuild() && g_autoUpdate) {
    g_autoUpdate = false;
  }
  g_clockCol = (uint16_t)prefs.getUInt("clkcol", DEFAULT_CLOCK_COL);
  g_homeAirport = prefs.getString("homeap", "");
  g_goveeKey = prefs.getString("govee", "");
  g_poolDeviceId = prefs.getString("poolid", "");
  g_poolModel = prefs.getString("poolmodel", "");
  g_poolName = prefs.getString("poolname", "");
  g_poolEnabled = prefs.getBool("poolen", false);
  // First boot = no saved location; guess from IP in setup() when connected.
  g_firstBoot = (g_lat == 0.0f && g_lon == 0.0f);
  // First-boot wizard: run touch calibration first if none was ever saved
  // (fresh device or after a full factory reset), then gather WiFi credentials
  // if none are stored. "Has it been calibrated?" is answered by whether the
  // calibration keys exist in NVS (calCompute() always writes all four
  // together), so no separate flag is needed. The keys are read here, while the
  // namespace is still open (calLoad() must not be re-opened until setup()
  // finishes reading).
  bool needCalib = !prefs.isKey("calsx");
  bool needWifi  = (g_savedSsid.length() == 0);
  if (needCalib)      g_bootStage = BOOT_CALIB;
  else if (needWifi)  g_bootStage = BOOT_WIFI;
  else                g_bootStage = BOOT_DONE;
  prefs.end();

  poolfsInit();   // load persisted pool temp history from flash into RAM
  logosInit();    // mount the "logos" partition (may be absent -> run logo-less)

  // esp_sleep_get_wakeup_cause() reads a hardware register that is NOT cleared
  // by a software reset (esp_restart(), used by OTA and Factory Reset). So on
  // any boot that ISN'T actually waking from deep sleep, it can still report
  // the cause from the last real deep-sleep exit, potentially hours earlier.
  // Only trust it when esp_reset_reason() confirms this boot really is a
  // deep-sleep wake; otherwise a stale value could send a fresh OTA/reset boot
  // straight into the low-power sleeperRun() path below (before the display
  // even initializes) instead of a normal boot.
  bool wokeFromDeepSleep = (esp_reset_reason() == ESP_RST_DEEPSLEEP);
  esp_sleep_wakeup_cause_t wakeCause = wokeFromDeepSleep
      ? esp_sleep_get_wakeup_cause() : ESP_SLEEP_WAKEUP_UNDEFINED;
#if TOUCH_IRQ_ENABLED
  // enterDeepSleep() latches TOUCH_CS_PIN low via gpio_hold_en() so the
  // XPT2046 stays selected and can assert its IRQ during sleep. That hold
  // survives the reset caused by waking from deep sleep, so it must be
  // released here - otherwise CS stays stuck low forever, keeping the touch
  // controller permanently selected. That leaves its IRQ line asserted
  // (LOW) indefinitely, and since the EXT0 wakeup below is level-triggered
  // on LOW, every later deep-sleep attempt would wake right back up
  // immediately, so the device would never actually stay asleep.
  gpio_hold_dis((gpio_num_t)TOUCH_CS_PIN);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);   // deselect the touch controller
  // A touch wake is normally reported as ESP_SLEEP_WAKEUP_EXT0. But the cause
  // register isn't always reliable here, so also honor the wake duration when
  // the touch IRQ line is actually asserted (finger still down) at boot.
  // Otherwise the device would wake for a few seconds and then immediately
  // fall back asleep once NTP syncs and inSleepWindowNow() turns true.
  bool touchActiveNow = (digitalRead(TOUCH_IRQ_PIN) == LOW);
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0 || touchActiveNow) {
    wakeUntil = millis() + (unsigned long)g_wakeMin * 60000UL;
  }
#endif

  // If we woke from the deep-sleep timer while inside the sleep window, run
  // the low-power pool logger (it keeps sleeping on its own). It only
  // returns once the sleep window has ended, or if WiFi/time couldn't be
  // obtained - either way we fall through to a normal boot below. When it
  // returns due to the window ending, WiFi is already connected and time is
  // already synced, so we skip repeating that work.
  bool alreadyAwake = false;
  if (g_sleepOn && wakeCause == ESP_SLEEP_WAKEUP_TIMER) {
    alreadyAwake = sleeperRun();
  }

  tft.init();
  tft.setRotation(1); // landscape 320x240
  tft.fillScreen(TFT_BLACK);

  // First-boot wizard. If no touch calibration is saved, kick it off now; it
  // runs on its own screen (driven by calPoll() in loop()) and, when it
  // finishes, advances to the WiFi step if needed or to the dashboard. If
  // calibration is already saved but no WiFi credentials are, jump straight to
  // the WiFi setup screen here.
  if (g_bootStage == BOOT_CALIB) {
    calBegin();
    // Calibration needs a responsive touch path before anything else.
    dirty = true;
  } else if (g_bootStage == BOOT_WIFI) {
    enterWifiScreen();
    dirty = true;
  }

  // Connect without blocking at boot: start an async connect and show the
  // dashboard right away (No WIFI border if not connected yet). Data loads once
  // WiFi is up via the loop() g_wifiConnectedOnce block, so boot isn't held up
  // by the 15s tryConnect timeout and fetch timeouts on a no-WiFi device.
  connected = (WiFi.status() == WL_CONNECTED);
  if (!alreadyAwake && !connected && g_savedSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_savedSsid.c_str(), g_savedPass.c_str());   // non-blocking
  }
  if (connected) {
    // Start NTP only after WiFi is up. Calling configTime() (SNTP/UDP) before
    // the WiFi link is established triggers the ESP32 Arduino "Required to lock
    // TCPIP core functionality" crash on lwip.
    setupNTP();
    delay(100);            // let SNTP get a moment to start cleanly
    // On first boot (no saved location) guess it from IP before fetching
    // weather, so we don't query the wrong coordinates.
    if (g_firstBoot) fetchIpLocation();
    else fetchWeather();
    // Pool temp: auto-load devices if a key is set but none selected yet,
    // otherwise refresh the temperature of the saved device. Give SNTP a moment
    // to sync first so the initial sample gets a real epoch (otherwise it would
    // be invisible to the history graph).
#if POOL_FEATURE
    if (g_goveeKey.length() > 0) {
      unsigned long t0 = millis();
      while (time(nullptr) < 1600000000L && millis() - t0 < 8000) delay(100);
      if (g_poolDeviceId.length() == 0) fetchGoveeDevices();
      else fetchGoveeTemp();
    }
#endif
    g_bootFetched = true;   // setup() fetched the network data it could
  }
  // Stay on the dashboard even if no WiFi is configured (users reach the WiFi
  // setup screen from Settings). If credentials are saved but the connection
  // fails, the dashboard shows the "No WIFI" border and retries automatically.

  // Start the async network task on the other core so blocking HTTP calls never
  // freeze the main loop (touch + drawing).
  xTaskCreatePinnedToCore(netTask, "net", 12288, NULL, 1, NULL, 0);

  dirty = true;
}

// Enter deep sleep with a timer wakeup (to keep logging the pool temp), plus
// a touch-IRQ wakeup if TOUCH_IRQ_ENABLED and wired. Never returns.
void enterDeepSleep() {
#if TOUCH_IRQ_ENABLED
  // Keep the XPT2046 selected during deep sleep so its IRQ can assert LOW on
  // touch. Holding the CS line low lets GPIO 36 (the IRQ) wake us via EXT0.
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, LOW);
  gpio_hold_en((gpio_num_t)TOUCH_CS_PIN);
  delay(10);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_IRQ_PIN, 0);  // wake on LOW (touch)
#endif
  esp_sleep_enable_timer_wakeup(SLEEP_POOL_INTERVAL_US);
  saveRollupState();   // keep the in-progress hour/day rollups across this deep sleep
  esp_deep_sleep_start();
}

// Is the current local time inside the configured sleep window?
bool inSleepWindowNow() {
  if (!g_sleepOn) return false;
  // Only trust the clock once NTP has synced. After a soft reset (e.g. OTA),
  // the RTC retains a valid-looking UTC time, but the timezone isn't set until
  // configTime() runs after WiFi connects - so getLocalTime() would apply the
  // default UTC offset and could report a local time that lands in the sleep
  // window even when the real time is outside it (causing an unwanted sleep).
  // sntp_get_sync_status() is NOT used here: Arduino-ESP32's configTime()
  // never drives it to SNTP_SYNC_STATUS_COMPLETED (confirmed on-device - it
  // stays SNTP_SYNC_STATUS_RESET forever even once the clock is correctly
  // synced), which would permanently block sleep. Use the same "epoch looks
  // sane" check already used elsewhere in this file instead.
  if (time(nullptr) < 1600000000L) return false;
  struct tm t;
  // Explicit 0ms timeout: called every main-loop iteration, and
  // getLocalTime()'s default 5s timeout would block the loop (touch +
  // drawing) for that long every time while time isn't synced yet.
  if (!getLocalTime(&t, 0)) return false;
  int cur = t.tm_hour * 60 + t.tm_min;
  int start = g_sleepStartH * 60 + g_sleepStartM;
  int end   = g_sleepEndH * 60 + g_sleepEndM;
  if (start < end) return (cur >= start && cur < end);
  return (cur >= start || cur < end);   // wraps past midnight
}

// Low-power pool logger run while inside the sleep window. Wakes on the deep-
// sleep timer, syncs time, logs the pool temperature to flash, and sleeps
// again. Returns true only when the sleep window has ended (so the caller
// should boot normally). Never returns while still inside the window.
bool sleeperRun() {
  if (g_savedSsid.length() == 0) return false;
  // WiFi first, then NTP: calling configTime() (SNTP/UDP) before the WiFi link
  // is established can trigger the lwip "Required to lock TCPIP core
  // functionality" crash (see the note above setup()'s setupNTP call).
  bool conn = tryConnect(g_savedSsid.c_str(), g_savedPass.c_str());
  if (!conn) return false;
  setupNTP();

  // wait for NTP time so the sleep-window check is valid
  unsigned long t0 = millis();
  while (time(nullptr) < 1600000000L && millis() - t0 < 8000) delay(100);

  while (true) {
    if (!inSleepWindowNow()) return true;    // window ended -> boot normally
    if (g_goveeKey.length() > 0 && g_poolDeviceId.length() > 0) {
      fetchGoveeTemp();                      // logs to flash via poolLog
    }
    enterDeepSleep();                        // resets on next wake
  }
}

// Dedicated task for performOTA. Runs on a large stack because the mbedtls TLS
// handshake overflows the small (8KB) loop task. Owns the display while running;
// never returns on success (reboots). On failure it returns to About showing the
// error state.
void otaTaskEntry(void*) {
  // The OTA downloads over the network while the net task may be mid-fetch
  // (flights/weather/pool polling continues on the dashboard). Two tasks doing
  // HTTP/lwIP at once can trigger a FreeRTOS xTaskPriorityDisinherit assert, so
  // wait for any in-flight net fetch to finish before starting the download.
  while (netBusy) vTaskDelay(20);
  performOTA(g_otaUrl, g_otaVersion, g_otaSha256);
  // Only reached on failure:
  g_otaRunning = false;
  g_otaTask = NULL;
  g_screen = SCR_ABOUT;
  g_updateState = 4;   // Update Check Failed
  dirty = true;
  vTaskDelete(NULL);
}

void loop() {
  unsigned long now = millis();

  // While an OTA runs on its dedicated task, yield so it can own the display
  // (no TFT contention from the loop).
  if (g_otaRunning) { delay(10); return; }

#if TOUCH_DEBUG
  // Heartbeat: if the gap between prints grows large, loop() itself is
  // stalling somewhere (not just slow drawing/network). Logged at most once/sec.
  static unsigned long lastBeat = 0;
  if (now - lastBeat >= 1000) {
    unsigned long gap = (lastBeat == 0) ? 0 : (now - lastBeat);
    Serial.printf("loop alive t=%lu gap=%lums dirty=%d screen=%d netBusy=%d\n",
                  now, gap, dirty, (int)g_screen, netBusy);
    lastBeat = now;
  }
#endif

  // --- Sleep Mode ---
  // Only sleep while on the dashboard (so Settings stays interactive).
  bool asleep = false;
  if (g_screen == SCR_DASH) {
    // expire a user-triggered wake once the duration has elapsed
    if (wakeUntil != 0 && (long)(now - wakeUntil) >= 0) wakeUntil = 0;
    asleep = !g_otaActive && !g_otaRunning && inSleepWindowNow() && (wakeUntil == 0);
  }

  if (asleep) {
    // True low-power sleep: blank the display, then enter deep sleep with a
    // timer wakeup so the pool temperature keeps getting logged. Also wakes
    // on touch if TOUCH_IRQ_ENABLED and the IRQ pin is wired (see config).
    if (!g_displayOff) { tft.writecommand(0x28); g_displayOff = true; }  // ILI9341 off
    delay(150);
    enterDeepSleep();   // does not return; resets on next wake
  }

  // --- Awake path: ensure display is on ---
  if (g_displayOff) { tft.writecommand(0x29); g_displayOff = false; dirty = true; }

  // Start a pending OTA (About Install button or daily auto-scan) on a dedicated
  // task with a large stack: the mbedtls TLS handshake overflows the small
  // loopTask. The OTA task owns the display; loop() yields while it runs.
  if (g_otaActive && !g_otaRunning) {
    g_otaActive = false;
    g_otaRunning = true;
    xTaskCreate(otaTaskEntry, "ota", 32768, NULL, 1, &g_otaTask);
    // The OTA task owns the display from here on. Without this return, the
    // rest of THIS loop() iteration can still fall through to the
    // dirty-redraw block below (e.g. because netUpdated was just set true by
    // the same auto-scan that set g_otaActive), issuing TFT/SPI draw calls
    // concurrently with the OTA task's drawOtaHeader(). TFT_eSPI has no
    // cross-task locking, so that race can hang the SPI bus indefinitely
    // (observed as a full freeze: no crash, no reboot, stale screen content).
    return;
  }
  // OTA rollback safeguard: after a successful boot grace period, cancel any
  // pending rollback so a freshly-installed slot stays active.
  if (!g_rollbackMarked && now > 30000UL) { g_rollbackMarked = true; markAppValidBoot(); }

  handleTouch();
  calPoll();   // drive the touch-calibration state machine (no-op unless active)

  // --- WiFi availability / non-blocking reconnect ---
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  if (!wifiUp && g_savedSsid.length() > 0) {
    if (!wifiTrying) {
      // kick off an asynchronous connect with the saved settings
      wifiTrying = true;
      wifiTryStart = now;
      WiFi.mode(WIFI_STA);
      WiFi.begin(g_savedSsid.c_str(), g_savedPass.c_str());
    } else if (now - wifiTryStart > 25000) {
      // give up on this attempt and try again shortly
      WiFi.disconnect();
      wifiTrying = false;
      wifiTryStart = now;
    }
  } else {
    wifiTrying = false;   // connected or no creds saved
  }
  connected = wifiUp;

  // First time WiFi is up since boot: refresh everything needing the network
  // immediately. Boot no longer blocks on a connect/fetch, so this is also how
  // NTP/time gets started and the initial weather/location/pool loads happen.
  if (wifiUp && !g_wifiConnectedOnce) {
    g_wifiConnectedOnce = true;
    // WiFi is established now, so it's safe to start SNTP (doing it before the
    // link is up triggers a lwip crash).
    setupNTP();
    delay(100);
    if (g_screen == SCR_DASH) {
      // Draw the dashboard now (with whatever data we have) so the screen
      // responds immediately, then the fetches below update it - a failed/slow
      // fetch shouldn't leave the display blank and the UI frozen.
      dirty = true;
      // Flights are never fetched at boot, so always request them right away.
      if (g_trackEnabled) { lastPoll = now; netWantFlights = true; }
      // Weather/location/pool were not fetched by setup() when WiFi wasn't up,
      // so request them here (g_bootFetched is true only if setup() did it).
      if (!g_bootFetched) {
        g_lastWeather = now;
        if (g_firstBoot) netWantLocation = true;
        else netWantWeather = true;
#if POOL_FEATURE
        if (g_poolDeviceId.length() > 0) { g_lastPool = now; netWantPool = true; }
#endif
      }
      dirty = true;
    }
    // Daily auto-update scan (once/day): kick it off on the net task so the
    // synchronous GitHub TLS fetch doesn't run on (and overflow) the small
    // loopTask stack, and doesn't freeze the main loop.
    if (g_autoUpdate) netWantAutoScan = true;
  }

  // --- Polling runs only while WiFi is up; otherwise updates are suspended ---
  if (g_screen == SCR_DASH && wifiUp) {
    // Periodic OpenSky flight poll (only while tracking is enabled). While
    // credits are exhausted, back off to an infrequent recovery check
    // instead of the normal cadence, so we notice once credits refill
    // (OpenSky resets daily) without hammering the API.
    unsigned long pollInterval = g_creditsExhausted
        ? CREDIT_RECOVERY_MS : (unsigned long)g_pollSec * 1000UL;
    if (g_trackEnabled && now - lastPoll >= pollInterval) {
      lastPoll = now;
      netWantFlights = true;
    }
    // Periodic weather refresh
    if (now - g_lastWeather >= WEATHER_REFRESH_MS) {
      g_lastWeather = now;
      netWantWeather = true;
      dirty = true;
    }
    // Periodic pool temp refresh (only when a Govee device is selected)
#if POOL_FEATURE
    if (g_poolDeviceId.length() > 0 && now - g_lastPool >= POOL_REFRESH_MS) {
      g_lastPool = now;
      netWantPool = true;
      dirty = true;
    }
#endif
  }

  // A fetch finished on the network task; make sure the new data is drawn.
  if (netUpdated) {
    netUpdated = false;
    dirty = true;
  }

  // Refresh the dashboard once a second. Only the clock and countdown bar are
  // redrawn (in place) to avoid flicker; the rest of the screen is untouched.
  if (g_screen == SCR_DASH && now - lastClockDraw >= 1000) {
    lastClockDraw = now;
    updateDashboard();
  }

  // Auto-dismiss the pool graph after 30s of inactivity (touch resets it)
  if (g_screen == SCR_POOLGRAPH && g_graphUntil != 0 && (long)(now - g_graphUntil) >= 0) {
    g_screen = SCR_DASH;
    dirty = true;
  }

  // Auto-return from the flight-detail page after 30s of inactivity.
  if (g_screen == SCR_FLIGHTDETAIL && g_flightDetailUntil != 0 &&
      (long)(now - g_flightDetailUntil) >= 0) {
    g_screen = SCR_DASH;
    dirty = true;
  }

  if (dirty) {
    if (g_calState != CAL_NONE) {
      // calibration draws its own screen (calDrawTarget / done message)
      dirty = false;
    } else if (g_screen == SCR_WIFI) drawWifiScreen();
    else if (g_screen == SCR_SETTINGS) drawSettings();
    else if (g_screen == SCR_GENERAL) drawGeneral();
    else if (g_screen == SCR_ABOUT) drawAbout();
    else if (g_screen == SCR_HELP) drawHelp();
    else if (g_screen == SCR_LOCATION) drawLocation();
    else if (g_screen == SCR_RESET) drawReset();
    else if (g_screen == SCR_SLEEP) drawSleep();
    else if (g_screen == SCR_FTRACKER) drawFtracker();
    else if (g_screen == SCR_POOL) drawPool();
    else if (g_screen == SCR_POOLGRAPH) drawPoolGraph();
    else if (g_screen == SCR_FLIGHTDETAIL) drawFlightDetailPage();
    else drawDashboard();
    dirty = false;
  }

  // Run a queued LED flash now that the flight view has been drawn, so the user
  // sees the data first and then the flash notification. Skipped entirely when
  // the "Blink for Flight" setting is off.
  if (g_pendingFlash) {
    g_pendingFlash = false;
    if (g_blinkForFlight)
      flashLed(g_flashDeparting, g_flashIncoming, g_flashTop50, g_flashWhite);
  }
  delay(20);
}
