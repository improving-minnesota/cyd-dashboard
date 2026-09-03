// logos.ino - runtime airline-logo loading from a dedicated LittleFS partition.
//
// Logos are NOT compiled into the firmware. They live as <ICAO>.bin files in a
// dedicated LittleFS partition (label "logos", see partitions.csv). The
// firmware mounts it once at boot and reads logo bitmaps on demand through a
// small bounded LRU cache (PSRAM-first allocation), so:
//   - OTA app slots never embed the brand bitmaps (firmware stays logo-less).
//   - Logos can be updated / added / removed by rewriting files on the
//     partition - no firmware rebuild, and OTA never touches that partition.
//   - If the partition is missing or a logo file is absent, we simply draw no
//     logo (same behavior as the old empty-fallback header).
//
// .bin file layout (little-endian), produced by convert_logos.py --out-dir:
//   "LGO1" (4) | w (u16) | h (u16) | transparent (u16) | reserved (2) | w*h*2
//
// The RuntimeLogo type and the logosInit/findAirlineLogo/logoRelease symbols
// are declared at the top of cyd-dashboard.ino so other .ino files can use
// them regardless of Arduino's alphabetical concatenation order.

#include <LittleFS.h>
#include "esp_heap_caps.h"

static const char LOGO_MAGIC[4] = {'L', 'G', 'O', '1'};
static const int  LOGO_CACHE_MAX = 16;   // bounded cache; never grows the heap unboundedly

struct LogoCacheEntry {
  RuntimeLogo logo;
  bool loaded;
  bool inUse;              // true while a caller is drawing this logo
  unsigned long lastUsed;  // LRU clock
};

static fs::LittleFSFS LogosFS;
static LogoCacheEntry s_logos[LOGO_CACHE_MAX];
static bool s_logosOk = false;
static unsigned long s_logoClock = 0;

// Prefer PSRAM for logo buffers (keeps them off the internal heap used by
// WiFi/HTTP/JSON). Falls back to internal malloc when there is no PSRAM or the
// PSRAM allocation fails.
static void* logoAlloc(size_t bytes) {
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(bytes);
}

// Load one logo file into a freshly allocated buffer. Returns the buffer on
// success (caller frees), nullptr on any failure. Sets *w/*h.
static uint16_t* logoLoad(const char* icao, int* w, int* h) {
  String path = String("/") + icao + ".bin";
  File f = LogosFS.open(path, "r");
  if (!f) return nullptr;

  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12) { f.close(); return nullptr; }
  if (memcmp(hdr, LOGO_MAGIC, 4) != 0) { f.close(); return nullptr; }

  uint16_t ww, hh;
  memcpy(&ww, hdr + 4, 2);
  memcpy(&hh, hdr + 6, 2);
  if (ww == 0 || hh == 0 || ww > 200 || hh > 200) { f.close(); return nullptr; }

  size_t n = (size_t)ww * hh;
  uint16_t* buf = (uint16_t*)logoAlloc(n * 2);
  if (!buf) { f.close(); return nullptr; }
  if (f.read((uint8_t*)buf, n * 2) != (int)(n * 2)) {
    free(buf); f.close(); return nullptr;
  }
  f.close();
  *w = ww; *h = hh;
  return buf;
}

// Mount the "logos" partition (independent of the pool temp history "spiffs" one).
// Returns true if it mounted; the device runs logo-less if not.
bool logosInit() {
  s_logosOk = false;
  if (!LogosFS.begin(false, "/logos", 10, "logos")) {
    log_w("logos: partition not mounted - running logo-less");
    return false;
  }
  s_logosOk = true;
  return true;
}

// Look up a logo by callsign (3-letter ICAO prefix), loading it into the cache
// on a miss. The returned pointer stays valid until the next cache-evicting
// call, so callers should draw immediately and then call logoRelease(). Returns
// nullptr if the logo is unavailable (draw no logo).
const RuntimeLogo* findAirlineLogo(const char* callsign) {
  if (!s_logosOk || !callsign || callsign[0] == 0) return nullptr;

  String code = String(callsign);
  code.toUpperCase();
  code = code.substring(0, 3);
  if (code.length() != 3) return nullptr;
  char icao[4];
  code.toCharArray(icao, 4);

  s_logoClock++;

  // Cache hit.
  for (int i = 0; i < LOGO_CACHE_MAX; i++) {
    LogoCacheEntry& e = s_logos[i];
    if (e.loaded && strcmp(e.logo.icao, icao) == 0) {
      e.lastUsed = s_logoClock;
      e.inUse = true;
      return &e.logo;
    }
  }

  // Pick a slot: an unused one if free, otherwise evict the LRU entry that is
  // not currently being drawn. If everything is in use and full, bail (the
  // caller draws no logo rather than risk a shared buffer).
  int victim = -1;
  unsigned long oldest = ~0UL;
  for (int i = 0; i < LOGO_CACHE_MAX; i++) {
    LogoCacheEntry& e = s_logos[i];
    if (!e.loaded) { victim = i; break; }
    if (!e.inUse && e.lastUsed < oldest) { oldest = e.lastUsed; victim = i; }
  }
  if (victim < 0) return nullptr;

  LogoCacheEntry& e = s_logos[victim];
  if (e.loaded) {
    if (e.logo.data) free(e.logo.data);
    e.logo.data = nullptr;
    e.loaded = false;
  }

  int w = 0, h = 0;
  uint16_t* buf = logoLoad(icao, &w, &h);
  if (!buf) return nullptr;

  e.logo.data = buf;
  e.logo.w = w;
  e.logo.h = h;
  strncpy(e.logo.icao, icao, 3);
  e.logo.icao[3] = 0;
  e.loaded = true;
  e.inUse = true;
  e.lastUsed = s_logoClock;
  return &e.logo;
}

// Mark a logo as no longer being drawn so it can be evicted later.
void logoRelease(const RuntimeLogo* logo) {
  if (!logo) return;
  for (int i = 0; i < LOGO_CACHE_MAX; i++) {
    if (&s_logos[i].logo == logo) { s_logos[i].inUse = false; return; }
  }
}
