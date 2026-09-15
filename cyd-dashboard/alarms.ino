// ---- Alarms ----
// Up to MAX_ALARMS time-of-day alarms with per-weekday masks and an LED +
// speaker notification pattern (each preset drives the RGB LED and a matching
// beep pattern on the JST speaker header, GPIO 26). Stored as one NVS blob
// in the "flight" namespace, so Settings/Factory reset wipes them like every
// other setting.
//
// Firing model: each alarm carries nextFire, the absolute epoch of its next
// firing, persisted in NVS. An alarm is due iff nextFire <= now - there is no
// time-of-day window, so a firing missed while asleep or powered off still
// goes off when the device next runs. nextFire is rewritten whenever the
// situation changes: editing an alarm rearms it to the next matching weekday,
// Dismiss does the same, and Snooze parks it at now + 5 min. A snoozed alarm
// needs no flag: its nextFire sits earlier than the next scheduled
// occurrence. Each alarm tracks its own nextFire, so multiple alarms can be
// snoozed at once, and each counts its snoozes (a.snoozes, persisted) - an
// episode allows ALARM_MAX_SNOOZES, then the next snooze request (idle
// timeout or button) dismisses it for the day.
//
// When an alarm fires the screen shows big Dismiss / Snooze buttons and the
// LED runs the alarm's pattern until handled. If nobody answers before the
// screen idle timeout it auto-snoozes.
#define MAX_ALARMS 6
#define ALARM_SNOOZE_SEC (5 * 60)
#define ALARM_MAX_SNOOZES 3   // snoozes per firing episode; the next request dismisses

struct Alarm {
  uint8_t en;        // enabled
  uint8_t h;         // hour, 0-23
  uint8_t m;         // minute, 0-59
  uint8_t days;      // weekday bitmask, bit 0 = Sunday
  uint8_t preset;    // index into kAlarmPresets
  uint8_t snoozes;   // snoozes taken this firing episode
  uint8_t reserved;
  uint32_t nextFire; // epoch of next firing; 0 = recompute once time is known
};

Alarm g_alarms[MAX_ALARMS];
int   g_alarmCount = 1;
int   g_alarmIdx = 0;        // which alarm the editor screen shows
int   g_fireIdx = -1;        // which alarm is currently firing
bool  g_alarmFiring = false;
bool  g_alarmDelConfirm = false;   // editor is showing the delete confirmation
String g_alarmTimeStr = "0700";   // HHMM buffer for the manual keyboard editor
extern int g_wifiSub;             // defined in wifi_config.ino (concatenated last)

// LED notification presets. All digital RGB patterns (active-low LED: LOW=on).
static const char* const kAlarmPresets[] = {
  "Blink", "Rapid", "Double", "Colors", "Pulse"
};
static const int kNumAlarmPresets =
    sizeof(kAlarmPresets) / sizeof(kAlarmPresets[0]);

// ---- storage ----
// Bump when the Alarm/AlarmStore layout changes; a mismatch loads defaults.
#define ALARM_STORE_VER 2
struct AlarmStore { uint8_t ver; uint8_t count; Alarm a[MAX_ALARMS]; };

void saveAlarms() {
  AlarmStore st;
  st.ver = ALARM_STORE_VER;
  st.count = (uint8_t)g_alarmCount;
  memcpy(st.a, g_alarms, sizeof st.a);
  prefs.begin("flight", false);
  prefs.putBytes("alarms", &st, sizeof st);
  prefs.end();
}

// Called from setup() while the "flight" prefs namespace is already open.
void loadAlarms() {
  AlarmStore st;
  if (prefs.getBytes("alarms", &st, sizeof st) == sizeof st &&
      st.ver == ALARM_STORE_VER &&
      st.count >= 1 && st.count <= MAX_ALARMS) {
    g_alarmCount = st.count;
    memcpy(g_alarms, st.a, sizeof g_alarms);
  } else {
    g_alarms[0] = Alarm{0, 7, 0, 0x3E, 0, 0, 0, 0};   // 7:00 AM, Mon-Fri, off
  }
}

bool anyAlarmEnabled() {
  for (int i = 0; i < g_alarmCount; i++) if (g_alarms[i].en) return true;
  return false;
}

// ---- scheduling ----
// Earliest future epoch matching the alarm's h:m on an allowed weekday, or 0
// if it can't fire (disabled, no days selected, or time not synced yet).
// Strictly future so a firing already in progress doesn't reselect itself.
static time_t nextScheduled(int i, time_t now) {
  const Alarm& a = g_alarms[i];
  if (!a.en || !a.days) return 0;
  struct tm t;
  if (!getLocalTime(&t, 0)) return 0;
  for (int d = 0; d < 8; d++) {
    struct tm c = t;
    c.tm_mday += d;
    c.tm_hour = a.h; c.tm_min = a.m; c.tm_sec = 0; c.tm_isdst = -1;
    time_t cand = mktime(&c);   // normalizes the date and fills tm_wday
    if (cand > now && ((a.days >> c.tm_wday) & 1)) return cand;
  }
  return 0;
}

// Recompute nextFire from the schedule and clear the snooze count - the end
// of a firing episode (dismiss) or a schedule change (any editor write).
// nextFire==0 means "pending": checkAlarms()/alarmDueNow() fill it in once
// the clock is synced.
void rearmAlarm(int i) {
  Alarm& a = g_alarms[i];
  a.snoozes = 0;
  a.nextFire = (uint32_t)nextScheduled(i, time(nullptr));
}

// ---- firing ----
// A snoozed alarm's nextFire sits earlier than its next scheduled
// occurrence; that comparison is the whole "is snoozed" test - no separate
// flag to keep in sync across reboots.
static bool isSnoozed(int i, time_t now) {
  const Alarm& a = g_alarms[i];
  if (!a.en || a.nextFire == 0 || (time_t)a.nextFire <= now) return false;
  time_t sched = nextScheduled(i, now);
  return sched == 0 || (time_t)a.nextFire < sched;
}

// Index of the alarm whose pending snooze matures soonest, or -1.
static int soonestSnooze(time_t now) {
  int best = -1;
  for (int i = 0; i < g_alarmCount; i++)
    if (isSnoozed(i, now) &&
        (best < 0 || g_alarms[i].nextFire < g_alarms[best].nextFire))
      best = i;
  return best;
}

// True while any alarm is parked on snooze (flight polls pause; the
// dashboard status line shows the countdown).
bool snoozePending() {
  time_t now = time(nullptr);
  if (now < 1600000000L) return false;
  return soonestSnooze(now) >= 0;
}

// Epoch when the soonest pending snooze refires, or 0 when none is pending.
time_t snoozeRefireAt() {
  time_t now = time(nullptr);
  if (now < 1600000000L) return 0;
  int i = soonestSnooze(now);
  return i < 0 ? 0 : (time_t)g_alarms[i].nextFire;
}

// Called from loop() every tick. Fires any alarm whose nextFire has passed -
// scheduled time, matured snooze, or a firing missed while asleep or powered
// off - from whatever screen is showing. Also fills in nextFire for alarms
// saved before the clock was known (e.g. edited with no time sync).
void checkAlarms() {
  if (g_alarmFiring || !g_timeReady) return;
  time_t now = time(nullptr);
  if (now < 1600000000L) return;
  bool changed = false;
  for (int i = 0; i < g_alarmCount; i++) {
    Alarm& a = g_alarms[i];
    if (!a.en) continue;
    if (!a.nextFire) { rearmAlarm(i); changed = true; }
    if (a.nextFire && now >= (time_t)a.nextFire) {
      if (changed) saveAlarms();
      fireAlarm(i);
      return;
    }
  }
  if (changed) saveAlarms();
}

// Same due-check without firing; sleeperRun() runs it on each deep-sleep
// wake so an alarm that came due while sleeping boots the device to fire.
bool alarmDueNow() {
  if (!g_timeReady) return false;
  time_t now = time(nullptr);
  if (now < 1600000000L) return false;
  bool due = false, changed = false;
  for (int i = 0; i < g_alarmCount; i++) {
    Alarm& a = g_alarms[i];
    if (!a.en) continue;
    if (!a.nextFire) { rearmAlarm(i); changed = true; }
    if (a.nextFire && now >= (time_t)a.nextFire) due = true;
  }
  if (changed) saveAlarms();
  return due;
}

// Earliest upcoming nextFire across enabled alarms, or 0. enterDeepSleep()
// uses it to wake exactly at the next firing instead of discovering it up to
// ~5 min late on the pool-temp cadence.
time_t nextAlarmAt() {
  time_t now = time(nullptr);
  if (now < 1600000000L) return 0;
  uint32_t best = 0;
  for (int i = 0; i < g_alarmCount; i++)
    if (g_alarms[i].en && g_alarms[i].nextFire > (uint32_t)now &&
        (!best || g_alarms[i].nextFire < best))
      best = g_alarms[i].nextFire;
  return (time_t)best;
}

// "Snoozing for N minutes..." for the dashboard's bottom-left status line
// while a snooze is pending; returns false when no snooze is pending. The
// minute count is ceiled so it reads 5 down to 1 rather than hitting 0.
bool snoozeStatusText(char* buf, size_t n) {
  time_t at = snoozeRefireAt();
  if (!at) return false;
  long rem = (long)(at - time(nullptr));
  int mins = (int)((rem + 59) / 60);
  if (mins < 1) mins = 1;
  snprintf(buf, n, "Snoozing for %d minute%s...", mins, mins == 1 ? "" : "s");
  return true;
}

void fireAlarm(int i) {
  g_fireIdx = i;
  g_alarmFiring = true;
  g_screen = SCR_ALARMFIRE;
  g_screenIdleUntil = millis() + SCREEN_IDLE_TIMEOUT_MS;
  dirty = true;
}

void alarmDismiss() {
  if (g_fireIdx >= 0 && g_fireIdx < g_alarmCount) {
    rearmAlarm(g_fireIdx);   // next scheduled day = "dismissed for today"
    saveAlarms();
  }
  g_alarmFiring = false;
  g_fireIdx = -1;
  g_screen = SCR_DASH;
  g_screenIdleUntil = 0;
  dirty = true;
}

// Park the firing alarm for 5 minutes. An episode allows ALARM_MAX_SNOOZES
// snoozes; one more request (idle timeout or button) dismisses it for the
// day. nextFire goes to NVS, so the snooze survives deep sleep and power
// loss.
void alarmSnooze() {
  if (g_fireIdx >= 0 && g_fireIdx < g_alarmCount) {
    Alarm& a = g_alarms[g_fireIdx];
    if (a.snoozes >= ALARM_MAX_SNOOZES) { alarmDismiss(); return; }
    a.snoozes++;
    a.nextFire = (uint32_t)(time(nullptr) + ALARM_SNOOZE_SEC);
    saveAlarms();
  }
  g_alarmFiring = false;
  g_fireIdx = -1;
  g_screen = SCR_DASH;
  g_screenIdleUntil = 0;
  dirty = true;
}

// ---- notification LED (non-blocking, called from loop()) ----
static void ledWrite(bool r, bool g, bool b) {   // active-low channels
  pinMode(CYD_LED_RED, OUTPUT); pinMode(CYD_LED_GREEN, OUTPUT); pinMode(CYD_LED_BLUE, OUTPUT);
  digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
  digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
  digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

// One-shot preview while picking a preset in the editor (see
// handleAlarmsTouch): plays the chosen pattern on the LED and speaker for a
// few seconds so the user can see and hear what they picked.
static uint32_t g_ledPreviewUntil = 0;
static int      g_ledPreviewPreset = -1;

void previewAlarmLed(int preset) {
  g_ledPreviewPreset = preset;
  g_ledPreviewUntil = millis() + 3000;
}

// True while the alarm LED owns the LED: a firing alarm or a preset preview.
// The normal flight/status LED code skips its updates while this is set.
bool alarmLedBusy() {
  return g_alarmFiring ||
         (g_ledPreviewPreset >= 0 && (long)(millis() - g_ledPreviewUntil) < 0);
}

static void ledPattern(int preset, unsigned long now) {
  switch (preset) {
    case 1:   // Rapid: fast white strobe
      ledWrite(1, 1, 1); if ((now / 150) % 2) ledWrite(0, 0, 0);
      break;
    case 2: { // Double: two quick blinks, then a pause
      unsigned long t = now % 1800;
      bool on = (t < 120) || (t >= 240 && t < 360);
      ledWrite(on, on, on);
      break;
    }
    case 3: { // Colors: cycle red -> green -> blue
      int p = (now / 350) % 3;
      ledWrite(p == 0, p == 1, p == 2);
      break;
    }
    case 4:   // Pulse: brief white flash every ~1.6s
      ledWrite(now % 1600 < 80, now % 1600 < 80, now % 1600 < 80);
      break;
    default:  // Blink: white on/off ~1Hz, like the tracked-flight blink
      ledWrite(1, 1, 1); if ((now / 500) % 2) ledWrite(0, 0, 0);
      break;
  }
}

// ---- notification speaker (non-blocking, driven with the LED) ----
// GPIO 26 is the CYD's JST speaker connector through the onboard amp. Each
// preset's beep pattern mirrors its LED cadence so the sound matches the
// light: Rapid is a fast beep train, Double is two quick beeps then a pause,
// Colors walks up a triad one note per color step, Pulse is a soft two-note
// chime per flash, and Blink is a plain ~1Hz beep. toneWrite() is gated on
// the requested frequency so the LEDC channel isn't re-attached every tick.
#define SPK_PIN 26
static int g_toneFreq = -1;   // currently playing freq, -1 = silent

static void toneWrite(int freq) {
  if (freq == g_toneFreq) return;
  g_toneFreq = freq;
#ifdef CYD_E32R40T
  // The E32R40T gates its FM8002E amp behind GPIO 4 (active-low shutdown) -
  // enable it once so tone() on GPIO 26 actually reaches the speaker.
  static bool ampOn = false;
  if (!ampOn) { pinMode(4, OUTPUT); digitalWrite(4, LOW); ampOn = true; }
#endif
  if (freq <= 0) noTone(SPK_PIN);
  else           tone(SPK_PIN, freq);
}

static void tonePattern(int preset, unsigned long now) {
  switch (preset) {
    case 1:   // Rapid: fast high beeps on the strobe cadence
      toneWrite((now / 150) % 2 ? 0 : 1047);            // C6
      break;
    case 2: { // Double: two quick beeps per 1.8s cycle (B5 then G5)
      unsigned long t = now % 1800;
      toneWrite((t < 120) ? 988 : (t >= 240 && t < 360) ? 784 : 0);
      break;
    }
    case 3: { // Colors: C5->E5->G5, one note per color step (350ms)
      const int f[3] = {523, 659, 784};
      toneWrite(f[(now / 350) % 3]);
      break;
    }
    case 4: { // Pulse: soft two-note chime each 1.6s cycle
      unsigned long t = now % 1600;
      toneWrite((t < 160) ? 880 : (t >= 320 && t < 480) ? 659 : 0);
      break;
    }
    default:  // Blink: single beep pulsing ~1Hz with the LED
      toneWrite((now / 500) % 2 ? 0 : 988);             // B5
      break;
  }
}

void updateAlarmLed(unsigned long now) {
  static bool wasActive = false;
  int preset = -1;
  if (g_alarmFiring)
    preset = (g_fireIdx >= 0 && g_fireIdx < g_alarmCount)
                 ? g_alarms[g_fireIdx].preset : 0;
  else if (g_ledPreviewPreset >= 0 && (long)(now - g_ledPreviewUntil) < 0)
    preset = g_ledPreviewPreset;
  if (preset < 0) {
    g_ledPreviewPreset = -1;
    if (wasActive) { ledWrite(0, 0, 0); toneWrite(0); wasActive = false; }
    return;
  }
  wasActive = true;
  ledPattern(preset, now);
  tonePattern(preset, now);
}

// ---- drawing ----
// Format an alarm time honoring the 12/24-hour setting: "7:30 AM" or "19:30".
void fmtAlarmTime(char* buf, size_t n, int h, int m) {
  if (g_clock24) {
    snprintf(buf, n, "%d:%02d", h, m);
  } else {
    int h12 = h % 12; if (!h12) h12 = 12;
    snprintf(buf, n, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
  }
}

// Small bell glyph shown in the header's top marker slot while any alarm is
// enabled (above the AM/PM text, which occupies the bottom slot in 12h mode).
void drawAlarmBell(int x, int y, uint16_t bg) {
  tft.fillTriangle(x + 4, y, x, y + 6, x + 8, y + 6, TFT_YELLOW);
  tft.fillRect(x, y + 6, 9, 2, TFT_YELLOW);
  tft.fillRect(x + 3, y + 8, 3, 2, TFT_YELLOW);   // clapper
  (void)bg;
}

// Alarms editor screen: "Alarms > N". Enable toggle, a time stepper (tap the
// time itself for keyboard entry), weekday toggles, notify-preset stepper,
// and a < / New-or-> / Del footer.
void drawAlarms() {
  Alarm& a = g_alarms[g_alarmIdx];
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, DISP_W, 28, g_clockCol);
  tft.setTextColor(btnFg(g_clockCol), g_clockCol);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.printf("Alarms > %d", g_alarmIdx + 1);
  backBtn("Back");

  // Delete confirmation replaces the editor body (same Yes/No layout as the
  // Reset confirmation). Back still works and cancels it.
  if (g_alarmDelConfirm) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(10, 60);
    tft.printf("Delete alarm %d?", g_alarmIdx + 1);
    char tb[16];
    fmtAlarmTime(tb, sizeof tb, a.h, a.m);
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setCursor(10, 86);
    tft.print(tb);
    tft.setTextFont(1);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(10, 116);
    tft.print("This cannot be undone.");
    tft.fillRoundRect(CX - 130, 180, 110, 34, 6, dangerCol());
    tft.setTextColor(btnFg(dangerCol()), dangerCol());
    tft.setTextFont(2);
    tft.drawCentreString("Yes", CX - 75, 189, 2);
    themeBtn(CX + 20, 180, 110, 34, 6);
    tft.setTextColor(btnFg(btnCol()), btnCol());
    tft.drawCentreString("No", CX + 75, 189, 2);
    return;
  }

  // Enabled toggle (same row style as the Sleep screen)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 44);
  tft.print("Enabled");
  tft.setTextColor(a.en ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(150, 44);
  tft.print(a.en ? "ON" : "OFF");
  themeBtn(RX(230), 40, 82, 24, 5);
  tft.setTextColor(btnFg(btnCol()), btnCol());
  tft.setTextFont(FONT_AUX);
  tft.setCursor(RX(250), 47);
  tft.print("Toggle");

  drawTimeAdj(80, "Time", a.h, a.m);

  // Days of week: seven toggles, bit 0 = Sunday. Selected days use the theme
  // color; unselected stay grey.
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 133);
  tft.print("Days");
  const char* dl = "SMTWTFS";
  for (int i = 0; i < 7; i++) {
    bool sel = (a.days >> i) & 1;
    int bx = 64 + i * (36 + (DISP_W - 320) / 6);
    uint16_t c = sel ? btnCol() : disabledCol();
    if (sel) themeBtn(bx, 128, 32, 26, 4);   // white outline if near-black
    else tft.fillRoundRect(bx, 128, 32, 26, 4, c);
    tft.setTextColor(btnFg(c), c);
    tft.setCursor(bx + 11, 133);
    tft.print(dl[i]);
  }

  // Notify preset stepper
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 172);
  tft.print("Notify");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.drawCentreString(kAlarmPresets[a.preset], RX(200), 172, 2);
  adjPair(TADJ_MX, 166);

  // Footer: < prev (left) | Del (center) | > next or New (right)
  tft.setTextFont(2);
  tft.setTextColor(btnFg(btnCol()), btnCol());
  if (g_alarmIdx > 0) {
    themeBtn(8, 206, 44, 26, 5);
    tft.drawCentreString("<", 30, 211, 2);
  }
  if (g_alarmCount > 1) {
    tft.fillRoundRect(CX - 34, 206, 68, 26, 5, dangerCol());
    tft.setTextColor(btnFg(dangerCol()), dangerCol());
    tft.drawCentreString("Del", CX, 211, 2);
    tft.setTextColor(btnFg(btnCol()), btnCol());
  }
  if (g_alarmIdx + 1 < g_alarmCount) {
    themeBtn(RX(268), 206, 44, 26, 5);
    tft.drawCentreString(">", RX(290), 211, 2);
  } else if (g_alarmCount < MAX_ALARMS) {
    themeBtn(RX(228), 206, 84, 26, 5);
    tft.drawCentreString("New", RX(270), 211, 2);
  }
}

// Full-screen alert while an alarm is firing: big Dismiss / Snooze buttons.
void drawAlarmFire() {
  Alarm& a = g_alarms[constrain(g_fireIdx, 0, g_alarmCount - 1)];
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, DISP_W, 52, TFT_MAROON);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.drawCentreString("ALARM", CX, 12, 4);

  char tb[16];
  fmtAlarmTime(tb, sizeof tb, a.h, a.m);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString(tb, CX, 78, 4);

  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  if (a.snoozes > 0) {
    char sb[24];
    snprintf(sb, sizeof sb, "Snoozed %d/%d", a.snoozes, ALARM_MAX_SNOOZES);
    tft.drawCentreString(sb, CX, 120, 2);
  } else {
    tft.drawCentreString(kAlarmPresets[a.preset], CX, 120, 2);
  }

  themeBtn(CX - 144, 158, 140, 62, 8);
  tft.setTextColor(btnFg(btnCol()), btnCol());
  tft.drawCentreString("Dismiss", CX - 74, 180, 2);
  themeBtn(CX + 4, 158, 140, 62, 8);
  tft.drawCentreString("Snooze", CX + 74, 174, 2);
  tft.drawCentreString("5 min", CX + 74, 194, 2);
}

// ---- touch ----
static void applyAlarmTimeAdj(int hit) {
  Alarm& a = g_alarms[g_alarmIdx];
  switch (hit) {
    case 1: a.h = (a.h + 1) % 24; break;    // hour wrap flips AM/PM on its own
    case 2: a.h = (a.h + 23) % 24; break;
    case 3: a.m = (a.m + 1) % 60; break;
    case 4: a.m = (a.m + 59) % 60; break;
  }
  rearmAlarm(g_alarmIdx);    // reschedule from the new time
  saveAlarms();
}

void handleAlarmsTouch(uint16_t x, uint16_t y) {
  Alarm& a = g_alarms[g_alarmIdx];
  if (inRect(x, y, RX(265), 4, RX(315), 24)) { g_alarmDelConfirm = false; g_screen = SCR_DASH; dirty = true; return; }
  // Delete confirmation: Yes removes the alarm, No (or Back) cancels.
  if (g_alarmDelConfirm) {
    if (inRect(x, y, CX - 130, 180, CX - 20, 214)) {
      g_alarmDelConfirm = false;
      for (int i = g_alarmIdx; i < g_alarmCount - 1; i++)
        g_alarms[i] = g_alarms[i + 1];
      g_alarmCount--;
      // A pending snooze rides along inside the shifted struct (nextFire),
      // so no index fixup is needed here.
      if (g_alarmIdx >= g_alarmCount) g_alarmIdx = g_alarmCount - 1;
      saveAlarms();
      dirty = true;
    } else if (inRect(x, y, CX + 20, 180, CX + 130, 214)) {
      g_alarmDelConfirm = false;
      dirty = true;
    }
    return;
  }
  if (inRect(x, y, RX(230), 40, RX(312), 64)) {   // Enabled toggle
    a.en = !a.en;
    rearmAlarm(g_alarmIdx);
    saveAlarms();
    dirty = true;
    return;
  }
  int hit = timeAdjHit(x, y, 80);
  if (hit == 7) {   // tapped the time itself -> manual HHMM keyboard entry
    char b[8];
    snprintf(b, sizeof b, "%02d%02d", a.h, a.m);
    g_alarmTimeStr = b;
    g_wifiSub = 8;
    g_screen = SCR_WIFI;
    dirty = true;
    return;
  }
  if (hit) { applyAlarmTimeAdj(hit); dirty = true; return; }
  for (int i = 0; i < 7; i++) {   // weekday toggles
    if (inRect(x, y, 64 + i * (36 + (DISP_W - 320) / 6), 128, 64 + i * (36 + (DISP_W - 320) / 6) + 32, 154)) {
      a.days ^= (1 << i);
      rearmAlarm(g_alarmIdx);
      saveAlarms();
      dirty = true;
      return;
    }
  }
  int np = adjPairHit(x, y, TADJ_MX, 166);   // notify preset stepper
  if (np) {
    a.preset = (a.preset + kNumAlarmPresets + np) % kNumAlarmPresets;
    previewAlarmLed(a.preset);   // play the picked pattern once on the LED
    saveAlarms();
    dirty = true;
    return;
  }
  // footer: < left | Del center | > or New right
  if (g_alarmIdx > 0 && inRect(x, y, 8, 206, 52, 232)) {
    g_alarmIdx--; dirty = true; return;
  }
  if (g_alarmCount > 1 && inRect(x, y, CX - 34, 206, CX + 34, 232)) {
    g_alarmDelConfirm = true;   // ask first; the actual delete happens on Yes
    dirty = true;
    return;
  }
  if (g_alarmIdx + 1 < g_alarmCount) {
    if (inRect(x, y, RX(268), 206, RX(312), 232)) { g_alarmIdx++; dirty = true; return; }
  } else if (g_alarmCount < MAX_ALARMS &&
             inRect(x, y, RX(228), 206, RX(312), 232)) {
    g_alarms[g_alarmCount] = Alarm{0, 7, 0, 0x3E, 0, 0, 0, 0};   // 7:00 AM, Mon-Fri
    g_alarmIdx = g_alarmCount++;
    saveAlarms();
    dirty = true;
    return;
  }
}

void handleAlarmFireTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, CX - 144, 158, CX - 4, 220)) alarmDismiss();
  else if (inRect(x, y, CX + 4, 158, CX + 144, 220)) alarmSnooze();
}
