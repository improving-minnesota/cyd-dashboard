// weather.ino - NTP clock, Open-Meteo weather, and idle/weather screen.

#include <time.h>

// Weather state (global across tabs)
float g_temp = 0.0f, g_humidity = 0.0f, g_feels = 0.0f;
char  g_sunrise[6] = "--:--", g_sunset[6] = "--:--";
float g_high[7] = {0}, g_low[7] = {0};
int   g_rain[7] = {0}, g_wcode[7] = {0};
int   g_wcode_cur = 0;     // current WMO weather code (for the idle icon)
bool  g_weatherValid = false;

extern unsigned long g_lastWeather;

// NTP / clock helpers
void setupNTP() {
  // US Central (UTC-6, DST +1)
  configTime(-6 * 3600, 3600, "pool.ntp.org", "time.nist.gov");
  // From this point on getLocalTime() applies the correct local offset.
  // Before this call it would report UTC, which could land inside the sleep
  // window right after a soft reset (OTA) and cause an unwanted deep sleep.
  g_timeReady = true;
}

String fmtClock() {
  struct tm t;
  // Explicit 0ms timeout: this is called every ~1s from the main loop, and
  // getLocalTime()'s default 5s timeout would block the whole loop (touch +
  // drawing) for that long on every call while time isn't synced yet.
  if (!getLocalTime(&t, 0)) return "--:--";
  char b[12];
  strftime(b, sizeof b, g_clock24 ? "%H:%M" : "%I:%M", &t);
  return String(b);
}

String fmtDate() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return "";
  char b[22];
  strftime(b, sizeof b, "%a %b %d", &t);
  return String(b);
}

// Convert an "HH:MM" (24h) string for display: 12-hour gets a one-letter
// period marker ("07:47" -> "7:47A"), 24-hour stays as-is ("19:47").
// Used for sunrise/sunset.
String fmtHm12(const char* hm) {
  if (!hm || hm[0] == 0 || hm[1] == 0 || hm[3] == 0 || hm[4] == 0) return "--:--";
  int h = atoi(hm);
  int m = atoi(hm + 3);
  char b[8];
  if (g_clock24) {
    snprintf(b, sizeof b, "%d:%02d", h, m);
  } else {
    int h12 = h % 12; if (h12 == 0) h12 = 12;
    snprintf(b, sizeof b, "%d:%02d%c", h12, m, (h < 12) ? 'A' : 'P');
  }
  return String(b);
}

// current day/night based on local time vs sunrise/sunset
bool isDayNow() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return true;
  int now = t.tm_hour * 60 + t.tm_min;
  int sr = atoi(g_sunrise) * 60 + atoi(g_sunrise + 3);
  int ss = atoi(g_sunset)  * 60 + atoi(g_sunset + 3);
  return now >= sr && now < ss;
}

// weekday name for today + offset
const char* wdName(int offset) {
  struct tm t;
  if (!getLocalTime(&t, 0)) {
    static const char* nms[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    return nms[offset % 7];
  }
  int w = (t.tm_wday + offset) % 7;
  static const char* nms[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  return nms[w];
}

// Open-Meteo weather fetch (free, no API key)
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[320];
  snprintf(url, sizeof url,
    "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code"
    "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code,sunrise,sunset"
    "&forecast_days=7&temperature_unit=fahrenheit&timezone=auto",
    g_lat, g_lon);

  // open-meteo is Let's Encrypt (ISRG root), same verified bundle as OpenSky.
  NetworkClientSecure sec;
  HTTPClient http;
  http.setTimeout(5000);
  int code = httpsRequestRetry(http, sec, url, HTTPS_METHOD_GET, "", nullptr, false);
  if (code != HTTP_CODE_OK) {
    g_weatherDataFailed = true;
    http.end();
    return;
  }
  BoundedAllocator weatherAlloc(8192);
  JsonDocument doc(&weatherAlloc);
  HttpBodyStream body(http);
  DeserializationError parseErr = deserializeJson(doc, body);
  bool bodyComplete = body.complete() || body.drain();
  http.end();
  if (parseErr || !bodyComplete) {
    g_weatherDataFailed = true;
    return;
  }

  JsonObject cur = doc["current"];
  g_temp = cur["temperature_2m"] | 0.0f;
  g_humidity = cur["relative_humidity_2m"] | 0.0f;
  g_feels = cur["apparent_temperature"] | g_temp;   // falls back to actual temp
  g_wcode_cur = cur["weather_code"] | g_wcode_cur;  // keep last known on absence

  JsonObject daily = doc["daily"];
  JsonArray hmax = daily["temperature_2m_max"];
  JsonArray hmin = daily["temperature_2m_min"];
  JsonArray rain = daily["precipitation_probability_max"];
  JsonArray wc   = daily["weather_code"];
  JsonArray sr   = daily["sunrise"];
  JsonArray ss   = daily["sunset"];
  for (int i = 0; i < 7; i++) {
    g_high[i]   = hmax[i] | 0.0f;
    g_low[i]    = hmin[i] | 0.0f;
    g_rain[i]   = rain[i] | 0;
    g_wcode[i]  = wc[i] | 0;
  }
  // ISO "2026-09-01T06:42" -> copy "06:42"
  if (sr && !sr[0].isNull()) {
    const char* s = sr[0];
    if (strlen(s) >= 16) { strncpy(g_sunrise, s + 11, 5); g_sunrise[5] = 0; }
  }
  if (ss && !ss[0].isNull()) {
    const char* s = ss[0];
    if (strlen(s) >= 16) { strncpy(g_sunset, s + 11, 5); g_sunset[5] = 0; }
  }
  g_weatherValid = true;
  g_weatherDataFailed = false;
  g_lastWeather = millis();

  // record in the RAM ring buffer and (if available) persistent flash log
  weatherLog(g_temp, (unsigned long)time(nullptr));
}

// Simple 24x24 weather icon; day=true for sun, false for moon.
void drawWeatherIcon(int x, int y, int code, bool day) {
  bool rain  = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
  bool snow  = (code >= 71 && code <= 77) || (code >= 85 && code <= 86);
  bool storm = (code >= 95);
  bool cloud = (code >= 1);

  // sun or moon background
  if (day) {
    tft.fillCircle(x + 12, y + 12, 5, TFT_YELLOW);
    for (int a = 0; a < 8; a++) {
      float ang = a * PI / 4.0f;
      int x1 = x + 12 + (int)(6 * cos(ang));
      int y1 = y + 12 + (int)(6 * sin(ang));
      int x2 = x + 12 + (int)(10 * cos(ang));
      int y2 = y + 12 + (int)(10 * sin(ang));
      tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    }
  } else {
    tft.fillCircle(x + 12, y + 12, 5, TFT_LIGHTGREY);
    tft.fillCircle(x + 15, y + 12, 5, TFT_BLACK); // crescent cutout
  }

  // cloud overlay
  if (cloud) {
    tft.fillCircle(x + 8,  y + 16, 4, TFT_DARKGREY);
    tft.fillCircle(x + 14, y + 13, 5, TFT_DARKGREY);
    tft.fillCircle(x + 18, y + 16, 4, TFT_DARKGREY);
  }

  if (rain) {
    tft.drawLine(x + 8,  y + 21, x + 7,  y + 23, TFT_CYAN);
    tft.drawLine(x + 14, y + 21, x + 13, y + 23, TFT_CYAN);
    tft.drawLine(x + 20, y + 21, x + 19, y + 23, TFT_CYAN);
  } else if (snow) {
    tft.fillCircle(x + 8,  y + 22, 1, TFT_WHITE);
    tft.fillCircle(x + 14, y + 22, 1, TFT_WHITE);
    tft.fillCircle(x + 20, y + 22, 1, TFT_WHITE);
  } else if (storm) {
    tft.drawLine(x + 12, y + 20, x + 8, y + 24, TFT_YELLOW);
  }
}

// Idle screen shown when no plane is overhead.
// Draws below the header (y >= 30), leaving the right 20 px for the countdown bar.
void drawIdle() {
  bool day = isDayNow();

  // current conditions (left column). Content starts below the taller header.
  const int top = 42;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(4);
  tft.setCursor(8, top);
  tft.printf("%d%c", (int)round(tempDisp(g_temp)), tempUnit());
  tft.setTextSize(1);
  // Current conditions icon, to the right of the big temperature (not below
  // the sunset). Refreshed each weather update.
  drawWeatherIcon(tft.getCursorX() + 6, top + 4, g_wcode_cur, isDayNow());
  // "Feels like" temperature (apparent_temperature), compact label "FL".
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(8, top + 30);
  tft.printf("FL %d%c", (int)round(tempDisp(g_feels)), tempUnit());
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(8, top + 50);
  tft.printf("%d%% hum", (int)g_humidity);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(8, top + 72);  tft.print("Sunr "); tft.print(fmtHm12(g_sunrise));
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(8, top + 90); tft.print("Suns "); tft.print(fmtHm12(g_sunset));

  // Pool temperature (from Govee; only shown when the feature is enabled).
  // Tapping this region opens the pool temp history graph. Same layout as Sunr/Suns:
  // label and value on one line, below the sunset row.
#if POOL_FEATURE
  if (g_poolEnabled) {
    tft.setTextFont(2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(8, top + 108); tft.print("Pool ");
    tft.setTextColor(g_poolValid ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
    if (g_poolValid) tft.printf("%.1f%c", poolDisp(g_poolTemp), tempUnit());
    else tft.print("--");
  }
#endif

  // 7-day forecast (right/bottom); columns spread into any extra panel
  // width but stay clear of the countdown-bar strip on the right edge.
  const int x0 = 110, y0 = top + 2, h = 92;
  const int w = (DISP_W - x0 - 16) / 4;   // 48 on the 2.8", 58 on the 4"
  for (int i = 0; i < 7; i++) {
    int col = i % 4;
    int row = i / 4;
    int x = x0 + col * w;
    int y = y0 + row * h;

    // day name (FONT_AUX: F2 on the 4" where 8px reads small, F1 on the 2.8")
    tft.setTextFont(FONT_AUX);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(x, y);
    tft.print(wdName(i));

    // icon
    drawWeatherIcon(x + 4, y + 16, g_wcode[i], i == 0 ? day : true);

    // high/low
    tft.setTextFont(FONT_AUX);
    tft.setTextColor(TFT_PINK, TFT_BLACK);
    tft.setCursor(x, y + 44);
    tft.printf("%d", (int)round(tempDisp(g_high[i])));
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(x + 28, y + 44);
    tft.printf("%d", (int)round(tempDisp(g_low[i])));

    // rain probability, centered under the temps block (the widest element -
    // the cell is wider than its left-anchored content on the 4" layout, so
    // centering on w/2 reads as right-of-center)
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    char rbuf[8]; snprintf(rbuf, sizeof rbuf, "%d%%", g_rain[i]);
    char lobuf[8]; snprintf(lobuf, sizeof lobuf, "%d", (int)round(tempDisp(g_low[i])));
    int blockC = x + (28 + tft.textWidth(lobuf, FONT_AUX)) / 2;
    tft.drawCentreString(rbuf, blockC, y + 62, FONT_AUX);
  }

  // status line: a pending alarm snooze counts down here instead of the
  // aircraft count (flight polls are suppressed during a snooze, so the
  // count would be stale)
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(FONT_AUX);
  tft.setCursor(8, 224);
  char sb[40];
  if (snoozeStatusText(sb, sizeof sb)) tft.print(sb);
  else tft.print(lastErr);
}

// ---- Weather temp history graph ----
// Mirrors the pool temp history graph (pool.ino), but plots the Open-Meteo
// current temperature that we log to flash every ~10 min via weatherLog().
// Opened by tapping the weather temperature on the idle screen.

// Window (seconds) for the current weather graph timeframe
unsigned long wxWindowSec() {
  switch (g_wxTF) {
    case WX_DAY:   return 86400UL;
    case WX_MONTH: return 2592000UL;
    case WX_YEAR:  return 31536000UL;
    default:       return 604800UL;  // WX_WEEK
  }
}

// Day/Week plot from the raw (10-min) tier; Month from hourly rollups; Year
// from daily rollups.
void wxSeriesForTF(unsigned long** times, float** temps, int* count) {
  switch (g_wxTF) {
    case WX_MONTH: *times = g_wxHourTime; *temps = g_wxHourTemp; *count = g_wxHourCount; break;
    case WX_YEAR:  *times = g_wxDayTime;  *temps = g_wxDayTemp;  *count = g_wxDayCount;  break;
    default:       *times = g_wxLogTime;  *temps = g_wxLogTemp;  *count = g_wxLogCount;  break;
  }
}

// Weather series wrapper; the generic plotter is in pool.ino.
bool plotWeatherSeries(unsigned long* times, float* temps, int count,
                       unsigned long t0, unsigned long nowSec, unsigned long win,
                       int gx, int gy, int gw, int gh,
                       float& dataMin, float& dataMax) {
  return plotSeries(times, temps, count, t0, nowSec, win, gx, gy, gw, gh,
                    dataMin, dataMax, tempDisp);
}

// Weather temp history graph. Plots the samples we have logged for the
// selected timeframe. Auto-dismisses after 30s; any touch keeps it alive.
void drawWxGraph() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, DISP_W, 28, g_clockCol);
  tft.setTextColor(btnFg(g_clockCol), g_clockCol);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("History > Weather Temperature");
  backBtn("Back");

  // timeframe selector
  const char* labels[4] = {"Day", "Week", "Month", "Year"};
  int bx = 8;
  for (int i = 0; i < 4; i++) {
    bool sel = i == g_wxTF;
    uint16_t col = sel ? btnCol() : disabledCol();
    if (sel) themeBtn(bx, 34, 70, 22, 5);
    else tft.fillRoundRect(bx, 34, 70, 22, 5, col);
    tft.setTextColor(btnFg(col), col);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(labels[i], bx + 35, 45, FONT_AUX);
    tft.setTextDatum(TL_DATUM);
    bx += 76 + (DISP_W - 320) / 4;
  }

  // graph area
  int gx = 10, gy = 66, gw = DISP_W - 20, gh = 140;
  uint16_t gbg = graphBgCol();
  tft.fillRect(gx, gy, gw, gh, gbg);
  tft.drawRect(gx - 1, gy - 1, gw + 2, gh + 2, btnFg(gbg));

  unsigned long nowSec = (unsigned long)time(nullptr);

  // Time not synced yet (right after a boot/deep-sleep wake). Show an explicit
  // "waiting" message instead of "No data" (see drawPoolGraph()).
  if (nowSec < 1600000000UL) {
    tft.setTextColor(btnFg(gbg), gbg);
    tft.setTextFont(1);
    tft.setCursor(gx + 20, gy + gh / 2);
    tft.print("Waiting for time sync...");
    return;
  }

  unsigned long win = wxWindowSec();
  unsigned long t0 = (nowSec > win) ? (nowSec - win) : 0;

  unsigned long* times; float* temps; int count;
  wxSeriesForTF(&times, &temps, &count);
  float lo, hi;
  bool plotted = plotWeatherSeries(times, temps, count, t0, nowSec, win, gx, gy, gw, gh, lo, hi);

  if (!plotted) {
    tft.setTextColor(btnFg(gbg), gbg);
    tft.setTextFont(1);
    tft.setCursor(gx + 20, gy + gh / 2);
    tft.print("No data in this period yet");
  } else {
    // Bottom strip below the chart: actual data low (left), current weather
    // temp (center), actual data high (right) for the timeframe shown. Sits on
    // the black screen, not on the graph fill.
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(gx, gy + gh + 6);
    tft.printf("Lo %.1f", lo);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(gx + 110, gy + gh + 6);
    tft.printf("now %.1f%c", tempDisp(g_temp), tempUnit());
    char hbuf[16];
    snprintf(hbuf, sizeof hbuf, "Hi %.1f", hi);
    tft.drawRightString(hbuf, gx + gw, gy + gh + 6, 2);
  }
}

void handleWxGraphTouch(uint16_t x, uint16_t y) {
  // any touch keeps the screen alive for another 2 minutes
  g_screenIdleUntil = millis() + SCREEN_IDLE_TIMEOUT_MS;

  if (inRect(x, y, RX(265), 4, RX(315), 24)) { g_screen = SCR_DASH; dirty = true; return; }  // close
  int bx = 8;
  for (int i = 0; i < 4; i++) {
    if (inRect(x, y, bx, 34, bx + 70, 56)) {
      if (g_wxTF != i) { g_wxTF = i; dirty = true; }
      return;
    }
    bx += 76 + (DISP_W - 320) / 4;
  }
  dirty = true;
}
