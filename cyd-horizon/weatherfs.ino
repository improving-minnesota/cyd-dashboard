// weatherfs.ino - persistent, tiered weather temperature history on LittleFS.
// Mirrors poolfs.ino (same tiers + rollup scheme) for the Open-Meteo current
// temperature, sampled every 10 min instead of 5.
//   /weather.csv       raw 10-min samples  (epoch,temp)      - Day & Week views
//   /weather_hour.csv  hourly rollups      (epoch,avg,lo,hi) - Month view
//   /weather_day.csv   daily rollups       (epoch,avg,lo,hi) - Year view
// Per-bucket lo/hi lets Month/Year scale to true extremes. RAM rings mirror
// each tier; no LittleFS = RAM-only session. Helpers shared with poolfs.ino.

unsigned long g_persistWxCount = 0;      // lines currently in /weather.csv
unsigned long g_persistWxHourCount = 0;  // lines currently in /weather_hour.csv
unsigned long g_persistWxDayCount = 0;   // lines currently in /weather_day.csv

// Load persisted weather history into the RAM rings at boot; the partition is
// shared with pool and mounted by poolfsInit(), which must run first.
void weatherfsInit() {
  if (!poolFsOk) return;

  g_persistWxCount = loadCsvRing("/weather.csv", g_wxLogTime, g_wxLogTemp,
                                 nullptr, nullptr,
                                 MAX_WX_LOG, g_wxLogCount, g_wxLogNext);
  g_persistWxHourCount = loadCsvRing("/weather_hour.csv", g_wxHourTime, g_wxHourTemp,
                                     g_wxHourMin, g_wxHourMax,
                                     MAX_WX_HOUR, g_wxHourCount, g_wxHourNext);
  g_persistWxDayCount = loadCsvRing("/weather_day.csv", g_wxDayTime, g_wxDayTemp,
                                    g_wxDayMin, g_wxDayMax,
                                    MAX_WX_DAY, g_wxDayCount, g_wxDayNext);
  // Restore the in-progress hour/day accumulators saved before the last deep
  // sleep (see poolfs.ino loadRollupState() for the why).
  loadWxRollupState();
}

// In-progress hour/day accumulators, persisted before each deep sleep and
// restored at boot - otherwise every wake resets the bucket ids to -1 and the
// hourly/daily tiers never flush overnight.
typedef struct {
  long  hourBucket;   // epoch/3600 of the in-progress hour (-1 = none)
  float hourSum;
  float hourMin;
  float hourMax;
  int   hourN;
  long  dayBucket;    // epoch/86400 of the in-progress day (-1 = none)
  float daySum;
  float dayMin;
  float dayMax;
  int   dayN;
} WxRollup;

void saveWxRollupState() {
  if (!poolFsOk) return;
  WxRollup r;
  r.hourBucket = g_wxHourBucket; r.hourSum = g_wxHourSum;
  r.hourMin = g_wxCurHourMin; r.hourMax = g_wxCurHourMax; r.hourN = g_wxHourN;
  r.dayBucket  = g_wxDayBucket;  r.daySum  = g_wxDaySum;
  r.dayMin = g_wxCurDayMin; r.dayMax = g_wxCurDayMax; r.dayN = g_wxDayN;
  File f = LittleFS.open("/weather_rollup.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
}

void loadWxRollupState() {
  g_wxHourBucket = -1; g_wxHourSum = 0; g_wxCurHourMin = 0; g_wxCurHourMax = 0; g_wxHourN = 0;
  g_wxDayBucket  = -1; g_wxDaySum  = 0; g_wxCurDayMin  = 0; g_wxCurDayMax  = 0; g_wxDayN  = 0;
  if (!poolFsOk) return;
  File f = LittleFS.open("/weather_rollup.bin", "r");
  if (!f) return;
  WxRollup r;
  // A struct from an older firmware (fewer fields) fails the size check and
  // the in-progress buckets just start over - same as a missing file.
  if (f.read((uint8_t*)&r, sizeof(r)) == (size_t)sizeof(r)) {
    g_wxHourBucket = r.hourBucket; g_wxHourSum = r.hourSum;
    g_wxCurHourMin = r.hourMin; g_wxCurHourMax = r.hourMax; g_wxHourN = r.hourN;
    g_wxDayBucket  = r.dayBucket;  g_wxDaySum  = r.daySum;
    g_wxCurDayMin = r.dayMin; g_wxCurDayMax = r.dayMax; g_wxDayN = r.dayN;
    // Only adopt a bucket that actually has samples accumulated.
    if (g_wxHourN <= 0) { g_wxHourBucket = -1; g_wxHourSum = 0; }
    if (g_wxDayN <= 0)  { g_wxDayBucket  = -1; g_wxDaySum  = 0; }
  }
  f.close();
}

// Append a fresh weather temperature reading to the raw tier, then roll it
// into the hourly and daily accumulators (see poolLog() for the scheme).
void weatherLog(float temp, unsigned long epoch) {
  // Require synced time (epoch after ~2020) - an unsynced sample is invisible
  // to the window-filtered graph and would corrupt the rollups below.
  if (epoch < 1600000000L) return;

  appendTier("/weather.csv", g_wxLogTime, g_wxLogTemp, nullptr, nullptr,
             MAX_WX_LOG,
             g_wxLogCount, g_wxLogNext, g_persistWxCount, epoch, temp, 0, 0,
             3000, 1200);   // raw: compact at 3000 lines, keep newest 1200 (~8.3 days @10min)

  long hourBucket = (long)(epoch / 3600UL);
  if (g_wxHourBucket < 0) { g_wxHourBucket = hourBucket; g_wxCurHourMin = g_wxCurHourMax = temp; }
  if (hourBucket != g_wxHourBucket) {
    if (g_wxHourN > 0) {
      appendTier("/weather_hour.csv", g_wxHourTime, g_wxHourTemp,
                 g_wxHourMin, g_wxHourMax, MAX_WX_HOUR,
                 g_wxHourCount, g_wxHourNext, g_persistWxHourCount,
                 (unsigned long)g_wxHourBucket * 3600UL, g_wxHourSum / g_wxHourN,
                 g_wxCurHourMin, g_wxCurHourMax,
                 2000, 900);   // hourly: keep newest 900 (~37 days)
    }
    g_wxHourBucket = hourBucket;
    g_wxHourSum = 0;
    g_wxCurHourMin = g_wxCurHourMax = temp;
    g_wxHourN = 0;
  }
  g_wxHourSum += temp;
  if (temp < g_wxCurHourMin) g_wxCurHourMin = temp;
  if (temp > g_wxCurHourMax) g_wxCurHourMax = temp;
  g_wxHourN++;

  long dayBucket = (long)(epoch / 86400UL);
  if (g_wxDayBucket < 0) { g_wxDayBucket = dayBucket; g_wxCurDayMin = g_wxCurDayMax = temp; }
  if (dayBucket != g_wxDayBucket) {
    if (g_wxDayN > 0) {
      appendTier("/weather_day.csv", g_wxDayTime, g_wxDayTemp,
                 g_wxDayMin, g_wxDayMax, MAX_WX_DAY,
                 g_wxDayCount, g_wxDayNext, g_persistWxDayCount,
                 (unsigned long)g_wxDayBucket * 86400UL, g_wxDaySum / g_wxDayN,
                 g_wxCurDayMin, g_wxCurDayMax,
                 1500, 800);  // daily: keep newest 800 (~2.2 years)
    }
    g_wxDayBucket = dayBucket;
    g_wxDaySum = 0;
    g_wxCurDayMin = g_wxCurDayMax = temp;
    g_wxDayN = 0;
  }
  g_wxDaySum += temp;
  if (temp < g_wxCurDayMin) g_wxCurDayMin = temp;
  if (temp > g_wxCurDayMax) g_wxCurDayMax = temp;
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
  g_wxHourBucket = -1; g_wxHourSum = 0; g_wxCurHourMin = 0; g_wxCurHourMax = 0; g_wxHourN = 0;
  g_wxDayBucket = -1;  g_wxDaySum = 0;  g_wxCurDayMin = 0;  g_wxCurDayMax = 0;  g_wxDayN = 0;
}
