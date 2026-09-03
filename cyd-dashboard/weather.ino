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
}

String fmtClock() {
  struct tm t;
  // Explicit 0ms timeout: this is called every ~1s from the main loop, and
  // getLocalTime()'s default 5s timeout would block the whole loop (touch +
  // drawing) for that long on every call while time isn't synced yet.
  if (!getLocalTime(&t, 0)) return "--:--";
  char b[12];
  strftime(b, sizeof b, "%I:%M", &t);
  return String(b);
}

String fmtDate() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return "";
  char b[22];
  strftime(b, sizeof b, "%a %b %d", &t);
  return String(b);
}

// Convert an "HH:MM" (24h) string to 12-hour with a one-letter period marker,
// e.g. "07:47" -> "7:47A", "19:47" -> "7:47P". Used for sunrise/sunset.
String fmtHm12(const char* hm) {
  if (!hm || hm[0] == 0 || hm[1] == 0 || hm[3] == 0 || hm[4] == 0) return "--:--";
  int h = atoi(hm);
  int m = atoi(hm + 3);
  char b[8];
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  snprintf(b, sizeof b, "%d:%02d%c", h12, m, (h < 12) ? 'A' : 'P');
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

  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

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
  g_lastWeather = millis();
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
  tft.printf("%dF", (int)round(g_temp));
  // Current conditions icon, to the right of the big temperature (not below
  // the sunset). Refreshed each weather update.
  drawWeatherIcon(tft.getCursorX() + 6, top + 4, g_wcode_cur, isDayNow());
  // "Feels like" temperature (apparent_temperature), compact label "FL".
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(8, top + 30);
  tft.printf("FL %dF", (int)round(g_feels));
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
    if (g_poolValid) tft.printf("%.1f%c", g_poolTemp, g_poolUnit);
    else tft.print("--");
  }
#endif

  // 7-day forecast (right/bottom)
  const int x0 = 110, y0 = top + 2, w = 46, h = 92;
  for (int i = 0; i < 7; i++) {
    int col = i % 4;
    int row = i / 4;
    int x = x0 + col * w;
    int y = y0 + row * h;

    // day name
    tft.setTextFont(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(x, y);
    tft.print(wdName(i));

    // icon
    drawWeatherIcon(x + 4, y + 10, g_wcode[i], i == 0 ? day : true);

    // high/low
    tft.setTextFont(1);
    tft.setTextColor(TFT_PINK, TFT_BLACK);
    tft.setCursor(x, y + 40);
    tft.printf("%d", (int)round(g_high[i]));
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(x + 22, y + 40);
    tft.printf("%d", (int)round(g_low[i]));

    // rain probability
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setCursor(x, y + 52);
    tft.printf("%d%%", g_rain[i]);
  }

  // status line
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 224);
  tft.print(lastErr);
}
