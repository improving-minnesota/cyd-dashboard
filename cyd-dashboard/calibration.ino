// calibration.ino - on-device touch calibration.
//
// Lets the user recalibrate the touchscreen if buttons ever drift. A long press
// (10s) anywhere on the dashboard enters calibration; it draws a series of
// crosshairs and asks the user to tap each one, then computes and stores the
// linear raw->display transform in NVS.
//
// The transform is used by touchReadXY() (cyd-dashboard.ino) via the globals
// here. Defaults are the factory-measured values for this unit; calibration
// overrides them once run.

// Calibration parameters (g_calScaleX/OffX/Y, g_calState, CalState enum) are
// defined in cyd-dashboard.ino so they are visible to the touch code there.

int   g_calIdx = 0;
bool  g_calCollect = false;   // true while waiting for the user to tap a target
uint32_t g_calCollectStart = 0;

// The 5 target display points to tap, in order.
struct CalPt { int x, y; const char* label; };
static const CalPt g_calTargets[5] = {
  { 20,  20, "TOP-LEFT" },
  {300,  20, "TOP-RIGHT"},
  { 20, 220, "BOTTOM-LEFT"},
  {300, 220, "BOTTOM-RIGHT"},
  {160, 120, "CENTER"   },
};

// Raw readings captured at each target.
uint16_t g_calRaw[5][2];   // [i][0]=rawX, [i][1]=rawY

void calDrawTarget(int i) {
  const CalPt& p = g_calTargets[i];
  tft.fillScreen(TFT_BLACK);
  tft.drawFastHLine(p.x - 10, p.y, 20, TFT_GREEN);
  tft.drawFastVLine(p.x, p.y - 10, 20, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 5);
  tft.print("Tap the crosshair: ");
  tft.print(p.label);
  tft.setCursor(8, 225);
  tft.print(String(i + 1) + "/5");
}

// Begin calibration. Returns immediately; the state machine runs in calPoll()
// from loop(). Because calibration is triggered by a long-press, the user's
// finger is still on the screen when this fires - so we first wait ~2s
// (CAL_LOADING) for them to lift, then show the first target so its tap is not
// mistaken for the still-held calibration trigger.
#define CAL_LOADING_MS 2000UL

void calBegin() {
  g_calIdx = 0;
  g_calCollect = false;
  g_calState = CAL_LOADING;
  g_calCollectStart = millis();
  g_screen = SCR_CALIB;   // run calibration on its own screen
  dirty = true;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(30, 100);
  tft.print("Prepare for Calibration...");
}

// Called every loop while in a calibration screen. Handles tap collection,
// progress, and finishing.
void calPoll() {
  if (g_calState == CAL_NONE) return;

  if (g_calState == CAL_LOADING) {
    // Wait out the delay so the still-held long-press is released before the
    // first target is shown and tapped.
    if (millis() - g_calCollectStart >= CAL_LOADING_MS) {
      g_calState = CAL_TARGET;
      calDrawTarget(0);
    }
  } else if (g_calState == CAL_TARGET) {
    // read a press (rising edge) while holding the screen clean of redraws
    uint16_t x, y, rx, ry;
    static bool prevPressed = false;
    bool pressed = touchReadXY(x, y, &rx, &ry);
    if (pressed && !prevPressed) {
      // capture the raw reading (median of a few) for the current target
      g_calRaw[g_calIdx][0] = rx;
      g_calRaw[g_calIdx][1] = ry;
      g_calIdx++;
      if (g_calIdx >= 5) {
        // compute + save
        calCompute();
        g_calState = CAL_DONE;
        dirty = true;
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(10, 100);
        tft.print("Calibration saved!");
        g_calCollect = false;
        return;
      } else {
        calDrawTarget(g_calIdx);
      }
    }
    prevPressed = pressed;
  } else if (g_calState == CAL_DONE) {
    // show the "saved" message briefly, then advance the flow
    static uint32_t doneAt = 0;
    if (doneAt == 0) doneAt = millis();
    if (millis() - doneAt > 1500) {
      doneAt = 0;
      g_calState = CAL_NONE;
      if (g_bootStage == BOOT_CALIB) {
        // Boot-time calibration (fresh device / after a full factory reset).
        // Continue the first-boot wizard: gather WiFi if none is saved, else
        // finish up on the dashboard.
        if (g_savedSsid.length() == 0) {
          enterWifiScreen();
          g_bootStage = BOOT_WIFI;
        } else {
          g_bootStage = BOOT_DONE;
          g_screen = SCR_DASH;
        }
      } else {
        g_screen = SCR_SETTINGS;   // manual calibration from Settings
      }
      dirty = true;
    }
  }
}

// Fit the linear transform from the 5 captured raw points and store to NVS.
void calCompute() {
  // Use the corner points (indices 0..3) for a robust min/max on each axis.
  uint16_t rxMin = 4095, rxMax = 0, ryMin = 4095, ryMax = 0;
  for (int i = 0; i < 4; i++) {
    rxMin = min(rxMin, g_calRaw[i][0]); rxMax = max(rxMax, g_calRaw[i][0]);
    ryMin = min(ryMin, g_calRaw[i][1]); ryMax = max(ryMax, g_calRaw[i][1]);
  }

  // dispX spans 20..300 (280 wide) from rawY min..max.
  // dispY spans 20..220 (200 tall) from rawX min..max.
  if (ryMax - ryMin > 100) {
    long scale = ((long)(ryMax - ryMin)) * 1000L / 280L;  // raw per display unit
    g_calScaleX = (int)scale;
    g_calOffX = (long)ryMin - (long)(20 * scale) / 1000L;
  }
  if (rxMax - rxMin > 100) {
    long scale = ((long)(rxMax - rxMin)) * 1000L / 200L;
    g_calScaleY = (int)scale;
    g_calOffY = (long)rxMin - (long)(20 * scale) / 1000L;
  }

  prefs.begin("flight", false);
  prefs.putInt("calsx", g_calScaleX);
  prefs.putLong("calox", g_calOffX);
  prefs.putInt("calsy", g_calScaleY);
  prefs.putLong("caloy", g_calOffY);
  prefs.end();
}

// Load calibration parameters from NVS (or leave defaults).
// NOTE: setup() already opened the "flight" namespace, so we must NOT call
// prefs.begin/end here - doing so would close the namespace and make setup()'s
// later prefs.get() reads (e.g. the WiFi ssid/pass) fail/return defaults.
void calLoad() {
  g_calScaleX = prefs.getInt("calsx", g_calScaleX);
  g_calOffX   = prefs.getLong("calox", g_calOffX);
  g_calScaleY = prefs.getInt("calsy", g_calScaleY);
  g_calOffY   = prefs.getLong("caloy", g_calOffY);
}
