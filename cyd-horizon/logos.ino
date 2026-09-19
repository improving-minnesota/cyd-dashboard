// logos.ino - airline logos loaded at runtime from the dedicated "logos"
// LittleFS partition (firmware stays logo-less; DEVELOPER.md "Airline logos").
// One boot-time buffer is reused for the one logo drawn at a time - a lazy
// cache fragmented the PSRAM-less heap enough to break mbedTLS.
// .bin layout (LE, from convert_logos.py): "LGO1" | w u16 | h u16 | transparent u16 | reserved 2 | w*h*2

#include <LittleFS.h>
#include "esp_heap_caps.h"

static const char LOGO_MAGIC[4] = {'L', 'G', 'O', '1'};

// Logo size produced by convert_logos.py (BOX_W x BOX_H) - the buffer's size.
#define LOGO_MAX_W 72
#define LOGO_MAX_H 48

// Prefer PSRAM (keeps logos off the internal heap); fall back to malloc.
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

// Logo by callsign ICAO prefix; the reusable buffer stays valid until the next
// call (draw immediately; logoRelease() is a no-op).
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
