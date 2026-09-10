// wifi_config.ino - on-device network provisioning via the touchscreen.
//
// Part of the cyd-dashboard sketch. Provides these sub-screens:
//   0 = network list (scan results) + "enter manually" + "IP setup"
//   1 = keyboard to type an SSID
//   2 = keyboard to type a password, then save & connect
//   20 = IP Settings page (DHCP/Static + address fields)
//   21..25 = keyboards for IP address, subnet mask, gateway, DNS, hostname
// Credentials are stored in NVS so the device reconnects after a reboot.

int  g_wifiSub = 0;         // 0=list, 1=ssid keyboard, 2=pass keyboard
bool g_kbShift = false;     // uppercase letters on the on-screen keyboard
bool g_kbSym   = false;     // symbol/number pad mode
int  g_kbCursor = 0;        // edit cursor index (0..length) in the active field
int  g_lastKbSub = -1;      // last field drawn, so we can reset the cursor on change
bool g_kbShow = false;      // password fields: true shows the value in cleartext
bool kbFieldPw() { return (g_wifiSub == 2 || g_wifiSub == 4); }   // password-type fields
String g_networks[20];      // deduplicated SSIDs
String g_netChan[20];       // comma-joined channel list per SSID (e.g. "1,6")
int   g_netAps[20];         // how many access points share that SSID
int  g_netCount = 0;
int  g_netScroll = 0;
String g_ssid = "";
String g_pass = "";
// g_latLonStr is defined in cyd-dashboard.ino (concatenation order)

// Scan for nearby networks and switch to the list screen.
void enterWifiScreen() {
  g_wifiSub = 0;
  g_ssid = "";
  g_pass = "";
  g_netScroll = 0;
  scanWifi();
  g_screen = SCR_WIFI;
  dirty = true;
}

void scanWifi() {
  WiFi.scanDelete();
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  g_netCount = 0;
  // Deduplicate by SSID, recording how many access points share it and the
  // 2.4GHz channels they use. The ESP32 only sees 2.4GHz, so duplicate SSIDs
  // are multiple APs of the same network, not different bands.
  for (int i = 0; i < n && g_netCount < 20; i++) {
    String ssid = WiFi.SSID(i);
    int ch = WiFi.channel(i);
    int found = -1;
    for (int k = 0; k < g_netCount; k++) {
      if (g_networks[k] == ssid) { found = k; break; }
    }
    if (found >= 0) {
      // add this channel to the existing SSID's channel list (if new)
      String chanStr = String(ch);
      if (String(g_netChan[found]).indexOf(chanStr) < 0) {
        if (String(g_netChan[found]).length() > 0) g_netChan[found] += ",";
        g_netChan[found] += chanStr;
      }
      g_netAps[found]++;
    } else {
      g_networks[g_netCount] = ssid;
      g_netChan[g_netCount] = String(ch);
      g_netAps[g_netCount] = 1;
      g_netCount++;
    }
  }
}

void drawWifiScreen() {
  if (g_wifiSub == 0) drawWifiList();
  else if (g_wifiSub == 1) drawKeyboard("Enter SSID", g_ssid, false);
  else if (g_wifiSub == 2) drawKeyboard("Password for " + g_ssid, g_pass, true);
  else if (g_wifiSub == 3) drawKeyboard("OpenSky client id", g_osClientId, false);
  else if (g_wifiSub == 4) drawKeyboard("OpenSky client secret", g_osClientSecret, true);
  else if (g_wifiSub == 5) drawKeyboard("Search address", g_addrSearch, false);
  else if (g_wifiSub == 6) drawKeyboard("Sleep start (HHMM)", g_sleepStartStr, false);
  else if (g_wifiSub == 7) drawKeyboard("Sleep end (HHMM)", g_sleepEndStr, false);
  else if (g_wifiSub == 8) drawKeyboard("Wake duration (min)", g_wakeStr, false);
  else if (g_wifiSub == 10) drawKeyboard("Lat,Lon", g_latLonStr, false);
  else if (g_wifiSub == 11) drawKeyboard("Home airport", g_homeAirport, false);
  else if (g_wifiSub == 12) drawAddrStatus();
  else if (g_wifiSub == 20) drawIpConfig();
  else if (g_wifiSub == 21) drawKeyboard("Static IP address", g_staticIp, false);
  else if (g_wifiSub == 22) drawKeyboard("Subnet mask", g_staticMask, false);
  else if (g_wifiSub == 23) drawKeyboard("Gateway", g_staticGw, false);
  else if (g_wifiSub == 24) drawKeyboard("DNS server", g_staticDns, false);
  else if (g_wifiSub == 25) drawKeyboard("Hostname", g_hostname, false);
  else drawKeyboard("Govee API key", g_goveeKey, true);
}

void handleWifiTouch(uint16_t x, uint16_t y) {
  if (g_wifiSub == 0) handleWifiListTouch(x, y);
  else if (g_wifiSub == 12) handleAddrStatusTouch(x, y);
  else if (g_wifiSub == 20) handleIpConfigTouch(x, y);
  else handleKeyboardTouch(x, y);
}

// ---- network list ----
void drawWifiList() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("Network");
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(8, 32);
  tft.printf("%d networks", g_netCount);
  tft.setCursor(200, 32);
  tft.print(g_ipDhcp ? "IP: DHCP" : "IP: static");

  if (g_netCount == 0) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, 52);
    tft.print("No networks found");
  }

  int visible = (g_netCount - g_netScroll > 6) ? 6 : (g_netCount - g_netScroll);
  for (int r = 0; r < visible; r++) {
    int y = 40 + r * 26;
    int idx = g_netScroll + r;
    String ssid = g_networks[idx];
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(8, y + 2);
    tft.print(ssid.substring(0, 18));
    // sub-line: 2.4GHz + channels (+ AP count if more than one)
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(8, y + 16);
    if (g_netAps[idx] > 1) {
      tft.printf("2.4GHz  %d APs  ch %s", g_netAps[idx], g_netChan[idx].c_str());
    } else {
      tft.printf("2.4GHz  ch %s", g_netChan[idx].c_str());
    }
  }

  // scroll indicators
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(308, 46);
  tft.print("^");
  tft.setCursor(308, 212);
  tft.print("v");

  // bottom buttons: rescan | type SSID | DHCP/static addressing
  tft.setTextFont(2);
  tft.fillRoundRect(10, 212, 92, 26, 6, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawCentreString("Scan", 56, 218, 2);
  tft.fillRoundRect(110, 212, 100, 26, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawCentreString("Manual", 160, 218, 2);
  tft.fillRoundRect(218, 212, 94, 26, 6, TFT_NAVY);
  tft.drawCentreString("IP setup", 265, 218, 2);
}

void handleWifiListTouch(uint16_t x, uint16_t y) {
  // During the first-boot wizard, Back ends the flow on the dashboard (a
  // "skip" that still leaves the device usable); otherwise it returns to
  // Settings as before.
  if (inRect(x, y, 265, 4, 315, 24)) {
    if (g_bootStage == BOOT_WIFI) { g_bootStage = BOOT_DONE; g_screen = SCR_DASH; }
    else g_screen = SCR_SETTINGS;
    dirty = true;
    return;
  }
  if (inRect(x, y, 300, 40, 319, 90)) { if (g_netScroll > 0) { g_netScroll--; dirty = true; } return; }
  if (inRect(x, y, 300, 200, 319, 240)) {
    int maxs = (g_netCount - 6 > 0) ? g_netCount - 6 : 0;
    if (g_netScroll < maxs) { g_netScroll++; dirty = true; }
    return;
  }
  if (inRect(x, y, 10, 212, 102, 238)) { scanWifi(); dirty = true; return; }
  if (inRect(x, y, 110, 212, 210, 238)) { g_ssid = ""; g_wifiSub = 1; dirty = true; return; }
  if (inRect(x, y, 218, 212, 312, 238)) { enterIpConfig(); return; }

  // network row
  int row = (y - 40) / 26;
  int idx = g_netScroll + row;
  if (row >= 0 && row < 6 && idx >= 0 && idx < g_netCount) {
    g_ssid = g_networks[idx];
    g_pass = "";
    g_wifiSub = 2;   // straight to password entry
    dirty = true;
  }
}

// ---- IP Settings (DHCP / static addressing + hostname) ----
// The g_static*/g_hostname strings are the live values; edits persist via
// saveNetCfg() on keyboard OK. Values are (re)loaded when the page opens so a
// backed-out edit can't leave unsaved text in the globals the next connect
// would use.

void loadNetCfg() {
  g_ipDhcp    = prefs.getBool("ipdhcp", true);
  g_staticIp   = prefs.getString("ipaddr", "");
  g_staticMask = prefs.getString("ipmask", "");
  g_staticGw   = prefs.getString("ipgw", "");
  g_staticDns  = prefs.getString("ipdns", "");
  g_hostname   = prefs.getString("hostname", "cyd-dashboard");
}

void saveNetCfg() {
  prefs.begin("flight", false);
  prefs.putBool("ipdhcp", g_ipDhcp);
  prefs.putString("ipaddr", g_staticIp);
  prefs.putString("ipmask", g_staticMask);
  prefs.putString("ipgw", g_staticGw);
  prefs.putString("ipdns", g_staticDns);
  prefs.putString("hostname", g_hostname);
  prefs.end();
}

void enterIpConfig() {
  prefs.begin("flight", false);
  loadNetCfg();
  prefs.end();
  g_netCfgDirty = false;
  g_wifiSub = 20;
  dirty = true;
}

// One label + right-aligned value + Edit button row on the IP Settings page.
void drawIpRow(int y, const char* label, const String& value) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, y + 4);
  tft.print(label);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(value.length() ? value : "--", 262, y + 4, 2);
  tft.setTextDatum(TL_DATUM);
  tft.fillRoundRect(270, y, 42, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(280, y + 6);
  tft.print("Edit");
}

void drawIpConfig() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print("IP Settings");
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // DHCP / Static mode toggle
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(8, 44);
  tft.print("Addressing");
  tft.setTextColor(g_ipDhcp ? TFT_LIGHTGREY : TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor(150, 44);
  tft.print(g_ipDhcp ? "DHCP" : "Static");
  tft.fillRoundRect(230, 40, 82, 24, 5, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(1);
  tft.setCursor(250, 47);
  tft.print("Toggle");

  if (g_ipDhcp) {
    // Hostname still applies under DHCP (it's sent in the DHCP request).
    drawIpRow(76, "Hostname", g_hostname);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(8, 116);
    tft.print("DHCP assigns IP, mask, gateway, DNS.");
    tft.setCursor(8, 130);
    if (WiFi.status() == WL_CONNECTED) {
      tft.print("Current IP: ");
      tft.print(WiFi.localIP().toString());
    } else {
      tft.print("Not connected.");
    }
  } else {
    drawIpRow(76,  "IP address", g_staticIp);
    drawIpRow(104, "Subnet mask", g_staticMask);
    drawIpRow(132, "Gateway",    g_staticGw);
    drawIpRow(160, "DNS",        g_staticDns);
    drawIpRow(188, "Hostname",   g_hostname);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(8, 218);
    tft.print("Blank DNS uses the gateway.");
    tft.setCursor(8, 228);
    tft.print("Incomplete fields fall back to DHCP.");
  }
}

void handleIpConfigTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) {   // Back -> network list
    g_wifiSub = 0;
    // If addressing settings changed while connected, drop the link so the
    // loop() reconnect path re-applies them via applyNetConfig().
    if (g_netCfgDirty && WiFi.status() == WL_CONNECTED) WiFi.disconnect();
    g_netCfgDirty = false;
    dirty = true;
    return;
  }
  if (inRect(x, y, 230, 40, 312, 64)) {  // DHCP/Static toggle
    g_ipDhcp = !g_ipDhcp;
    saveNetCfg();
    g_netCfgDirty = true;
    dirty = true;
    return;
  }
  // Edit buttons (right edge of each row). DHCP shows only the hostname row;
  // static shows all five, matching drawIpConfig()'s layout.
  if (!g_ipDhcp) {
    if      (inRect(x, y, 270,  76, 312, 100)) g_wifiSub = 21;
    else if (inRect(x, y, 270, 104, 312, 128)) g_wifiSub = 22;
    else if (inRect(x, y, 270, 132, 312, 156)) g_wifiSub = 23;
    else if (inRect(x, y, 270, 160, 312, 184)) g_wifiSub = 24;
    else if (inRect(x, y, 270, 188, 312, 212)) g_wifiSub = 25;
    else return;
  } else {
    if (!inRect(x, y, 270, 76, 312, 100)) return;
    g_wifiSub = 25;
  }
  dirty = true;
}

// ---- on-screen keyboard (QWERTY + Shift + symbols) ----
// Three rows of 10 keys at y 60..143; a bottom control bar at y 144..219.
char keyFromXY(int x, int y) {
  int row = (y - 60) / 28;
  if (row < 0 || row > 2) return 0;
  int col = constrain(x / 32, 0, 9);
  char c;
  if (!g_kbSym) {
    if (row == 0) c = "QWERTYUIOP"[col];
    else if (row == 1) c = "ASDFGHJKL."[col];
    else c = "ZXCVBNM-_@"[col];
    // shift only affects letters (symbols stay as-is)
    if (c >= 'A' && c <= 'Z' && !g_kbShift) c = c + 32;
  } else {
    if (row == 0) c = "1234567890"[col];
    else if (row == 1) c = "-@#$%&*()_"[col];
    else c = ",.!?'\";:/+"[col];
  }
  return c;
}

// ---- cursor-based text editing in the on-screen keyboard ----
// g_kbCursor is the edit cursor index (0..length) within the active field. It
// is reset to the end whenever the active field (g_wifiSub) changes. Tapping
// the text field places the cursor; letters/space/del act at that position.

// Compute the horizontal scroll so the cursor stays visible. Returns the index
// of the first visible character and its cumulative pixel width (the base used
// to align drawn text and the cursor bar, so they never drift apart).
void kbVisibleRange(const String& text, int& start, int& startWidth, int avail) {
  int len = text.length();
  if (g_kbCursor < 0) g_kbCursor = 0;
  if (g_kbCursor > len) g_kbCursor = len;
  int cursorX = tft.textWidth(text.substring(0, g_kbCursor));
  int off = 0;
  if (cursorX - off > avail - 2) off = cursorX - (avail - 2);
  if (cursorX - off < 0) off = cursorX;
  start = 0;
  while (start < len && tft.textWidth(text.substring(0, start + 1)) <= off) start++;
  startWidth = tft.textWidth(text.substring(0, start));
}

// Map a pixel offset from the start of the full text to the nearest character
// boundary index (cursor position).
int cursorIndexAt(const String& text, int textX) {
  if (textX <= 0) return 0;
  if (textX >= tft.textWidth(text)) return text.length();
  int best = 0, bestD = abs(textX);
  for (unsigned int i = 1; i <= text.length(); i++) {
    int d = abs(textX - tft.textWidth(text.substring(0, i)));
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

// Draw the editable text field with the cursor bar and horizontal scrolling.
// `avail` is the usable text width (narrower for password fields, which have a
// View/Hide toggle on the right). Password fields are masked unless g_kbShow.
void drawEditableField(const String& text, bool pw, int avail) {
  tft.fillRoundRect(6, 34, 308, 22, 4, TFT_DARKGREY);
  tft.setTextColor(TFT_GREENYELLOW, TFT_DARKGREY);
  tft.setTextFont(2);

  int len = text.length();
  if (g_kbCursor < 0) g_kbCursor = 0;
  if (g_kbCursor > len) g_kbCursor = len;
  int start, startWidth;
  kbVisibleRange(text, start, startWidth, avail);
  int cursorX = tft.textWidth(text.substring(0, g_kbCursor));
  int cursorScreenX = 10 + (cursorX - startWidth);

  String visible = text.substring(start);
  if (pw && !g_kbShow) {
    String mask;
    for (int i = 0; i < visible.length(); i++) mask += '*';
    visible = mask;
  }

  tft.setCursor(10, 38);
  tft.print(visible);

  // cursor bar, clamped into the visible field
  if (cursorScreenX < 10) cursorScreenX = 10;
  if (cursorScreenX > 10 + avail) cursorScreenX = 10 + avail;
  tft.drawFastVLine(cursorScreenX, 36, 18, TFT_WHITE);
}

void drawKeyboard(const String& title, const String& text, bool pw) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  tft.print(title.substring(0, 24));
  tft.fillRoundRect(265, 4, 50, 20, 5, TFT_MAROON);
  tft.setCursor(274, 7);
  tft.setTextColor(TFT_WHITE, TFT_MAROON);
  tft.print("Back");

  // Reset the edit cursor to the end whenever the active field changes.
  if (g_wifiSub != g_lastKbSub) { g_kbCursor = text.length(); g_lastKbSub = g_wifiSub; g_kbShow = false; }
  // Password fields leave room on the right for the View/Hide toggle.
  drawEditableField(text, pw, pw ? 252 : 300);

  // View/Hide toggle for password fields (reveals the value in cleartext).
  if (pw) {
    uint16_t bg = g_kbShow ? TFT_DARKGREY : TFT_NAVY;
    tft.fillRoundRect(264, 35, 48, 20, 4, bg);
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextFont(1);
    tft.setCursor(273, 41);
    tft.print(g_kbShow ? "Hide" : "View");
  }

  // letter / symbol rows
  static const char* ROWS[2][3] = {
    { "QWERTYUIOP", "ASDFGHJKL.", "ZXCVBNM-_@" },   // letters
    { "1234567890", "-@#$%&*()_", ",.!?'\";:/+" },  // symbols
  };
  const char** rows = ROWS[g_kbSym ? 1 : 0];
  int yy = 60;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 10; c++) {
      int x0 = c * 32;
      tft.fillRect(x0 + 1, yy, 30, 24, TFT_DARKGREY);
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.setTextFont(2);
      char ch = rows[r][c];
      if (ch >= 'A' && ch <= 'Z' && !g_kbShift) ch = ch + 32;   // show lowercase unless shift
      tft.setCursor(x0 + 9, yy + 4);
      tft.print(ch);
    }
    yy += 28;
  }

  // bottom control bar: Shift | ?123/abc | space | del | OK
  const int ctrlY = 144, ctrlH = 75, ctrlW = 64;
  const char* labels[5] = { "Shift", g_kbSym ? "abc" : "?123", "space", "del", "OK" };
  uint16_t cols[5] = { g_kbShift ? TFT_YELLOW : TFT_NAVY, TFT_NAVY, TFT_NAVY, TFT_MAROON, TFT_DARKGREEN };
  for (int i = 0; i < 5; i++) {
    int x = i * ctrlW;
    uint16_t bg = cols[i];
    tft.fillRoundRect(x, ctrlY, ctrlW - 2, ctrlH, 5, bg);
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextFont(2);
    tft.setCursor(x + ctrlW / 2 - strlen(labels[i]) * 4, ctrlY + 30);
    tft.print(labels[i]);
  }

  // Cursor navigation row for long text: < moves the cursor left, > right.
  tft.fillRoundRect(8, 220, 148, 18, 4, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(2);
  tft.setCursor(72, 222);
  tft.print("<");
  tft.fillRoundRect(164, 220, 148, 18, 4, TFT_DARKGREY);
  tft.setCursor(228, 222);
  tft.print(">");
}

void handleKeyboardTouch(uint16_t x, uint16_t y) {
  if (inRect(x, y, 265, 4, 315, 24)) {  // Back
    g_kbShift = false; g_kbSym = false; g_kbShow = false;
    g_lastKbSub = -1;   // next entry starts with the cursor at the end
    if (g_wifiSub == 3) { g_screen = SCR_FTRACKER; dirty = true; return; }   // OpenSky creds
    if (g_wifiSub == 4) { g_wifiSub = 3; dirty = true; return; }
    if (g_wifiSub == 5) { g_screen = SCR_LOCATION; dirty = true; return; }   // address search
    if (g_wifiSub == 6 || g_wifiSub == 7 || g_wifiSub == 8) { g_screen = SCR_SLEEP; dirty = true; return; }
    if (g_wifiSub == 9) { g_screen = SCR_POOL; dirty = true; return; }       // Govee key
    if (g_wifiSub == 10) { g_screen = SCR_LOCATION; dirty = true; return; }  // lat/lon
    if (g_wifiSub == 11) { g_screen = SCR_FTRACKER; dirty = true; return; }   // home airport
    if (g_wifiSub >= 21 && g_wifiSub <= 25) { g_wifiSub = 20; dirty = true; return; }  // IP field
    g_wifiSub = 0; dirty = true; return;
  }

  String* bp;
  switch (g_wifiSub) {
    case 1: bp = &g_ssid; break;
    case 2: bp = &g_pass; break;
    case 3: bp = &g_osClientId; break;
    case 4: bp = &g_osClientSecret; break;
    case 6: bp = &g_sleepStartStr; break;
    case 7: bp = &g_sleepEndStr; break;
    case 8: bp = &g_wakeStr; break;
    case 9: bp = &g_goveeKey; break;
    case 10: bp = &g_latLonStr; break;
    case 11: bp = &g_homeAirport; break;
    case 21: bp = &g_staticIp; break;
    case 22: bp = &g_staticMask; break;
    case 23: bp = &g_staticGw; break;
    case 24: bp = &g_staticDns; break;
    case 25: bp = &g_hostname; break;
    default: bp = &g_addrSearch; break;
  }
  String& buf = *bp;

  // View/Hide toggle for password fields (right side of the text field).
  if (kbFieldPw() && inRect(x, y, 264, 35, 312, 55)) {
    g_kbShow = !g_kbShow;
    dirty = true;
    return;
  }

  // Tap in the text field to place the edit cursor at that character.
  if (x >= 6 && x <= 314 && y >= 34 && y <= 56) {
    int start, startWidth;
    kbVisibleRange(buf, start, startWidth, kbFieldPw() ? 252 : 300);
    int fullX = (x - 10) + startWidth;
    g_kbCursor = cursorIndexAt(buf, fullX);
    dirty = true;
    return;
  }

  // Cursor navigation row (< / >) at the very bottom: move the cursor left/right.
  if (y >= 220) {
    if (inRect(x, y, 8, 220, 156, 239) && g_kbCursor > 0) { g_kbCursor--; dirty = true; }
    else if (inRect(x, y, 164, 220, 312, 239) && g_kbCursor < (int)buf.length()) { g_kbCursor++; dirty = true; }
    return;
  }

  int maxlen;
  switch (g_wifiSub) {
    case 1: maxlen = 32; break;
    case 6: case 7: maxlen = 4; break;
    case 8: maxlen = 3; break;
    case 9: maxlen = 80; break;
    case 10: maxlen = 24; break;
    case 11: maxlen = 6; break;
    case 21: case 22: case 23: case 24: maxlen = 15; break;  // IP addresses
    case 25: maxlen = 32; break;   // hostname
    default: maxlen = 63; break;
  }

  // bottom control bar (y 144..219): Shift | ?123/abc | space | del | OK
  if (y >= 144 && y <= 219) {
    int i = constrain(x / 64, 0, 4);
    if (i == 0) { g_kbShift = !g_kbShift; dirty = true; }
    else if (i == 1) { g_kbSym = !g_kbSym; g_kbShift = false; dirty = true; }
    else if (i == 2) {  // space (insert at cursor)
      if (buf.length() < maxlen) {
        buf = buf.substring(0, g_kbCursor) + ' ' + buf.substring(g_kbCursor);
        g_kbCursor++;
        dirty = true;
      }
    }
    else if (i == 3) {  // del / backspace at cursor
      if (g_kbCursor > 0 && buf.length() > 0) {
        buf.remove(g_kbCursor - 1, 1);
        g_kbCursor--;
        dirty = true;
      }
    }
    else if (i == 4) {  // OK
      g_kbShift = false; g_kbSym = false; g_kbShow = false;
      if (g_wifiSub == 1) { g_wifiSub = 2; dirty = true; }
      else if (g_wifiSub == 2 && g_ssid.length() > 0) saveAndConnectWifi();
      else if (g_wifiSub == 3) { g_wifiSub = 4; dirty = true; }
      else if (g_wifiSub == 4) { saveOsCreds(); g_screen = SCR_FTRACKER; dirty = true; }
      else if (g_wifiSub == 5) {
        // Address search. Show a "Searching..." screen during the blocking
        // geocode call, and on failure keep the user in the flow (sub 12) so
        // they can edit the address instead of starting over from empty.
        if (g_addrSearch.length() == 0) {
          g_addrErr = "empty";
          g_wifiSub = 12;
          dirty = true;
        } else {
          drawSearchingAddr(g_addrSearch);
          if (geocodeAddress()) {
            g_addrErr = "";
            g_screen = SCR_LOCATION;
          } else {
            g_addrErr = (String(lastErr) == "no match") ? "no-match"
                                                         : friendlyGeoError(lastErr);
            g_wifiSub = 12;   // stay in the flow; "Fix address" returns to edit
          }
          dirty = true;
        }
      }
      else if (g_wifiSub == 6) { commitSleepTime(6); g_screen = SCR_SLEEP; dirty = true; }
      else if (g_wifiSub == 7) { commitSleepTime(7); g_screen = SCR_SLEEP; dirty = true; }
      else if (g_wifiSub == 8) {
        g_wakeMin = constrain(g_wakeStr.toInt(), 1, 120);
        prefs.begin("flight", false); prefs.putInt("wake", g_wakeMin); prefs.end();
        g_screen = SCR_SLEEP; dirty = true;
      }
      else if (g_wifiSub == 9) {
        prefs.begin("flight", false); prefs.putString("govee", g_goveeKey); prefs.end();
        g_screen = SCR_POOL; dirty = true;
      }
      else if (g_wifiSub == 10) {
        // Parse "lat,lon" and save.
        int comma = g_latLonStr.indexOf(',');
        if (comma > 0) {
          float lat = atof(g_latLonStr.substring(0, comma).c_str());
          float lon = atof(g_latLonStr.substring(comma + 1).c_str());
          if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
            g_lat = lat; g_lon = lon;
            saveFloat("lat", g_lat);
            saveFloat("lon", g_lon);
            g_lastPlace = "";   // manual override clears the address confirmation
            netWantWeather = true;
          }
        }
        g_screen = SCR_LOCATION; dirty = true;
      }
      else if (g_wifiSub == 11) { saveHomeAirport(); g_screen = SCR_FTRACKER; dirty = true; }
      else if (g_wifiSub >= 21 && g_wifiSub <= 25) { saveNetCfg(); g_netCfgDirty = true; g_wifiSub = 20; dirty = true; }
    }
    return;
  }

  // letter/symbol key
  char c = keyFromXY(x, y);
  if (c) {
    if (buf.length() < maxlen) {
      buf = buf.substring(0, g_kbCursor) + c + buf.substring(g_kbCursor);
      g_kbCursor++;
      dirty = true;
    }
    // tap one uppercase char per Shift press, then drop back to lowercase
    if (g_kbShift && c >= 'A' && c <= 'Z') { g_kbShift = false; dirty = true; }
  }
}

// Parse and persist a sleep start/end time from the "HHMM" edit buffer.
void commitSleepTime(int which) {
  String& s = (which == 6) ? g_sleepStartStr : g_sleepEndStr;
  int h = constrain(s.substring(0, 2).toInt(), 0, 23);
  int m = constrain(s.substring(2, 4).toInt(), 0, 59);
  // normalize back to HHMM
  char buf[8];
  snprintf(buf, sizeof buf, "%02d%02d", h, m);
  s = buf;
  prefs.begin("flight", false);
  if (which == 6) { g_sleepStartH = h; g_sleepStartM = m; prefs.putInt("sleepsH", h); prefs.putInt("sleepsM", m); }
  else            { g_sleepEndH   = h; g_sleepEndM   = m; prefs.putInt("sleepeH", h); prefs.putInt("sleepeM", m); }
  prefs.end();
}

// ---- Serial NVS provisioning ----
// Listens for a few seconds at boot for "KEY=VALUE" lines over USB serial and
// writes each recognized key straight into NVS via Preferences. This lets you
// provision WiFi/OpenSky/Govee credentials without compiling them into the
// firmware and without touching the filesystem (so pool temp history is preserved).
// The host side is provision_config.py; it sends the lines and resets the board.
// A blank line "@END" exits the window early.
//
// TEMP / TESTING ONLY: to strip this out for production, set
// ENABLE_SERIAL_PROVISION to 0 in cyd-dashboard.ino (or delete the whole
// block below and the serialProvision() call in setup()). Credentials stay in
// NVS after provisioning, so the board keeps working without this code.
// (ENABLE_SERIAL_PROVISION is defined in cyd-dashboard.ino so it is visible
// before this file in Arduino's alphabetical concatenation.)
#if ENABLE_SERIAL_PROVISION
#define PROVISION_WINDOW_MS 4000UL

// OTA command state for building a URL from IP + filename (or direct URL)
static String s_otaIp = "";
static String s_otaFile = "";

static String buildOtaUrl(const String& ipOrUrl, const String& file) {
  // If a full URL was given (starts with http:// or https://), use it directly.
  if (ipOrUrl.startsWith("http://") || ipOrUrl.startsWith("https://")) return ipOrUrl;
  // Otherwise treat ipOrUrl as just an IP/host and build the local replay server URL.
  // OTA firmware is served over plain HTTP on port 8080; all other replay data is HTTPS on 8081.
  String url = "http://" + ipOrUrl + ":8080/firmware";
  if (file.length()) url += "?file=" + file;
  return url;
}

static bool scheduleOTA(const String& url) {
#if !ENABLE_LOCAL_OTA
  (void)url;
  return false;
#else
  if (!isDevBuild()) return false;
  extern String g_otaUrl;
  extern String g_otaVersion;
  extern String g_otaSha256;
  extern bool g_otaActive;
  g_otaUrl = url;
  g_otaVersion = "dev";
  g_otaSha256 = ""; // Local dev images have no release digest.
  g_otaActive = true;
  return true;
#endif
}

bool provisionKey(const String& key, const String& val) {
  if      (key == "WIFI_SSID")       { prefs.putString("ssid",   val); g_savedSsid = val; return true; }
  else if (key == "WIFI_PASSWORD")   { prefs.putString("pass",   val); g_savedPass = val; return true; }
  else if (key == "WIFI_MODE")       { bool d = !val.equalsIgnoreCase("static"); g_ipDhcp = d; prefs.putBool("ipdhcp", d); return true; }
  else if (key == "WIFI_IP")         { prefs.putString("ipaddr", val);  g_staticIp = val; return true; }
  else if (key == "WIFI_SUBNET")     { prefs.putString("ipmask", val);  g_staticMask = val; return true; }
  else if (key == "WIFI_GATEWAY")    { prefs.putString("ipgw",   val);  g_staticGw = val; return true; }
  else if (key == "WIFI_DNS")        { prefs.putString("ipdns",  val);  g_staticDns = val; return true; }
  else if (key == "WIFI_HOSTNAME")   { prefs.putString("hostname", val); g_hostname = val; return true; }
  else if (key == "OPENSKY_CLIENT_ID")     { prefs.putString("oscid",  val); g_osClientId     = val; return true; }
  else if (key == "OPENSKY_CLIENT_SECRET") { prefs.putString("ocssec", val); g_osClientSecret = val; return true; }
  else if (key == "HOME_AIRPORT")          { String v = val; v.toUpperCase(); prefs.putString("homeap", v); g_homeAirport = v; return true; }
  else if (key == "GOVEE_KEY")       { prefs.putString("govee",  val); g_goveeKey  = val; return true; }
  else if (key == "OTA_URL")         {
    String url = buildOtaUrl(val, s_otaFile);
    if (scheduleOTA(url)) {
      Serial.println("PROV: OTA_URL=" + url + " scheduled");
      return true;
    }
  }
  else if (key == "OTA_IP")          {
    s_otaIp = val;
    g_otaHost = val;
    prefs.begin("flight", false);
    prefs.putString("otahost", val);
    prefs.end();
    Serial.println("PROV: OTA_IP=" + val + " saved");
    return true;
  }
  else if (key == "OTA_FILE")        { s_otaFile = val; Serial.println("PROV: OTA_FILE=" + val); return true; }
  else if (key == "OTA_GO")          {
    String url = buildOtaUrl(s_otaIp, s_otaFile);
    if (url.length() < 10) { Serial.println("PROV: OTA_GO missing OTA_IP/OTA_FILE"); return false; }
    if (scheduleOTA(url)) {
      Serial.println("PROV: OTA_GO=" + url + " scheduled");
      return true;
    }
  }
  return false;
}

void serialProvision() {
  Serial.println("PROV: listen " + String(PROVISION_WINDOW_MS / 1000) + "s for KEY=VALUE");
  String buf;
  unsigned long start = millis();
  bool changed = false;
  while ((long)(millis() - start) < PROVISION_WINDOW_MS) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        buf.trim();
        if (buf == "@END") { buf = ""; goto done; }
        int eq = buf.indexOf('=');
        if (eq > 0) {
          String key = buf.substring(0, eq); key.trim();
          String val = buf.substring(eq + 1); val.trim();
          if (provisionKey(key, val)) {
            Serial.println("PROV: " + key + "=OK");
            changed = true;
          } else {
            Serial.println("PROV: " + key + " ignored");
          }
        }
        buf = "";
      } else if (c >= ' ') {
        buf += c;
      }
    }
  }
done:
  if (changed) Serial.println("PROV: done");
}

// Runtime serial command handler for OTA (can be called anytime from loop)
// Supports:
//   OTA_URL=http://server:port/firmware.bin   (full URL, legacy)
//   OTA_IP=192.168.x.x                        (server IP)
//   OTA_FILE=cyd-dashboard.ino.bin            (firmware filename)
//   OTA_GO                                      (trigger with OTA_IP + OTA_FILE)
void handleSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\n' && c != '\r') {
      if (line.length() < 128) line += c;
      continue;
    }
    if (line.length() == 0) continue;
    String buf = line;
    line = "";
    buf.trim();
    if (buf.length() == 0) continue;

    int eq = buf.indexOf('=');
    String key, val;
    if (eq > 0) {
      key = buf.substring(0, eq); key.trim();
      val = buf.substring(eq + 1); val.trim();
    } else if (buf == "OTA_GO") {
      key = buf;
      val = "";
    } else {
      Serial.println("CMD: unknown command " + buf);
      continue;
    }

    if (key == "OTA_URL") {
      String url = buildOtaUrl(val, s_otaFile);
      if (scheduleOTA(url)) {
        Serial.println("CMD: OTA scheduled from " + url);
      }
    } else if (key == "OTA_IP") {
      s_otaIp = val;
      g_otaHost = val;
      prefs.begin("flight", false);
      prefs.putString("otahost", val);
      prefs.end();
      Serial.println("CMD: OTA_IP=" + val + " saved");
    } else if (key == "OTA_FILE") {
      s_otaFile = val;
      Serial.println("CMD: OTA_FILE=" + val);
    } else if (key == "OTA_GO") {
      String url = buildOtaUrl(s_otaIp, s_otaFile);
      if (url.length() < 10) {
        Serial.println("CMD: OTA_GO missing OTA_IP/OTA_FILE");
      } else if (scheduleOTA(url)) {
        Serial.println("CMD: OTA scheduled from " + url);
      }
    } else {
      Serial.println("CMD: unknown command " + key);
    }
  }
}
#endif  // ENABLE_SERIAL_PROVISION

// Persist OpenSky credentials to NVS.
void saveOsCreds() {
  prefs.begin("flight", false);
  prefs.putString("oscid",  g_osClientId);
  prefs.putString("ocssec", g_osClientSecret);
  prefs.end();
}

// Persist the "home airport" route-display setting to NVS (uppercased).
void saveHomeAirport() {
  g_homeAirport.toUpperCase();
  prefs.begin("flight", false);
  prefs.putString("homeap", g_homeAirport);
  prefs.end();
}

void saveAndConnectWifi() {
  g_savedSsid = g_ssid;
  g_savedPass = g_pass;
  prefs.begin("flight", false);
  prefs.putString("ssid", g_ssid);
  prefs.putString("pass", g_pass);
  prefs.end();
  connected = tryConnect(g_ssid.c_str(), g_pass.c_str());
  if (!connected) snprintf(lastErr, sizeof lastErr, "wifi failed");
  g_bootStage = BOOT_DONE;   // end the first-boot wizard once creds are saved
  g_screen = SCR_DASH;
  dirty = true;
}

// ---- Address search: user-friendly flow (see also geocodeAddress) ----
// g_addrSearch holds the text being edited (kept across visits so a failed
// search can be fixed). wifiSub 5 = edit keyboard, 12 = result/status screen.

// Brief "Searching..." screen shown while the (blocking) geocode request runs.
void drawSearchingAddr(const String& q) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(40, 90);
  tft.print("Searching for:");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setCursor(40, 114);
  tft.print(q.substring(0, 40));
}

// Map the lastErr code from a failed geocode to a friendly message.
String friendlyGeoError(const char* code) {
  String s = code;
  if (s.startsWith("geo 0"))  return "No internet connection.";
  if (s.startsWith("geo 429")) return "Search service busy - try again.";
  if (s.startsWith("geo 4") || s.startsWith("geo 5")) return "Search service error - try again.";
  if (s == "geo json") return "Unexpected reply from the server.";
  return "Could not complete the search.";
}

// Result/status screen after an address search. Any tap returns to editing the
// address so the user can fix it rather than start over.
void drawAddrStatus() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 28, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(8, 6);
  if (g_addrErr == "empty")            tft.print("Address search");
  else if (g_addrErr == "no-match")    tft.print("Address not found");
  else                                 tft.print("Search failed");

  if (g_addrErr == "empty") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(30, 96);
    tft.print("Type an address first.");
  } else if (g_addrErr == "no-match") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(10, 50);
    tft.print("No result for:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(10, 72);
    tft.print(g_addrSearch.substring(0, 42));
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(10, 100);
    tft.print("Try a city name, or a street");
    tft.setCursor(10, 112);
    tft.print("address like:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 128);
    tft.print("123 Main St, Springfield");
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(10, 152);
    tft.print("You can also set lat/lon manually");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(10, 60);
    tft.print(g_addrErr.substring(0, 40));
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(10, 100);
    tft.print("Make sure the device is");
    tft.setCursor(10, 112);
    tft.print("connected to WiFi, then retry.");
  }

  tft.fillRoundRect(10, 200, 300, 30, 6, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextFont(2);
  tft.setCursor(86, 207);
  tft.print("Fix address");
}

void handleAddrStatusTouch(uint16_t x, uint16_t y) {
  // Any tap returns to the address editor with the text intact.
  g_wifiSub = 5;
  dirty = true;
}
