// logos.ino - runtime airline-logo loading from a dedicated LittleFS partition.
//
// Logos are NOT compiled into the firmware. They live as <ICAO>.bin files in a
// dedicated LittleFS partition (label "logos", see partitions.csv). The
// firmware mounts it once at boot and reads logo bitmaps on demand through a
// single reusable buffer, so:
//   - OTA app slots never embed the brand bitmaps (firmware stays logo-less).
//   - Logos can be updated / added / removed by rewriting files on the
//     partition - no firmware rebuild, and OTA never touches that partition.
//   - If the partition is missing or a logo file is absent, we simply draw no
//     logo (same behavior as the old empty-fallback header).
//
// The CYD has no PSRAM, so every logo would land on the internal heap shared
// by WiFi/TLS/JSON. The old 16-entry lazy cache parked up to ~110 KB of
// mid-heap blocks and eroded the largest contiguous block until the mbedTLS
// handshake could no longer allocate its ~32 KB buffers. This implementation
// allocates one buffer once during logosInit(), before WiFi/TLS/JSON have
// fragmented the heap, and reuses it for the one logo that is drawn at a time.
//
// .bin file layout (little-endian), produced by convert_logos.py --out-dir:
//   "LGO1" (4) | w (u16) | h (u16) | transparent (u16) | reserved (2) | w*h*2

#include <LittleFS.h>
#include "esp_heap_caps.h"

static const char LOGO_MAGIC[4] = {'L', 'G', 'O', '1'};

// Logo size as produced by convert_logos.py (BOX_W x BOX_H). Pre-allocate one
// buffer of this size up front instead of caching many decoded logos lazily,
// which fragments the internal heap on the PSRAM-less CYD.
#define LOGO_MAX_W 72
#define LOGO_MAX_H 48

// Prefer PSRAM for logo buffers (keeps them off the internal heap used by
// WiFi/HTTP/JSON). Falls back to internal malloc when there is no PSRAM or the
// PSRAM allocation fails.
static void* logoAlloc(size_t bytes) {
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(bytes);
}

static fs::LittleFSFS LogosFS;
static uint16_t* s_logoBuf = nullptr;
static RuntimeLogo s_logo = {"", nullptr, 0, 0};
static bool s_logoValid = false;   // s_logo holds a decoded bitmap
static bool s_logosOk = false;     // LittleFS mount succeeded

// Read one logo file into `buf` (capacity maxW*maxH pixels). Returns false on
// any failure; the caller's buffer is left unchanged if no valid payload fits.
static bool logoLoadInto(const char* icao, uint16_t* buf, int maxW, int maxH, int* w, int* h) {
  String path = String("/") + icao + ".bin";
  File f = LogosFS.open(path, "r");
  if (!f) return false;

  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, LOGO_MAGIC, 4) != 0) { f.close(); return false; }

  uint16_t ww, hh;
  memcpy(&ww, hdr + 4, 2);
  memcpy(&hh, hdr + 6, 2);
  if (ww == 0 || hh == 0 || ww > (uint16_t)maxW || hh > (uint16_t)maxH) { f.close(); return false; }

  size_t bytes = (size_t)ww * hh * 2;
  bool ok = (f.read((uint8_t*)buf, bytes) == (int)bytes);
  f.close();
  if (!ok) return false;
  *w = ww; *h = hh;
  return true;
}

// Mount the "logos" partition (independent of the pool temp history "spiffs" one).
// Returns true if it mounted; the device runs logo-less if not.
bool logosInit() {
  s_logosOk = false;
  if (!LogosFS.begin(false, "/logos", 10, "logos")) {
    log_w("logos: partition not mounted - running logo-less");
    return false;
  }

  if (!s_logoBuf) {
    size_t bytes = (size_t)LOGO_MAX_W * LOGO_MAX_H * 2;
    s_logoBuf = (uint16_t*)logoAlloc(bytes);
    if (!s_logoBuf) {
      log_w("logos: no memory for logo buffer - running logo-less");
      return false;
    }
    s_logo.data = s_logoBuf;
  }

  s_logosOk = true;
  return true;
}

// Look up a logo by callsign (3-letter ICAO prefix). Returns a pointer to a
// reusable RuntimeLogo that stays valid until the next call. Callers should
// draw immediately; logoRelease() is a no-op. Returns nullptr if unavailable.
const RuntimeLogo* findAirlineLogo(const char* callsign) {
  if (!s_logosOk || !s_logoBuf || !callsign || callsign[0] == 0) return nullptr;

  char icao[4] = {0};
  for (int i = 0; i < 3; i++) {
    char c = callsign[i];
    if (c == 0) return nullptr;
    icao[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
  }

  // Same airline as last time: reuse the decoded bitmap, no flash read.
  if (s_logoValid && strcmp(s_logo.icao, icao) == 0) return &s_logo;

  s_logoValid = false;
  int w = 0, h = 0;
  if (!logoLoadInto(icao, s_logoBuf, LOGO_MAX_W, LOGO_MAX_H, &w, &h)) return nullptr;

  s_logo.w = w;
  s_logo.h = h;
  strncpy(s_logo.icao, icao, 3);
  s_logo.icao[3] = 0;
  s_logoValid = true;
  return &s_logo;
}

// Mark a logo as no longer being drawn. With the single reusable buffer this
// is a no-op, but the call sites are unchanged.
void logoRelease(const RuntimeLogo* logo) {
  (void)logo;
}

// Wipe all files from the logos partition. Called by Settings -> Reset ->
// Factory Reset so a full factory reset removes provisioned airline logos.
void logosWipe() {
  if (!s_logosOk) return;
  File root = LogosFS.open("/");
  if (!root || !root.isDirectory()) return;
  File f = root.openNextFile();
  while (f) {
    const char* name = f.name();
    if (name && name[0] != '\0') {
      String path = String("/") + name;
      LogosFS.remove(path.c_str());
    }
    f = root.openNextFile();
  }
  s_logoValid = false;
}
