// settings.ino - Settings screen and its sub-screens (Flight Tracker config,
// Sleep Mode config, Reset confirmation) plus their small drawing helpers.
// Part of the cyd-dashboard sketch; shares globals/helpers (prefs, dirty,
// g_screen, inRect(), rowMinus()/rowPlus(), saveFloat()/saveInt(), etc.)
// declared in cyd-dashboard.ino.

// The Settings screen is a pure navigation list - it holds no settings itself;
// each row jumps to a dedicated sub-page. Settings live on those pages.

void drawSettings() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Settings");

  // Back button (top-right)
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // 2-column grid, alphabetical: About, Calibrate Touch, Flight Tracker,
  // General, Help, Location, Pool Temp, Sleep Mode, Reset, WiFi. Reset is kept
  // at the bottom-left cell and drawn red (it is destructive). Touch handling
  // in handleTouch() (cyd-dashboard.ino) mirrors this layout.
  const char* items[] = { "About", "Calibrate Touch", "Flight Tracker", "General",
                          "Help", "Location", "Pool Temp", "Sleep Mode",
                          "Reset", "WiFi" };
  const int n = 10, rowH = 34, step = 38;
  const int colX[2] = { 10, 164 };   // left / right column x
  const int colW = 146;              // button width
  const int y0 = 42;                 // first row y
  for (int i = 0; i < n; i++) {
    int row = i / 2, col = i % 2;
    int y = y0 + row * step;
    uint16_t c = (strcmp(items[i], "Reset") == 0) ? TFT_MAROON : TFT_NAVY;   // Reset -> red
    tft.fillRoundRect(colX[col], y, colW, rowH, 6, c);
    tft.setTextColor(TFT_WHITE, c);
    tft.setTextFont(2);
    tft.setCursor(colX[col] + 8, y + (rowH - 16) / 2);
    tft.print(items[i]);
  }
}

// ---- About page ----
void drawAbout() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("About");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // Body (compact, to leave room for the upgrade section below)
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 40);
  tft.print("CYD (Cheap Yellow Display) Dashboard");

  tft.setTextFont(1);
  tft.setCursor(8, 62);
  tft.print("Developer: Paul Hassinger");

  tft.setCursor(8, 76);
  tft.print("Email: paul.hassinger (at) improving.com");

  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor(8, 92);
  tft.print("github.com/improving-minnesota/cyd-dashboard");

  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(8, 112);
  tft.print("Version: v");
  tft.print(kVersion);

  // Divider above the upgrade section
  tft.drawFastHLine(8, 130, 304, TFT_DARKGREY);

  // Upgrade section
  tft.setTextFont(2);
  if (g_updateState == 2) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 140);
    tft.print("Upgrade Available:");
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setCursor(8, 164);
    tft.print("(v");
    tft.print(g_updateLatest);
    tft.print(")");
    // Install button (right)
    tft.fillRoundRect(218, 140, 76, 26, 5, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setCursor(236, 147);
    tft.print("Install");
  } else if (g_updateState == 3) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 140);
    tft.print("No Updates Available");
  } else if (g_updateState == 4) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(8, 140);
    tft.print("Update Check Failed");
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 140);
    tft.print("Checking for updates...");
  }
}

void handleAboutTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }
  // Install button (only shown when an update is available)
  if (g_updateState == 2 && inRect(x, y, 218, 140, 294, 166)) {
    g_otaVersion = g_updateLatest;
    g_otaUrl = g_updateAsset;
    g_otaSha256 = g_updateDigest;
    g_otaActive = true;
    return;
  }
}

// ---- Help page (scrollable) ----
// Shows the end-user guide on the device. Text is pre-wrapped to fit and
// scrolls vertically with a scrollbar on the right.
#define HELP_TOP    36
#define HELP_BOT    222
#define HELP_X      8
#define HELP_W      286
#define HELP_SB_X   304
#define HELP_SB_W   10
#define HELP_LINEH  9

static const char* const kHelpLines[] = {
  "CYD Dashboard",
  "---------------",
  "Built for the Cheap Yellow",
  "Display (CYD): the",
  "ESP32-2432S028R board with a",
  "built-in 2.8\" 320x240 color",
  "touchscreen. It shows the",
  "time, weather, nearby flights,",
  "and Govee pool temp over",
  "WiFi. Made for this board",
  "only (TFT/touch wiring is",
  "CYD-specific).",
  "",
  "FEATURES",
  "Clock: big time and date.",
  "Weather: temp, feels-like,",
  "humidity, sunrise/sunset,",
  "and a 7-day forecast.",
  "Flights: live aircraft with",
  "a radar, callsign, route,",
  "and airline logo. The LED",
  "flashes red/green for DFW",
  "flights and blue for a top",
  "US airport.",
  "  Tap the aircraft count",
  "  (e.g. '6 aircraft') on the",
  "  idle screen for the last",
  "  flight's details.",
  "Govee pool temp monitor:",
  "  thermometer temp plus a",
  "  history graph with low /",
  "  average / high.",
  "  Tap the Pool reading to",
  "  open its history graph.",
  "Weather temp history: the",
  "  current temperature is",
  "  logged to flash every 10",
  "  min and graphed with low /",
  "  average / high. Always on.",
  "  Tap the big temperature on",
  "  the idle screen to open it.",
  "Sleep: deep-sleeps overnight",
  "and wakes on touch.",
  "",
  "GETTING STARTED",
  "Upload a compiled image to",
  "the device first - see",
  "DEVELOPER.md for build and",
  "flash instructions.",
  "First boot: the device",
  "  calibrates the touchscreen",
  "  then asks for your WiFi.",
  "To redo either later:",
  "  Calibrate: Settings ->",
  "    General -> Calibrate",
  "    Touch, or hold the",
  "    screen 10 sec.",
  "  WiFi: Settings -> WiFi.",
  "Weather and flights work",
  "out of the box. Adding your",
  "OpenSky credentials raises",
  "the rate limit and removes",
  "the yellow 'anonymous'",
  "warning. Optional",
  "credentials below.",
  "Airline logos (optional):",
  "  see DEVELOPER.md ->",
  "  'Airline logos'.",
  "",
  "TYPING",
  "On a keyboard, tap inside",
  "the text field to place the",
  "cursor, then type or delete",
  "in the middle of a value.",
  "",
  "GETTING CREDENTIALS",
  "WiFi: from your router - the",
  "   network name and password.",
  "OpenSky (flights, optional):",
  "   free account at",
  "   opensky-network.org; create",
  "   an API client under My",
  "   OpenSky for a client ID and",
  "   secret.",
  "Govee (pool, optional): free",
  "   developer account at",
  "   developer.govee.com;",
  "   generate an API key.",
  "",
  "SETTINGS GUIDE (defaults)",
  "General: auto-update (on),",
  "   clock color (blue).",
  "Location: your coordinates;",
  "   IP guess on first boot.",
  "   Search Address keeps your",
  "   last search so you can fix it.",
  "WiFi: your network + API keys.",
  "Flight Tracker: on/off, units",
  "   (imperial), radius 3.5 mi,",
  "   ceiling 15000 ft, poll 60s,",
  "   timer bar on/off (off),",
  "   home airport, blink LED on",
  "   noteworthy flight (on).",
  "Sleep Mode: on; 10 PM - 8 AM,",
  "   wake 10 min.",
  "Pool Temp: off; add Govee key,",
  "   pick thermometer.",
  "Calibrate Touch: if taps land",
  "   in the wrong spot, rerun it.",
  "About: version, author, update.",
  "Reset: All (settings, files,",
  "   pool data, calibration) or",
  "   Settings (settings only).",
  "   Both reboot; Cancel keeps",
  "   everything. All re-calibrates",
  "   on the next boot.",
  "",
  "TROUBLESHOOTING",
  "Screen taps in the wrong spot?",
  "Press and hold anywhere on the",
  "screen for 10 seconds to",
  "recalibrate touch.",
  "",
  "Border colors: red = critical",
  "issue; yellow = running",
  "OpenSky anonymously (not an",
  "error - flights still work).",
  "",
  "UPDATES",
  "The device can update itself",
  "over WiFi from GitHub.",
  "General -> Auto-Update: on by",
  "default for release builds;",
  "checks once per day and",
  "installs new firmware.",
  "About -> shows if a newer",
  "version is available; tap",
  "Install to update now.",
  "Do not power off during an",
  "update.",
};
static const int kNumHelpLines = sizeof(kHelpLines) / sizeof(kHelpLines[0]);

void drawHelp() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Help");
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  const int viewH = HELP_BOT - HELP_TOP;
  const int contentH = kNumHelpLines * HELP_LINEH;
  int maxScroll = contentH - viewH;
  if (maxScroll < 0) maxScroll = 0;
  if (g_helpScroll > maxScroll) g_helpScroll = maxScroll;
  if (g_helpScroll < 0) g_helpScroll = 0;

  // Text clipped to the scroll area (absolute coords preserved).
  tft.setViewport(HELP_X, HELP_TOP, HELP_W, viewH, false);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  for (int i = 0; i < kNumHelpLines; i++) {
    int y = i * HELP_LINEH - g_helpScroll;
    if (y < -HELP_LINEH || y > viewH) continue;
    tft.setCursor(HELP_X, HELP_TOP + y);
    tft.print(kHelpLines[i]);
  }
  tft.resetViewport();

  // Scrollbar (right side).
  tft.fillRect(HELP_SB_X, HELP_TOP, HELP_SB_W, viewH, TFT_DARKGREY);
  int thumbH = (int)((long)viewH * viewH / contentH);
  if (thumbH < 16) thumbH = 16;
  if (thumbH > viewH) thumbH = viewH;
  int thumbY = HELP_TOP + (maxScroll > 0 ? ((viewH - thumbH) * g_helpScroll) / maxScroll : 0);
  tft.fillRoundRect(HELP_SB_X, thumbY, HELP_SB_W, thumbH, 3, TFT_LIGHTGREY);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, HELP_BOT + 2);
  tft.print("Tap right bar above/below the thumb to scroll.");
}

void handleHelpTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }  // Back

  const int viewH = HELP_BOT - HELP_TOP;
  const int contentH = kNumHelpLines * HELP_LINEH;
  int maxScroll = contentH - viewH;
  if (maxScroll < 0) maxScroll = 0;

  // Tapping the scrollbar track: above the thumb scrolls up, below scrolls down.
  if (x >= HELP_SB_X && x <= HELP_SB_X + HELP_SB_W && y >= HELP_TOP && y <= HELP_BOT) {
    int thumbH = (int)((long)viewH * viewH / contentH);
    if (thumbH < 16) thumbH = 16;
    if (thumbH > viewH) thumbH = viewH;
    int thumbY = HELP_TOP + (maxScroll > 0 ? ((viewH - thumbH) * g_helpScroll) / maxScroll : 0);
    if (y < thumbY) g_helpScroll -= (int)(viewH * 0.6);
    else if (y > thumbY + thumbH) g_helpScroll += (int)(viewH * 0.6);
    if (g_helpScroll < 0) g_helpScroll = 0;
    if (g_helpScroll > maxScroll) g_helpScroll = maxScroll;
    dirty = true;
  }
}

// ---- General page ----
// Preset clock-bar colors shown as tappable swatches (see "Clock Color").
// White/light-gray are intentionally excluded: the clock text is white, so a
// near-white bar would be unreadable. The two added colors are dark enough for
// white text.
static const uint16_t kClockColors[] = {
  TFT_BLUE, TFT_NAVY, TFT_CYAN, TFT_GREEN, TFT_DARKGREEN, TFT_DARKCYAN,
  TFT_YELLOW, TFT_ORANGE, TFT_RED, TFT_MAROON, TFT_PURPLE, TFT_MAGENTA,
};
static const int kNumClockColors = sizeof(kClockColors) / sizeof(kClockColors[0]);
static const int kClockSw = 46, kClockSh = 22, kClockGap = 4;
static const int kClockX0 = 8, kClockY0 = 44;
static const int kClockPerRow = 6;

void drawGeneral() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("General");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // Clock Color: label + swatches (tap to change)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 40);
  tft.print("Clock Color");
  for (int i = 0; i < kNumClockColors; i++) {
    int row = i / kClockPerRow, col = i % kClockPerRow;
    int x = kClockX0 + col * (kClockSw + kClockGap);
    int y = kClockY0 + row * (kClockSh + kClockGap);
    tft.fillRoundRect(x, y, kClockSw, kClockSh, 4, kClockColors[i]);
    if (g_clockCol == kClockColors[i]) {   // highlight the selected one
      tft.drawRect(x - 1, y - 1, kClockSw + 2, kClockSh + 2, TFT_WHITE);
    }
  }

  // Auto-Update toggle
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 112);
  tft.print("Auto-Update");
  tft.setTextColor(g_autoUpdate ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(150, 112);
  tft.print(g_autoUpdate ? "ON" : "OFF");
  tft.fillRoundRect(230, 108, 82, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(250, 115);
  tft.print("Toggle");

  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(8, 140);
  tft.print("Checks for new firmware once");
  tft.setCursor(8, 152);
  tft.print("a day and installs it.");
}

void handleGeneralTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }
  if (inRect(x, y, 230, 108, 312, 132)) {  // Auto-Update toggle
    g_autoUpdate = !g_autoUpdate;
    prefs.begin("flight", false); prefs.putBool("autoupd", g_autoUpdate); prefs.end();
    // Turning auto-update ON clears the last-scan date so it can try again today.
    if (g_autoUpdate) {
      g_lastScanDay = 0;
      prefs.begin("flight", false); prefs.putULong("lastscan", 0); prefs.end();
    }
    dirty = true;
    return;
  }
  // Clock Color swatch taps
  for (int i = 0; i < kNumClockColors; i++) {
    int row = i / kClockPerRow, col = i % kClockPerRow;
    int x0 = kClockX0 + col * (kClockSw + kClockGap);
    int y0 = kClockY0 + row * (kClockSh + kClockGap);
    if (inRect(x, y, x0, y0, x0 + kClockSw, y0 + kClockSh)) {
      g_clockCol = kClockColors[i];
      prefs.begin("flight", false); prefs.putUInt("clkcol", g_clockCol); prefs.end();
      dirty = true;
      return;
    }
  }
}

// ---- Location page ----
void drawLocation() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Location");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // current coordinates + edit button
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 44);
  tft.print("Lat, Lon");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 62);
  tft.printf("%.4f, %.4f", g_lat, g_lon);
  tft.fillRoundRect(240, 40, 72, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(254, 47);
  tft.print("Set");

  // Search by address
  tft.fillRoundRect(10, 96, 300, 34, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(20, 104);
  tft.print("Search Address");

  // Find by IP
  tft.fillRoundRect(10, 140, 300, 34, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(20, 148);
  tft.print("Find by IP");

  // Confirmation / error from the last address search
  tft.setTextFont(1);
  if (g_lastPlace.length()) {
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setCursor(8, 184);
    tft.print("Location:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(8, 196);
    tft.print(g_lastPlace.substring(0, 38));
  } else if (String(lastErr) == "no match") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(8, 184);
    tft.print("No match for that address.");
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, 196);
    tft.print("Search again or set lat/lon.");
  } else {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, 190);
    tft.print("Find by IP guesses location from");
    tft.setCursor(8, 200);
    tft.print("the network the device is on.");
  }
}

// Flight Tracker settings screen
void drawFtracker() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Flight Tracker");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  if (g_ftPage == 0) {
    // Page 1: Enabled, Units, sliders
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, 40);
    tft.print("Enabled");
    tft.setTextColor(g_trackEnabled ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(150, 40);
    tft.print(g_trackEnabled ? "ON" : "OFF");
    tft.fillRoundRect(230, 36, 82, 24, 5, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(250, 43);
    tft.print("Toggle");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, 76);
    tft.print("Units");
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setCursor(150, 76);
    tft.print(g_metric ? "Metric" : "Imperial");
    tft.fillRoundRect(230, 72, 82, 24, 5, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(250, 79);
    tft.print("Toggle");

    drawSlider(104, "Radius", g_radiusMi, 1.0f, 10.0f, 0.5f, 1);
    drawSlider(142, "Ceiling", (float)g_ceilingFt, 3000, 30000, 1000, 0);
    drawSlider(180, "Poll (s)", (float)g_pollSec, 10, 300, 10, 0);
  } else {
    // Page 2: Blink-for-flight toggle + Enable timer toggle + Home-airport
    // route setting + Credentials
    // Blink for Flight toggle
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, 40);
    tft.print("Blink for Flight");
    tft.setTextColor(g_blinkForFlight ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(150, 40);
    tft.print(g_blinkForFlight ? "ON" : "OFF");
    tft.fillRoundRect(230, 36, 82, 24, 5, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(250, 43);
    tft.print("Toggle");

    // Enable timer toggle
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, 76);
    tft.print("Enable timer");
    tft.setTextColor(g_showTimer ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(150, 76);
    tft.print(g_showTimer ? "ON" : "OFF");
    tft.fillRoundRect(230, 72, 82, 24, 5, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(250, 79);
    tft.print("Toggle");

    drawEditRow(112, "Home airport", g_homeAirport.length() ? g_homeAirport : "--");
    tft.setTextFont(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, 144);
    tft.print("Airport not shown in route data.");
    tft.setCursor(8, 156);
    tft.print("Empty = show all (default).");

    tft.fillRoundRect(10, 188, 300, 24, 6, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextFont(2);
    tft.setCursor(20, 194);
    tft.print("OpenSky Credentials");
  }

  // Pager (bottom): left/right arrows + page indicator
  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(140, 222);
  tft.printf("Page %d/2", g_ftPage + 1);
  if (g_ftPage > 0) {
    tft.fillRoundRect(12, 214, 44, 26, 6, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextFont(2);
    tft.setCursor(27, 220);
    tft.print("<");
  }
  if (g_ftPage < 1) {
    tft.fillRoundRect(264, 214, 44, 26, 6, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextFont(2);
    tft.setCursor(279, 220);
    tft.print(">");
  }
}

void handleFtrackerTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }
  // Pager arrows
  if (inRect(x, y, 12, 214, 56, 240)) { if (g_ftPage > 0) { g_ftPage--; dirty = true; } return; }
  if (inRect(x, y, 264, 214, 308, 240)) { if (g_ftPage < 1) { g_ftPage++; dirty = true; } return; }

  if (g_ftPage == 0) {
    if (inRect(x, y, 230, 36, 312, 60)) {  // Enabled toggle
      g_trackEnabled = !g_trackEnabled;
      prefs.begin("flight", false); prefs.putBool("track", g_trackEnabled); prefs.end();
      dirty = true;
      return;
    }
    if (inRect(x, y, 230, 72, 312, 96)) {  // Units toggle
      g_metric = !g_metric;
      prefs.begin("flight", false); prefs.putBool("metric", g_metric); prefs.end();
      dirty = true;
      return;
    }
    // Radius / Ceiling / Poll sliders
    if      (rowMinus(x, y, 104)) { g_radiusMi  = constrain(g_radiusMi - 0.5f, 1.0f, 10.0f);  saveFloat("radius", g_radiusMi); }
    else if (rowPlus(x, y, 104))  { g_radiusMi  = constrain(g_radiusMi + 0.5f, 1.0f, 10.0f);  saveFloat("radius", g_radiusMi); }
    else if (rowMinus(x, y, 142)) { g_ceilingFt = constrain(g_ceilingFt - 1000, 3000, 30000); saveInt("ceiling", g_ceilingFt); }
    else if (rowPlus(x, y, 142))  { g_ceilingFt = constrain(g_ceilingFt + 1000, 3000, 30000); saveInt("ceiling", g_ceilingFt); }
    else if (rowMinus(x, y, 180)) { g_pollSec   = constrain(g_pollSec - 10, 10, 300);         saveInt("poll", g_pollSec); }
    else if (rowPlus(x, y, 180))  { g_pollSec   = constrain(g_pollSec + 10, 10, 300);         saveInt("poll", g_pollSec); }
    return;
  }

  // Page 2
  if (inRect(x, y, 230, 36, 312, 60)) {  // Blink for Flight toggle
    g_blinkForFlight = !g_blinkForFlight;
    prefs.begin("flight", false); prefs.putBool("blinkf", g_blinkForFlight); prefs.end();
    dirty = true;
    return;
  }
  if (inRect(x, y, 230, 72, 312, 96)) {  // Enable timer toggle
    g_showTimer = !g_showTimer;
    prefs.begin("flight", false); prefs.putBool("timer", g_showTimer); prefs.end();
    dirty = true;
    return;
  }
  if (inRect(x, y, 270, 112, 312, 136)) {  // Edit home airport
    g_screen = SCR_WIFI;
    g_wifiSub = 11;
    dirty = true;
    return;
  }
  if (inRect(x, y, 10, 188, 310, 212)) {  // OpenSky credentials
    g_screen = SCR_WIFI;
    g_wifiSub = 3;
    dirty = true;
    return;
  }
}

// Reset screen. Two steps: choose what to reset (All / Settings / Cancel), then
// a confirmation prompt before anything is wiped and the device reboots.
void drawReset() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_MAROON);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Reset Device");

  if (g_resetConfirm != 0) {
    // Confirmation prompt for the chosen reset.
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(10, 64);
    tft.print("Are you sure?");
    tft.setTextFont(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    int ty = 96;
    if (g_resetConfirm == 1) {          // All: full factory reset incl. calibration
      tft.setCursor(10, ty); tft.print("All: settings, files, and"); ty += 12;
      tft.setCursor(10, ty); tft.print("credentials will be cleared."); ty += 12;
      tft.setCursor(10, ty); tft.print("Touch calibration will be cleared."); ty += 12;
      tft.setCursor(10, ty); tft.print("Pool & weather history deleted."); ty += 12;
      tft.setCursor(10, ty); tft.print("The device will reboot."); ty += 12;
    } else {                            // Settings: keeps calibration so it stays usable
      tft.setCursor(10, ty); tft.print("Settings: settings and"); ty += 12;
      tft.setCursor(10, ty); tft.print("credentials will be cleared."); ty += 12;
      tft.setCursor(10, ty); tft.print("Pool temp history is kept."); ty += 12;
      tft.setCursor(10, ty); tft.print("Touch calibration is kept."); ty += 12;
      tft.setCursor(10, ty); tft.print("The device will reboot."); ty += 12;
    }
    tft.setCursor(10, ty);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("This cannot be undone.");
    // Yes / No
    tft.fillRoundRect(30, 180, 110, 34, 6, TFT_MAROON);
    tft.setTextColor(TFT_WHITE, TFT_MAROON);
    tft.setTextFont(2);
    tft.setCursor(70, 189);
    tft.print("Yes");
    tft.fillRoundRect(180, 180, 110, 34, 6, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setCursor(222, 189);
    tft.print("No");
    return;
  }

  // Choose what to reset.
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(10, 52);
  tft.print("Choose what to reset:");
  tft.setTextFont(1);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(10, 76);
  tft.print("All: settings + files");
  tft.setCursor(10, 86);
  tft.print("     (incl. pool temp history).");
  tft.setCursor(10, 102);
  tft.print("Settings: settings &");
  tft.setCursor(10, 112);
  tft.print("     credentials only.");
  tft.setCursor(10, 128);
  tft.print("Cancel: go back without");
  tft.setCursor(10, 138);
  tft.print("     changing anything.");
  tft.setCursor(10, 154);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print("Selected option reboots the device.");

  // All / Settings / Cancel
  tft.fillRoundRect(8, 180, 96, 34, 6, TFT_MAROON);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.setTextFont(2);
  tft.setCursor(34, 189);
  tft.print("All");

  tft.fillRoundRect(112, 180, 96, 34, 6, TFT_ORANGE);
  tft.setTextColor(TFT_WHITE, TFT_ORANGE);
  tft.setCursor(126, 189);
  tft.print("Settings");

  tft.fillRoundRect(216, 180, 96, 34, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(240, 189);
  tft.print("Cancel");
}

// Sleep Mode settings screen
void drawSleep() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Sleep Mode");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // Enabled toggle
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 44);
  tft.print("Enabled");
  tft.setTextColor(g_sleepOn ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(150, 44);
  tft.print(g_sleepOn ? "ON" : "OFF");
  tft.fillRoundRect(230, 40, 82, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(250, 47);
  tft.print("Toggle");

  drawEditRow(84, "Start (HHMM)", g_sleepStartStr);
  drawEditRow(124, "End (HHMM)", g_sleepEndStr);
  drawEditRow(164, "Wake duration (min)", g_wakeStr);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 214);
#if TOUCH_IRQ_ENABLED
  tft.print("Deep sleep; pool temp keeps logging. Touch");
  tft.setCursor(8, 224);
  tft.print("wakes it for the wake duration, then sleeps.");
#else
  tft.print("Deep sleep; pool temp keeps logging. Ends at");
  tft.setCursor(8, 224);
  tft.print("the scheduled time (touch-wake needs wiring).");
#endif
}

void drawEditRow(int y, const char* label, const String& value) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, y);
  tft.print(label);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor(210, y + 5);
  tft.print(value);
  tft.fillRoundRect(270, y, 42, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(280, y + 6);
  tft.print("Edit");
}

void handleSleepTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }
  if (inRect(x, y, 230, 40, 312, 64)) {  // Toggle
    g_sleepOn = !g_sleepOn;
    prefs.begin("flight", false); prefs.putBool("sleepon", g_sleepOn); prefs.end();
    dirty = true;
    return;
  }
  if (inRect(x, y, 270, 84, 312, 108)) { g_screen = SCR_WIFI; g_wifiSub = 6; dirty = true; return; }  // start
  if (inRect(x, y, 270, 124, 312, 148)) { g_screen = SCR_WIFI; g_wifiSub = 7; dirty = true; return; }  // end
  if (inRect(x, y, 270, 164, 312, 188)) { g_screen = SCR_WIFI; g_wifiSub = 8; dirty = true; return; }  // wake
}

// type 1 = float step, type 0 = int
void drawSlider(int y, const char* label, float val, float vmin, float vmax, float step, int type) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, y);
  tft.print(label);

  // [-] value [+]
  tft.fillRoundRect(170, y, 34, 24, 5, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(181, y + 5);
  tft.print("-");
  tft.fillRoundRect(238, y, 34, 24, 5, TFT_DARKGREY);
  tft.setCursor(249, y + 5);
  tft.print("+");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(118, y + 5);
  if (type) tft.printf("%.1f", val);
  else      tft.print((int)val);
  (void)vmin; (void)vmax; (void)step;
}
