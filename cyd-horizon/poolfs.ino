// poolfs.ino - persistent, tiered pool temperature history on LittleFS (flash).
//
// Three tiers are kept so the Day/Week/Month/Year graphs all have real data:
//   /pool.csv       raw 5-min samples   (epoch,temp)        - Day & Week views
//   /pool_hour.csv  hourly rollups      (epoch,avg,lo,hi)   - Month view
//   /pool_day.csv   daily rollups       (epoch,avg,lo,hi)   - Year view
// The rolled-up tiers store each bucket's low/high alongside the average so
// the Month/Year graphs can scale to the true extremes (e.g. the actual
// annual high) instead of min/max-of-averages.
// Each tier is mirrored in a RAM ring buffer for fast graph drawing and is
// loaded back from flash at boot. If LittleFS is unavailable, everything
// degrades gracefully to RAM-only for the current session.

#include <LittleFS.h>

bool poolFsOk = false;
unsigned long g_persistCount = 0;      // lines currently in /pool.csv
unsigned long g_persistHourCount = 0;  // lines currently in /pool_hour.csv
unsigned long g_persistDayCount = 0;   // lines currently in /pool_day.csv

// Generic loader: reads CSV lines from `path` into the given ring buffer
// arrays, returning how many lines were in the file. Raw tiers are
// "epoch,temp" (pass mins/maxs as nullptr); rolled-up tiers are
// "epoch,avg,lo,hi". Legacy 2-field rollup lines load with lo=hi=avg.
// The CSV is append-only (newest at the end) and compaction retains more
// lines than the RAM buffer can hold, so the loader must keep the NEWEST
// `cap` entries, not the oldest: otherwise the most recent samples
// (including today's) would be dropped from RAM and the Day/Week graphs
// would look empty after a boot.
unsigned long loadCsvRing(const char* path, unsigned long* times, float* temps,
                           float* mins, float* maxs,
                           int cap, int &outCount, int &outNext) {
  outCount = 0;
  outNext = 0;
  File f = LittleFS.open(path, "r");
  if (!f) return 0;

  // First pass: count valid lines so we know how many of the oldest to skip.
  unsigned long total = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 3) continue;
    int comma = line.indexOf(',');
    if (comma <= 0) continue;
    total++;
  }
  f.close();

  // Second pass: skip the oldest (total - cap) valid lines, keep the newest cap.
  unsigned long skip = (total > (unsigned long)cap) ? total - (unsigned long)cap : 0;
  f = LittleFS.open(path, "r");
  if (!f) return total;
  unsigned long idx = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 3) continue;
    int comma = line.indexOf(',');
    if (comma <= 0) continue;
    if (idx++ < skip) continue;
    if (outCount >= cap) break;   // all slots filled with the newest entries
    times[outCount] = (unsigned long)atol(line.substring(0, comma).c_str());
    String rest = line.substring(comma + 1);
    int comma2 = rest.indexOf(',');
    float avg = atof(rest.c_str());
    temps[outCount] = avg;
    if (mins && maxs) {
      // Rolled-up tier: "epoch,avg,lo,hi". Legacy 2-field lines have no
      // second comma (or a missing hi) - treat them as a point sample so
      // old history still draws correctly (lo=hi=avg).
      float lo = avg, hi = avg;
      if (comma2 > 0) {
        String hiStr = rest.substring(comma2 + 1);
        int comma3 = hiStr.indexOf(',');
        lo = atof(hiStr.c_str());
        if (comma3 > 0) hi = atof(hiStr.substring(comma3 + 1).c_str());
        else hi = lo;
      }
      mins[outCount] = lo;
      maxs[outCount] = hi;
    }
    outCount++;
  }
  f.close();
  outNext = outCount;
  return total;
}

// Load all persisted history into the RAM ring buffers at boot.
void poolfsInit() {
  // begin(true) formats the spiffs partition on first use. Sketch-only flashes
  // never write the data partition, so without this the mount would fail on
  // every boot and silently disable pool temp history persistence.
  poolFsOk = LittleFS.begin(true);
  if (!poolFsOk) return;

  g_persistCount = loadCsvRing("/pool.csv", g_poolLogTime, g_poolLogTemp,
                                nullptr, nullptr,
                                MAX_POOL_LOG, g_poolLogCount, g_poolLogNext);
  g_persistHourCount = loadCsvRing("/pool_hour.csv", g_poolHourTime, g_poolHourTemp,
                                    g_poolHourMin, g_poolHourMax,
                                    MAX_POOL_HOUR, g_poolHourCount, g_poolHourNext);
  g_persistDayCount = loadCsvRing("/pool_day.csv", g_poolDayTime, g_poolDayTemp,
                                   g_poolDayMin, g_poolDayMax,
                                   MAX_POOL_DAY, g_poolDayCount, g_poolDayNext);
  // Restore the in-progress hour/day accumulators saved before the last deep
  // sleep, so the rollups keep accumulating across deep-sleep wakes instead of
  // restarting at every boot (which would starve the Month/Year tiers overnight).
  loadRollupState();
}

// Append one line to a ring buffer + its backing file, then compact the file
// once it grows past `compactAt` lines, keeping `keep`. Raw tiers store
// "epoch,temp" (pass mins/maxs as nullptr; lo/hi are ignored). Rolled-up
// tiers store "epoch,avg,lo,hi" in both the ring and the file.
void appendTier(const char* path, unsigned long* times, float* temps,
                 float* mins, float* maxs, int cap,
                 int &count, int &next, unsigned long &persistCount,
                 unsigned long epoch, float value, float lo, float hi,
                 unsigned long compactAt, unsigned long keep) {
  times[next] = epoch;
  temps[next] = value;
  if (mins && maxs) { mins[next] = lo; maxs[next] = hi; }
  next = (next + 1) % cap;
  if (count < cap) count++;

  if (!poolFsOk) return;
  File f = LittleFS.open(path, "a");
  if (!f) return;
  if (mins && maxs) f.printf("%lu,%.2f,%.2f,%.2f\n", epoch, value, lo, hi);
  else f.printf("%lu,%.2f\n", epoch, value);
  f.close();
  persistCount++;

  if (persistCount > compactAt) compactCsvFile(path, persistCount, keep);
}

// Rewrite `path` keeping only the newest `keep` lines.
void compactCsvFile(const char* path, unsigned long &persistCount, unsigned long keep) {
  if (persistCount <= keep) return;
  unsigned long skip = persistCount - keep;

  File src = LittleFS.open(path, "r");
  File tmp = LittleFS.open("/tmp_compact.csv", "w");
  if (!src || !tmp) { if (src) src.close(); if (tmp) tmp.close(); return; }

  unsigned long idx = 0;
  while (src.available()) {
    String line = src.readStringUntil('\n');
    line.trim();
    if (line.length() < 3) continue;
    if (idx >= skip) { tmp.print(line); tmp.print('\n'); }
    idx++;
  }
  src.close();
  tmp.close();

  LittleFS.remove(path);
  LittleFS.rename("/tmp_compact.csv", path);
  persistCount = keep;
}

// The in-progress hour/day rollup accumulators. Persisted to flash right before
// each deep sleep and restored at boot. Without this, every deep-sleep wake is
// a fresh boot that resets g_curHourBucket/g_curDayBucket to -1, so the hour/
// day buckets never "change" within a single one-sample boot and the hourly/
// daily tiers would never flush during the night (starving Month/Year).
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
} PoolRollup;

void saveRollupState() {
  if (!poolFsOk) return;
  PoolRollup r;
  r.hourBucket = g_curHourBucket; r.hourSum = g_curHourSum;
  r.hourMin = g_curHourMin; r.hourMax = g_curHourMax; r.hourN = g_curHourN;
  r.dayBucket  = g_curDayBucket;  r.daySum  = g_curDaySum;
  r.dayMin = g_curDayMin; r.dayMax = g_curDayMax; r.dayN = g_curDayN;
  File f = LittleFS.open("/pool_rollup.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
}

void loadRollupState() {
  g_curHourBucket = -1; g_curHourSum = 0; g_curHourMin = 0; g_curHourMax = 0; g_curHourN = 0;
  g_curDayBucket  = -1; g_curDaySum  = 0; g_curDayMin  = 0; g_curDayMax  = 0; g_curDayN  = 0;
  if (!poolFsOk) return;
  File f = LittleFS.open("/pool_rollup.bin", "r");
  if (!f) return;
  PoolRollup r;
  // A struct from an older firmware (fewer fields) fails the size check and
  // the in-progress buckets just start over - same as a missing file.
  if (f.read((uint8_t*)&r, sizeof(r)) == (size_t)sizeof(r)) {
    g_curHourBucket = r.hourBucket; g_curHourSum = r.hourSum;
    g_curHourMin = r.hourMin; g_curHourMax = r.hourMax; g_curHourN = r.hourN;
    g_curDayBucket  = r.dayBucket;  g_curDaySum  = r.daySum;
    g_curDayMin = r.dayMin; g_curDayMax = r.dayMax; g_curDayN = r.dayN;
    // Only adopt a bucket that actually has samples accumulated.
    if (g_curHourN <= 0) { g_curHourBucket = -1; g_curHourSum = 0; }
    if (g_curDayN <= 0)  { g_curDayBucket  = -1; g_curDaySum  = 0; }
  }
  f.close();
}

// Append a fresh reading to the raw tier, then roll it into the hourly and
// daily accumulators, flushing each whenever its time bucket rolls over.
void poolLog(float temp, unsigned long epoch) {
  // Require synced time (epoch after ~2020): otherwise the sample would be
  // invisible to the history graph (it filters to the current window) and
  // would corrupt the hourly/daily rollups below.
  if (epoch < 1600000000L) return;

  appendTier("/pool.csv", g_poolLogTime, g_poolLogTemp, nullptr, nullptr,
             MAX_POOL_LOG,
             g_poolLogCount, g_poolLogNext, g_persistCount, epoch, temp, 0, 0,
             6000, 2500);   // raw: compact at 6000 lines, keep newest 2500 (~8.7 days)

  long hourBucket = (long)(epoch / 3600UL);
  if (g_curHourBucket < 0) { g_curHourBucket = hourBucket; g_curHourMin = g_curHourMax = temp; }
  if (hourBucket != g_curHourBucket) {
    if (g_curHourN > 0) {
      appendTier("/pool_hour.csv", g_poolHourTime, g_poolHourTemp,
                 g_poolHourMin, g_poolHourMax, MAX_POOL_HOUR,
                 g_poolHourCount, g_poolHourNext, g_persistHourCount,
                 (unsigned long)g_curHourBucket * 3600UL, g_curHourSum / g_curHourN,
                 g_curHourMin, g_curHourMax,
                 2000, 900);   // hourly: keep newest 900 (~37 days)
    }
    g_curHourBucket = hourBucket;
    g_curHourSum = 0;
    g_curHourMin = g_curHourMax = temp;
    g_curHourN = 0;
  }
  g_curHourSum += temp;
  if (temp < g_curHourMin) g_curHourMin = temp;
  if (temp > g_curHourMax) g_curHourMax = temp;
  g_curHourN++;

  long dayBucket = (long)(epoch / 86400UL);
  if (g_curDayBucket < 0) { g_curDayBucket = dayBucket; g_curDayMin = g_curDayMax = temp; }
  if (dayBucket != g_curDayBucket) {
    if (g_curDayN > 0) {
      appendTier("/pool_day.csv", g_poolDayTime, g_poolDayTemp,
                 g_poolDayMin, g_poolDayMax, MAX_POOL_DAY,
                 g_poolDayCount, g_poolDayNext, g_persistDayCount,
                 (unsigned long)g_curDayBucket * 86400UL, g_curDaySum / g_curDayN,
                 g_curDayMin, g_curDayMax,
                 1500, 800);  // daily: keep newest 800 (~2.2 years)
    }
    g_curDayBucket = dayBucket;
    g_curDaySum = 0;
    g_curDayMin = g_curDayMax = temp;
    g_curDayN = 0;
  }
  g_curDaySum += temp;
  if (temp < g_curDayMin) g_curDayMin = temp;
  if (temp > g_curDayMax) g_curDayMax = temp;
  g_curDayN++;
}

// Delete all persisted pool temp history files and reset the RAM ring buffers /
// rollup accumulators. Used by the "Reset All" option (Settings -> Reset).
void poolfsWipe() {
  if (poolFsOk) {
    LittleFS.remove("/pool.csv");
    LittleFS.remove("/pool_hour.csv");
    LittleFS.remove("/pool_day.csv");
    LittleFS.remove("/tmp_compact.csv");
    LittleFS.remove("/pool_rollup.bin");
  }
  g_persistCount = g_persistHourCount = g_persistDayCount = 0;
  g_poolLogCount = g_poolLogNext = 0;
  g_poolHourCount = g_poolHourNext = 0;
  g_poolDayCount = g_poolDayNext = 0;
  g_curHourBucket = -1; g_curHourSum = 0; g_curHourMin = 0; g_curHourMax = 0; g_curHourN = 0;
  g_curDayBucket = -1;  g_curDaySum = 0;  g_curDayMin = 0;  g_curDayMax = 0;  g_curDayN = 0;
}
