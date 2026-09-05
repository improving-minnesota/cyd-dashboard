// pool.ino - Pool Temp settings screen, history graph, and Govee Open API
// integration. Requires a free Govee API key from developer.govee.com;
// thermometers are detected by their "sensorTemperature" capability and
// selected via the Pool Temp settings screen. Falls back to "--" whenever a
// reading is unavailable (feature disabled, no key, no device, or a failed
// fetch). Globals (g_goveeKey, g_poolValid, the pool temp history ring buffers,
// etc.) are declared in cyd-dashboard.ino so they're visible everywhere.

// ---- Govee Open API: Pool Temp integration ----

// Select the currently-highlighted thermometer and persist it.
void selectGoveeDevice() {
  if (g_goveeCount == 0) { g_poolValid = false; return; }
  GoveeDev &g = g_goveeDevs[g_goveeSel];
  if (g_poolDeviceId != g.id) {   // device actually changed
    g_poolValid = false;          // don't show the previous device's temp
    g_poolTemp = 0;
  }
  g_poolDeviceId = g.id;
  g_poolModel = g.model;
  g_poolName = g.name;
  prefs.begin("flight", false);
  prefs.putString("poolid", g_poolDeviceId);
  prefs.putString("poolmodel", g_poolModel);
  prefs.putString("poolname", g_poolName);
  prefs.end();
  netWantPool = true;   // fetch the newly-selected device's temp async
}

// Fetch the list of thermometers on the account into g_goveeDevs.
// Response shape: data is an array of {sku, device, deviceName, type, capabilities}.
// Thermometers have type "devices.types.thermometer" / a "sensorTemperature" capability.
bool fetchGoveeDevices() {
  if (WiFi.status() != WL_CONNECTED || g_goveeKey.length() == 0) return false;
  // Govee's Open API is Amazon-signed, verified against Amazon Root CA 1.
  NetworkClientSecure sec;
  HTTPClient http;
  http.addHeader("Govee-API-Key", g_goveeKey);
  http.setTimeout(5000);
  int code = httpsRequestRetry(http, sec, "https://openapi.api.govee.com/router/api/v1/user/devices",
                               kAmazonRootCA1, HTTP_METHOD_GET, "");
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  g_goveeCount = 0;
  JsonArray devs = doc["data"];   // array of devices
  for (JsonObject d : devs) {
    if (g_goveeCount >= MAX_GOVEE) break;
    bool isThermo = (String(d["type"] | "").indexOf("thermometer") >= 0);
    JsonArray caps = d["capabilities"];
    for (JsonObject c : caps) {
      if (String(c["instance"] | "") == "sensorTemperature") isThermo = true;
    }
    if (!isThermo) continue;
    GoveeDev &g = g_goveeDevs[g_goveeCount];
    String id = d["device"] | "", sku = d["sku"] | "", name = d["deviceName"] | "";
    strncpy(g.id, id.c_str(), 63); g.id[63] = 0;
    strncpy(g.model, sku.c_str(), 15); g.model[15] = 0;
    strncpy(g.name, name.c_str(), 39); g.name[39] = 0;
    g_goveeCount++;
  }
  g_goveeSel = 0;
  selectGoveeDevice();
  return g_goveeCount > 0;
}

// Fetch the current temperature of the selected device.
// POST /device/state with body {requestId, payload:{sku, device}}; the
// temperature is in payload.capabilities[] where instance=="sensorTemperature".
bool fetchGoveeTemp() {
  if (WiFi.status() != WL_CONNECTED || g_goveeKey.length() == 0 || g_poolDeviceId.length() == 0) {
    g_poolValid = false;
    return false;
  }
  String body = "{\"requestId\":\"pool-1\",\"payload\":{\"sku\":\"" + g_poolModel
                + "\",\"device\":\"" + g_poolDeviceId + "\"}}";
  // Verified TLS against Amazon Root CA 1 (see kAmazonRootCA1).
  NetworkClientSecure sec;
  HTTPClient http;
  http.addHeader("Govee-API-Key", g_goveeKey);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int code = httpsRequestRetry(http, sec, "https://openapi.api.govee.com/router/api/v1/device/state",
                               kAmazonRootCA1, HTTP_METHOD_POST, body);
  if (code != HTTP_CODE_OK) { g_poolValid = false; http.end(); return false; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) { g_poolValid = false; return false; }
  bool found = false;
  JsonArray caps = doc["payload"]["capabilities"];
  for (JsonObject c : caps) {
    if (String(c["instance"] | "") == "sensorTemperature") {
      g_poolTemp = c["state"]["value"] | 0.0f;
      found = true;
      break;
    }
  }
  if (!found) { g_poolValid = false; return false; }
  g_poolValid = true;
  g_lastPool = millis();

  // record in the RAM ring buffer and (if available) persistent flash log
  poolLog(g_poolTemp, (unsigned long)time(nullptr));

  return true;
}

// ---- Pool Temp settings screen ----
void drawPool() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Pool Temp");

  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // Enabled toggle
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 40);
  tft.print("Enabled");
  tft.setTextColor(g_poolEnabled ? TFT_GREENYELLOW : TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(150, 40);
  tft.print(g_poolEnabled ? "ON" : "OFF");
  tft.fillRoundRect(230, 36, 82, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(250, 43);
  tft.print("Toggle");

  // Govee API key
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 76);
  tft.print("Govee API Key");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 94);
  tft.print(g_goveeKey.length() ? g_goveeKey.substring(0, 34) : "(none)");
  tft.fillRoundRect(250, 72, 62, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(256, 79);
  tft.print("Edit");

  // Fetch devices
  tft.fillRoundRect(10, 116, 300, 28, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(20, 123);
  tft.print("Fetch Devices");

  // selected device + prev/next
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 160);
  tft.print("Device");
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(80, 160);
  if (g_goveeCount > 0) tft.print(g_goveeDevs[g_goveeSel].name);
  else if (g_poolName.length() > 0) tft.print(g_poolName);
  else tft.print("none");
  if (g_goveeCount > 1) {
    tft.fillRoundRect(250, 156, 28, 24, 5, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY); tft.setTextFont(2);
    tft.setCursor(258, 163); tft.print("<");
    tft.fillRoundRect(284, 156, 28, 24, 5, TFT_DARKGREY);
    tft.setCursor(292, 163); tft.print(">");
  }

  // current temperature
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 200);
  tft.print("Current");
  tft.setTextColor(g_poolValid ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(4);   // font 3 is unused in TFT_eSPI and renders nothing
  tft.setCursor(90, 196);
  if (g_poolValid) tft.printf("%.1f%c", g_poolTemp, g_poolUnit);
  else tft.print("--");

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 232);
  tft.print("Key: developer.govee.com. Show -- if unavailable.");
}

void handlePoolTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_SETTINGS; dirty = true; return; }
  if (inRect(x, y, 230, 36, 312, 60)) {  // Enabled toggle
    g_poolEnabled = !g_poolEnabled;
    prefs.begin("flight", false); prefs.putBool("poolen", g_poolEnabled); prefs.end();
    dirty = true;
    return;
  }
  if (inRect(x, y, 250, 72, 312, 96)) { g_screen = SCR_WIFI; g_wifiSub = 9; dirty = true; return; }  // edit key
  if (inRect(x, y, 10, 116, 310, 144)) { netWantPoolDevices = true; dirty = true; return; }  // fetch devices
  if (g_goveeCount > 1) {
    if (inRect(x, y, 250, 156, 278, 180)) { g_goveeSel = (g_goveeSel - 1 + g_goveeCount) % g_goveeCount; selectGoveeDevice(); dirty = true; return; }
    if (inRect(x, y, 284, 156, 312, 180)) { g_goveeSel = (g_goveeSel + 1) % g_goveeCount; selectGoveeDevice(); dirty = true; return; }
  }
}

// ---- Pool Temp history graph ----

// Window (seconds) for the current pool graph timeframe
unsigned long poolWindowSec() {
  switch (g_poolTF) {
    case TF_DAY:   return 86400UL;
    case TF_MONTH: return 2592000UL;
    case TF_YEAR:  return 31536000UL;
    default:       return 604800UL;  // TF_WEEK
  }
}

// Day/Week plot from the raw (5-min) tier; Month from hourly rollups; Year
// from daily rollups. This keeps each view populated well beyond what a
// single fixed-size raw buffer could hold.
void poolSeriesForTF(unsigned long** times, float** temps, int* count) {
  switch (g_poolTF) {
    case TF_MONTH: *times = g_poolHourTime; *temps = g_poolHourTemp; *count = g_poolHourCount; break;
    case TF_YEAR:  *times = g_poolDayTime;  *temps = g_poolDayTemp;  *count = g_poolDayCount;  break;
    default:       *times = g_poolLogTime;  *temps = g_poolLogTemp;  *count = g_poolLogCount;  break;
  }
}

// Plot one series into the given graph rect. Returns true if any point fell
// inside the [t0, nowSec] window. Outputs the actual data low/high for the
// window via dataMin/dataMax (before any y-axis padding). Also draws the padded
// y-axis scale labels (max/min in the chart corners) and a dotted average line
// with an "avg" label.
bool plotSeries(unsigned long* times, float* temps, int count,
                unsigned long t0, unsigned long nowSec, unsigned long win,
                int gx, int gy, int gw, int gh,
                float& dataMin, float& dataMax, uint16_t lineColor) {
  float vmin = 1e9f, vmax = -1e9f;
  float sum = 0.0f;
  int cnt = 0;
  for (int i = 0; i < count; i++) {
    unsigned long t = times[i];
    if (t < t0 || t > nowSec) continue;
    float v = temps[i];
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
    sum += v;
    cnt++;
  }
  if (cnt == 0) return false;

  dataMin = vmin;   // actual data low for this timeframe
  dataMax = vmax;   // actual data high for this timeframe
  float avg = sum / cnt;   // average temp for this timeframe

  if (vmax - vmin < 1.0f) { vmin -= 1.0f; vmax += 1.0f; }   // pad only for y-scale
  int prevX = -1, prevY = -1;
  for (int i = 0; i < count; i++) {
    unsigned long t = times[i];
    if (t < t0 || t > nowSec) continue;
    float v = temps[i];
    int px = gx + (int)((double)(t - t0) * gw / (double)win);
    int py = gy + gh - (int)((v - vmin) * gh / (vmax - vmin));
    px = constrain(px, gx, gx + gw);
    py = constrain(py, gy, gy + gh);
    if (prevX >= 0) tft.drawLine(prevX, prevY, px, py, lineColor);
    prevX = px; prevY = py;
  }

  // Dotted horizontal line at the average temperature, with an "avg" label just
  // above it, near the left edge of the graph.
  int avgY = gy + gh - (int)((avg - vmin) * gh / (vmax - vmin));
  avgY = constrain(avgY, gy, gy + gh);
  for (int x = gx; x <= gx + gw; x += 6) {
    int w = (x + 3 <= gx + gw) ? 3 : (gx + gw - x);
    if (w > 0) tft.drawFastHLine(x, avgY, w, TFT_ORANGE);
  }
  char abuf[16];
  snprintf(abuf, sizeof abuf, "avg %.1f", avg);
  tft.setTextColor(TFT_ORANGE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(gx + 2, avgY - 8);
  tft.print(abuf);

  // Labels for the chart's padded y-axis scale (padded max top-right, padded
  // min bottom-right), so the vertical range of the graph is readable. Drawn
  // inside the chart corners - there is no room above the graph (timeframe
  // buttons sit right above it).
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  char sbuf[16];
  snprintf(sbuf, sizeof sbuf, "%.0f", vmax);
  tft.drawRightString(sbuf, gx + gw, gy + 2, 1);        // padded max
  snprintf(sbuf, sizeof sbuf, "%.0f", vmin);
  tft.drawRightString(sbuf, gx + gw, gy + gh - 9, 1);   // padded min
  return true;
}

// Pool temp series wrapper (green line). Weather uses plotWeatherSeries below.
bool plotPoolSeries(unsigned long* times, float* temps, int count,
                    unsigned long t0, unsigned long nowSec, unsigned long win,
                    int gx, int gy, int gw, int gh,
                    float& dataMin, float& dataMax) {
  return plotSeries(times, temps, count, t0, nowSec, win, gx, gy, gw, gh,
                    dataMin, dataMax, TFT_GREENYELLOW);
}

// Pool temp history graph. Plots the samples we have logged for the selected
// timeframe. Auto-dismisses after 30s; any touch keeps it alive.
void drawPoolGraph() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Pool Temp History");
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("X");

  // timeframe selector
  const char* labels[4] = {"Day", "Week", "Month", "Year"};
  int bx = 8;
  for (int i = 0; i < 4; i++) {
    uint16_t col = (i == g_poolTF) ? TFT_DARKGREEN : TFT_DARKGREY;
    tft.fillRoundRect(bx, 34, 70, 22, 5, col);
    tft.setTextColor(TFT_WHITE, col);
    tft.setTextFont(1);
    tft.setCursor(bx + 18, 40);
    tft.print(labels[i]);
    bx += 76;
  }

  // graph area
  int gx = 10, gy = 66, gw = 300, gh = 140;
  tft.fillRect(gx, gy, gw, gh, TFT_NAVY);
  tft.drawRect(gx - 1, gy - 1, gw + 2, gh + 2, TFT_WHITE);

  unsigned long nowSec = (unsigned long)time(nullptr);

  // Time not synced yet (right after a boot/deep-sleep wake). The window filter
  // would compare every real sample against nowSec==0 and wrongly reject them
  // all, so show an explicit "waiting" message instead of "No data".
  if (nowSec < 1600000000UL) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(gx + 20, gy + gh / 2);
    tft.print("Waiting for time sync...");
    return;
  }

  unsigned long win = poolWindowSec();
  unsigned long t0 = (nowSec > win) ? (nowSec - win) : 0;

  unsigned long* times; float* temps; int count;
  poolSeriesForTF(&times, &temps, &count);
  float lo, hi;
  bool plotted = plotPoolSeries(times, temps, count, t0, nowSec, win, gx, gy, gw, gh, lo, hi);

  if (!plotted) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    tft.setTextFont(1);
    tft.setCursor(gx + 20, gy + gh / 2);
    tft.print("No data in this period yet");
  } else {
    // Bottom strip below the chart: actual data low (left), current temp
    // (center), actual data high (right) for the timeframe shown.
    tft.setTextColor(TFT_CYAN, TFT_NAVY);
    tft.setTextFont(2);
    tft.setCursor(gx, gy + gh + 6);
    tft.printf("Lo %.1f", lo);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setCursor(gx + 110, gy + gh + 6);
    tft.printf("now %.1f%c", g_poolTemp, g_poolUnit);
    char hbuf[16];
    snprintf(hbuf, sizeof hbuf, "Hi %.1f", hi);
    tft.drawRightString(hbuf, gx + gw, gy + gh + 6, 2);
  }
}

void handlePoolGraphTouch(uint16_t x, uint16_t y) {
  // any touch keeps the graph alive for another 30s
  g_graphUntil = millis() + 30000UL;

  if (inRect(x, y, 265, 4, 315, 24)) { g_screen = SCR_DASH; dirty = true; return; }  // close
  int bx = 8;
  for (int i = 0; i < 4; i++) {
    if (inRect(x, y, bx, 34, bx + 70, 56)) {
      if (g_poolTF != i) { g_poolTF = i; dirty = true; }
      return;
    }
    bx += 76;
  }
  dirty = true;
}
