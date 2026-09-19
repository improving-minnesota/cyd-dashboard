// weatherfs.ino - persistent, tiered weather temperature history on LittleFS
// (flash). Mirrors the pool temp storage in poolfs.ino (same CSV tiers, same
// hourly/daily rollup scheme) but for the Open-Meteo current temperature,
// which is sampled every 10 minutes instead of every 5.
//   /weather.csv       raw 10-min samples  (epoch,temp) - Day & Week views
//   /weather_hour.csv  hourly averages     (epoch,temp) - Month view
//   /weather_day.csv   daily averages      (epoch,temp) - Year view
// Each tier is mirrored in a RAM ring buffer for fast graph drawing and is
// loaded back from flash at boot. If LittleFS is unavailable, everything
// degrades gracefully to RAM-only for the current session. The generic
// loadCsvRing/appendTier/compactCsvFile helpers are shared with poolfs.ino.

unsigned long g_persistWxCount = 0;      // lines currently in /weather.csv
unsigned long g_persistWxHourCount = 0;  // lines currently in /weather_hour.csv
unsigned long g_persistWxDayCount = 0;   // lines currently in /weather_day.csv

// Load all persisted weather history into the RAM ring buffers at boot.
// Both pool and weather share the same LittleFS partition, mounted by
// poolfsInit(), so just make sure that happened first.
void weatherfsInit() {
  if (!poolFsOk) return;

  g_persistWxCount = loadCsvRing("/weather.csv", g_wxLogTime, g_wxLogTemp,
                                 MAX_WX_LOG, g_wxLogCount, g_wxLogNext);
  g_persistWxHourCount = loadCsvRing("/weather_hour.csv", g_wxHourTime, g_wxHourTemp,
                                     MAX_WX_HOUR, g_wxHourCount, g_wxHourNext);
  g_persistWxDayCount = loadCsvRing("/weather_day.csv", g_wxDayTime, g_wxDayTemp,
                                    MAX_WX_DAY, g_wxDayCount, g_wxDayNext);
  // Restore the in-progress hour/day accumulators saved before the last deep
  // sleep (see poolfs.ino loadRollupState() for the why).
  loadWxRollupState();
}

// The in-progress hour/day rollup accumulators, persisted to flash right before
// each deep sleep and restored at boot. Without this, every deep-sleep wake is
// a fresh boot that resets g_wxHourBucket/g_wxDayBucket to -1, so the hour/day
// buckets never "change" within a one-sample boot and the hourly/daily tiers
// would never flush during the night (starving Month/Year).
typedef struct {
  long  hourBucket;   // epoch/3600 of the in-progress hour (-1 = none)
  float hourSum;
  int   hourN;
  long  dayBucket;    // epoch/86400 of the in-progress day (-1 = none)
  float daySum;
  int   dayN;
} WxRollup;

void saveWxRollupState() {
  if (!poolFsOk) return;
  WxRollup r;
  r.hourBucket = g_wxHourBucket; r.hourSum = g_wxHourSum; r.hourN = g_wxHourN;
  r.dayBucket  = g_wxDayBucket;  r.daySum  = g_wxDaySum;  r.dayN  = g_wxDayN;
  File f = LittleFS.open("/weather_rollup.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
}

void loadWxRollupState() {
  g_wxHourBucket = -1; g_wxHourSum = 0; g_wxHourN = 0;
  g_wxDayBucket  = -1; g_wxDaySum  = 0; g_wxDayN  = 0;
  if (!poolFsOk) return;
  File f = LittleFS.open("/weather_rollup.bin", "r");
  if (!f) return;
  WxRollup r;
  if (f.read((uint8_t*)&r, sizeof(r)) == (size_t)sizeof(r)) {
    g_wxHourBucket = r.hourBucket; g_wxHourSum = r.hourSum; g_wxHourN = r.hourN;
    g_wxDayBucket  = r.dayBucket;  g_wxDaySum  = r.daySum;  g_wxDayN  = r.dayN;
    // Only adopt a bucket that actually has samples accumulated.
    if (g_wxHourN <= 0) { g_wxHourBucket = -1; g_wxHourSum = 0; }
    if (g_wxDayN <= 0)  { g_wxDayBucket  = -1; g_wxDaySum  = 0; }
  }
  f.close();
}

// Append a fresh weather temperature reading to the raw tier, then roll it
// into the hourly and daily accumulators (see poolLog() for the scheme).
void weatherLog(float temp, unsigned long epoch) {
  // Require synced time (epoch after ~2020): otherwise the sample would be
  // invisible to the history graph (it filters to the current window) and
  // would corrupt the hourly/daily rollups below.
  if (epoch < 1600000000L) return;

  appendTier("/weather.csv", g_wxLogTime, g_wxLogTemp, MAX_WX_LOG,
             g_wxLogCount, g_wxLogNext, g_persistWxCount, epoch, temp,
             3000, 1200);   // raw: compact at 3000 lines, keep newest 1200 (~8.3 days @10min)

  long hourBucket = (long)(epoch / 3600UL);
  if (g_wxHourBucket < 0) g_wxHourBucket = hourBucket;
  if (hourBucket != g_wxHourBucket) {
    if (g_wxHourN > 0) {
      appendTier("/weather_hour.csv", g_wxHourTime, g_wxHourTemp, MAX_WX_HOUR,
                 g_wxHourCount, g_wxHourNext, g_persistWxHourCount,
                 (unsigned long)g_wxHourBucket * 3600UL, g_wxHourSum / g_wxHourN,
                 2000, 900);   // hourly: keep newest 900 (~37 days)
    }
    g_wxHourBucket = hourBucket;
    g_wxHourSum = 0;
    g_wxHourN = 0;
  }
  g_wxHourSum += temp;
  g_wxHourN++;

  long dayBucket = (long)(epoch / 86400UL);
  if (g_wxDayBucket < 0) g_wxDayBucket = dayBucket;
  if (dayBucket != g_wxDayBucket) {
    if (g_wxDayN > 0) {
      appendTier("/weather_day.csv", g_wxDayTime, g_wxDayTemp, MAX_WX_DAY,
                 g_wxDayCount, g_wxDayNext, g_persistWxDayCount,
                 (unsigned long)g_wxDayBucket * 86400UL, g_wxDaySum / g_wxDayN,
                 1500, 800);  // daily: keep newest 800 (~2.2 years)
    }
    g_wxDayBucket = dayBucket;
    g_wxDaySum = 0;
    g_wxDayN = 0;
  }
  g_wxDaySum += temp;
  g_wxDayN++;
}

// Delete all persisted weather temp history files and reset the RAM ring
// buffers / rollup accumulators. Called alongside poolfsWipe() for "Reset All".
void weatherfsWipe() {
  if (poolFsOk) {
    LittleFS.remove("/weather.csv");
    LittleFS.remove("/weather_hour.csv");
    LittleFS.remove("/weather_day.csv");
    LittleFS.remove("/weather_rollup.bin");
  }
  g_persistWxCount = g_persistWxHourCount = g_persistWxDayCount = 0;
  g_wxLogCount = g_wxLogNext = 0;
  g_wxHourCount = g_wxHourNext = 0;
  g_wxDayCount = g_wxDayNext = 0;
  g_wxHourBucket = -1; g_wxHourSum = 0; g_wxHourN = 0;
  g_wxDayBucket = -1;  g_wxDaySum = 0;  g_wxDayN = 0;
}
