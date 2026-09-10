// flight_details.ino - origin/destination + flight details view.
//
// When a new plane becomes the nearest overhead aircraft, its route
// (origin/destination airports) is fetched automatically the first time it is
// seen via the OpenSky `flights/aircraft` endpoint and cached (g_routeFetched,
// reset whenever the overhead identity changes), so it only costs credits once
// per distinct plane. Tapping "Details" only recalls/redisplays that cached
// route — it does not trigger a new fetch. Airport ICAO codes are mapped to
// city names via a small embedded table; unknown codes fall back to showing
// the raw code.

// ---- Airport ICAO -> city lookup (major US + some intl). Fallback = raw code.
struct Airport { const char* icao; const char* city; };
static const Airport kAirports[] = {
  {"KDFW", "Dallas-Ft Worth"}, {"KDAL", "Dallas"},      {"KHOU", "Houston"},
  {"KIAH", "Houston"},         {"KAUS", "Austin"},      {"KSAT", "San Antonio"},
  {"KELP", "El Paso"},         {"KCRP", "Corpus Christi"},
  {"KATL", "Atlanta"},         {"KLAX", "Los Angeles"}, {"KJFK", "New York"},
  {"KLGA", "New York"},        {"KEWR", "Newark"},      {"KORD", "Chicago"},
  {"KMDW", "Chicago"},         {"KSEA", "Seattle"},     {"KSFO", "San Francisco"},
  {"KOAK", "Oakland"},         {"KSJC", "San Jose"},    {"KDEN", "Denver"},
  {"KPHX", "Phoenix"},         {"KPHL", "Philadelphia"},{"KBOS", "Boston"},
  {"KMIA", "Miami"},           {"KMCO", "Orlando"},     {"KTPA", "Tampa"},
  {"KLAS", "Las Vegas"},       {"KMSP", "Minneapolis"}, {"KDTW", "Detroit"},
  {"KCLT", "Charlotte"},       {"KSTL", "St Louis"},    {"KPIT", "Pittsburgh"},
  {"KIND", "Indianapolis"},    {"KCLE", "Cleveland"},   {"KCVG", "Cincinnati"},
  {"KMEM", "Memphis"},         {"KBNA", "Nashville"},   {"KJAX", "Jacksonville"},
  {"KRDU", "Raleigh"},          {"KABQ", "Albuquerque"},
  {"KSLC", "Salt Lake City"},  {"KPDX", "Portland"},    {"KSAN", "San Diego"},
  {"KDCA", "Washington"},      {"KIAD", "Washington"},  {"KBWI", "Baltimore"},
};
static const int kNumAirports = sizeof(kAirports) / sizeof(kAirports[0]);

String airportCity(const char* icao) {
  if (!icao || icao[0] == 0) return "--";
  for (int i = 0; i < kNumAirports; i++) {
    if (strcmp(kAirports[i].icao, icao) == 0) return kAirports[i].city;
  }
  return "--";   // unknown -> no city (don't echo the code and look like a route)
}

// ---- Airline lookup (ICAO 3-letter callsign prefix -> name + brand color) ----
struct Airline { const char* code; const char* name; uint16_t color; };
static const Airline kAirlines[] = {
  {"AAL", "American Airlines", TFT_BLUE},
  {"DAL", "Delta Air Lines",   TFT_RED},
  {"UAL", "United Airlines",   TFT_NAVY},
  {"SWA", "Southwest Airlines",TFT_RED},
  {"SCX", "Sun Country Airlines",TFT_RED},
  {"FDX", "FedEx Express",     TFT_PURPLE},
  {"UPS", "UPS Airlines",      TFT_MAROON},
  {"JBU", "JetBlue",           TFT_BLUE},
  {"ASA", "Alaska Airlines",   TFT_DARKCYAN},
  {"AAY", "Allegiant Air",     TFT_RED},
  {"FFT", "Frontier Airlines", TFT_DARKGREEN},
  {"NKS", "Spirit Airlines",   TFT_ORANGE},
  {"SKW", "SkyWest Airlines",  TFT_NAVY},
  {"ENY", "Envoy Air",         TFT_BLUE},
  {"RPA", "Republic Airways",  TFT_NAVY},
  {"EJA", "NetJets",           TFT_DARKGREY},
  {"BAW", "British Airways",   TFT_BLUE},
  {"AFR", "Air France",        TFT_NAVY},
  {"DLH", "Lufthansa",         TFT_YELLOW},
  {"KLM", "KLM Royal Dutch",   TFT_BLUE},
  {"UAE", "Emirates",          TFT_RED},
  {"QTR", "Qatar Airways",     TFT_PURPLE},
  {"ACA", "Air Canada",        TFT_RED},
  {"ANA", "All Nippon Airways",TFT_RED},
  {"EIN", "Aer Lingus",        TFT_GREEN},
  {"AMX", "Aeromexico",        TFT_NAVY},
  {"CCA", "Air China",         TFT_RED},
  {"AXM", "AirAsia",           TFT_RED},
  {"CPA", "Cathay Pacific",    TFT_GREEN},
  {"CES", "China Eastern",     TFT_RED},
  {"CSN", "China Southern",    TFT_BLUE},
  {"CMP", "Copa Airlines",     TFT_RED},
  {"ETD", "Etihad Airways",    TFT_YELLOW},
  {"EWG", "Eurowings",         TFT_RED},
  {"FIN", "Finnair",           TFT_NAVY},
  {"CHH", "Hainan Airlines",   TFT_RED},
  {"HAL", "Hawaiian Airlines", TFT_PURPLE},
  {"IBE", "Iberia",            TFT_RED},
  {"ICE", "Icelandair",        TFT_NAVY},
  {"IGO", "IndiGo",            TFT_BLUE},
  {"JAL", "Japan Airlines",    TFT_RED},
  {"KAL", "Korean Air",        TFT_BLUE},
  {"NAX", "Norwegian",         TFT_RED},
  {"QFA", "Qantas Airways",    TFT_RED},
  {"RYR", "Ryanair",           TFT_YELLOW},
  {"SAS", "Scandinavian Airlines", TFT_NAVY},
  {"SIA", "Singapore Airlines",TFT_YELLOW},
  {"SWR", "Swiss International",TFT_RED},
  {"AVA", "Avianca",           TFT_RED},
  {"TAM", "TAM (LATAM)",       TFT_RED},
  {"TAP", "TAP Air Portugal",  TFT_RED},
  {"THY", "Turkish Airlines",  TFT_RED},
  {"VIR", "Virgin Atlantic",   TFT_RED},
  {"VLG", "Vueling",           TFT_RED},
  {"WJA", "WestJet",           TFT_RED},
  {"WZZ", "Wizz Air",          TFT_MAGENTA},
  {"EZY", "easyJet",           TFT_ORANGE},
};
static const int kNumAirlines = sizeof(kAirlines) / sizeof(kAirlines[0]);

// Returns true if the airline is known, filling `name` and `color`.
bool airlineInfo(const char* callsign, String& name, uint16_t& color) {
  if (!callsign || callsign[0] == 0) return false;
  char code[4] = {};
  for (int i = 0; i < 3 && callsign[i]; i++) {
    code[i] = (char)toupper((unsigned char)callsign[i]);
  }
  for (int i = 0; i < kNumAirlines; i++) {
    if (strncmp(code, kAirlines[i].code, sizeof code - 1) == 0) {
      name = kAirlines[i].name;
      color = kAirlines[i].color;
      return true;
    }
  }
  return false;
}

// findAirlineLogo() is defined in logos.ino: it loads logos from the dedicated
// LittleFS "logos" partition at runtime (single reusable buffer, PSRAM-first),
// and is declared at the top of cyd-dashboard.ino.

// Top US airports by 2023 enplanements (ICAO codes), from FAA commercial
// service enplanement data. KDFW is included as a major hub and blinks blue
// like any other top-50 airport unless the user has also set it as their home
// airport. Used to blink the LED (blue) when an overhead flight involves one
// of these airports.
static const char* const kTopAirports[] = {
  "KATL","KLAX","KDFW","KDEN","KORD","KJFK","KMCO","KLAS","KCLT","KMIA",
  "KSEA","KEWR","KSFO","KPHX","KIAH","KBOS","KFLL","KMSP","KLGA","KDTW",
  "KPHL","KSLC","KBWI","KDCA","KSAN","KIAD","KTPA","KBNA","KAUS","KMDW","PHNL",
  "KDAL","KPDX","KSTL","KRDU","KHOU","KSMF","KMSY","TJSJ","KSJC","KSNA",
  "KMCI","KOAK","KSAT","KRSW","KCLE","KIND","KPIT","KCVG","KCMH","PHOG",
  "KJAX","KONT","KBUR","KBDL","KCHS","KMKE","PANC","KABQ","KOMA","KMEM",
  "KRIC","KBOI","KORF","KBUF","KSDF","KRNO","KSRQ","KOKC","PHKO","KELP",
  "KGEG","KTUS","KSAV","KGRR","KLGB","PHLI","KPVD","KMYR","KPSP","KTUL",
  "KDSM","KBHM","KSFB","KSYR","KTYS","KALB","KPNS","KROC","KGSP","KPIE",
  "KBZN","KFAT","KCOS","KHPN","KAVL","KVPS","KPWM","KLIT","KMSN","KXNA",
  "KPGD","KGSO","PGUM","KICT","KEUG","KECP","KHSV","TIST","KCID","KMAF",
  "PHTO","KEYW","KLEX","KILM","KFSD","KMDT","KBTV","KMHT","KISP","KSBA",
  "KSGF","KJAN","KDAY","KCAE","KRDM","PAFA","KLBB","KHRL","KFAR","KMFE",
  "KJAC","KHVN","KCHA","KMFR","KATW","KMSO","PAJN","KPSC","KACY","TJBQ",
  "KBIL","KABE","KTLH","KPVU","KSBN","KAMA","KFWA","KBTR","KGPT","KMLB",
  "KBGR","KCRP","KDAB","KTVC","KROA","KRAP","KCAK","KGRB","KTTN","KSBP",
  "KSTS","KPIA","KBLI","KASE","KSHV","KCHO","KPAE","KFNT","KAGS","KMOB",
  "KGNV","KMLI","KIDA","KMRY","KBIS","KMTJ","KGJT","TISX","KLFT","KEGE",
  "KTRI","KDRO","KHDN","KCRW","KMGM","KGTF","KAEX","KBRO","KBFL","KAVP",
  "KFAY","KEVV","KBMI","PABE","KLRD","KLCK","PAKT","KBLV","KMOT","TJPS",
  "KSAF","KACK","KSGU","KOAJ","KILG","KLNK","KSWF","KDLH","KRFD","KACV",
  "KLAN","KGRK","KSUN","KORH","KCOU","KMLU","PASI","KGFK","KMBS","KRST",
  "KHTS","KHLN","KRDD","KAZO","KCPR","PHMK","PADQ","KELM","KCWA","KMVY",
  "KLCH","PASC","KXWA","KABI","KLBE","KLYH","KFLG","KPBG","KTOL","KCMI",
  "KPHF","KSCK","PAEN","PAOM","KCSG","KGRI","KPUW","KITH","PAOT","KCLL",
  "KEWN","KFSM","KPSM","KIAG","KSBY","KACT","KSJT","KMHK","KSPI","KTYR",
  "PABG","KGUC","KERI","KLAW","KROW","KLSE","NSTU","KCKB","KGTR","KVRB",
  "KVLD","KTXK","PABR","PAKN","KLWS","KBGM","KDHN","KSWO","KPGV","PADL",
  "KCOD","PAHO","PHNY","KBQK","KGGG","KHGR","KBPT","PAMR","KABY","TJVQ",
  "KGCK","KEAT","KBFI","KSBD","KSUX","KGCC","KSHR","KALW","KEAU","KBJI",
  "KSPS","PAPG","KPRC","KDIK","KYKM","KCMX","KFLO","KPLN","KHOB","KCIU",
  "KABR","KART","KRHI","KTWF","KOTH","KSTC","KCNY","KPQI","PADU","TJCP",
  "PACV","KLAR","KIMT","KPIH","KHYA","KDBQ","KBRD","KRKS","KBTM","KPGA",
  "KPIR","KMCN","KESC","KATY","KMEI","KRIW","KBID","KSLN","KEAR","KJLN",
  "KSMX","KLBF","PAWG","KJST","KVEL","KCVN","KTUP","TJIG","KINL","KCYS",
  "KPAH","KPIB","KALO","KWST","PAYA","KBFF","KTBN","KHIB","KCDC","PAGA",
  "KALS","KVCT","KDEC","KLEB","KBFM","KTEX","KOWB","KEKO","KFXE","KJMS",
  "KMWA","KBHB","KHYS","KBIH","PAGS","KCEZ","KWYS","KTVF","KLWB","PAUN",
  "KLBL","TJMZ","KDVL","KAPN","KSDY","KSHD","KCEC","KPVC","KCGI","KGLH",
  "KIPL","KRKD","KIWD","KSLE","KSVC","KMGW","KDDC","PAHN","KLNS","KFOD",
  "KMBL","KMSL","KOGS","PAOH","KMCW","PAIL","PACX","KDUJ","KPDT","PFYU",
  "KAUG","KDRT","KRUT","KHHR","KCNM","KMSS","PAGY","KSLK","PASM","KBKW",
  "KUIN","PANI","PAVA","KHRO","KMKG","KPKB","KAIA","PAHP","KIRK","PAVD",
  "KBRL","PAPO","PASK","KMMH","KBFD","KMCE","KSOW","KHOT","PAWN","PHMU",
  "PACM","PASH","PASO","KPUB","PASA","KMKL","KAOO","PAOO","KOLF","KELD",
  "PAGM","PHLU","PAMO","PACD","PAVL","PAKP","PATG","KHVR","KGGW","PASD",
  "PAEE","PAIK","PFKW","PAMC","KPBI","PGRO","KBBG","KBED","KBJC","KCCR",
  "KCDR","KCRQ","KDOV","KDTS","KEWB","KFHR","KFTW","KGCN","KGDV","KGPI",
  "KGUP","KGYY","KHII","KHXD","KIPT","KIWA","KJBR","KJQF","KLAF","KLAL",
  "KLRU","KLUK","KMCK","KMGC","KMWL","KMYL","KNYL","KOGD","KOLM","KOPF",
};
static const int kNumTopAirports = sizeof(kTopAirports) / sizeof(kTopAirports[0]);

bool isTopAirport(const char* icao) {
  if (!icao || icao[0] == 0) return false;
  for (int i = 0; i < kNumTopAirports; i++) {
    if (strcmp(kTopAirports[i], icao) == 0) return true;
  }
  return false;
}

// Fetch route (estDepartureAirport / estArrivalAirport) for the given aircraft
// via OpenSky. Fills g_routeOrigin/g_routeDest (ICAO codes). Cheap-ish but uses
// OpenSky credits, so we cache it (g_routeFetched).
void fetchRoute(const char* icao24) {
  g_routeBusy = true;   // guard g_routeOrigin/g_routeDest while we write them
  g_routeOrigin = "";
  g_routeDest = "";
  // No WiFi is transient; leave g_routeFetched false so we retry once connected.
  if (WiFi.status() != WL_CONNECTED) { g_routeBusy = false; return; }
  // The flights/aircraft endpoint requires begin/end (Unix seconds) to return a
  // flight. The plane is overhead right now, so search the last few hours.
  time_t now = time(nullptr);
  time_t begin = now - 4 * 3600;   // 4 hours back
  time_t end = now + 60;           // a little into the future
  String url = String("https://opensky-network.org/api/flights/aircraft?icao24=") + icao24
               + "&begin=" + String((long)begin) + "&end=" + String((long)end);
  // Verified TLS against the same ISRG roots used by the main OpenSky calls.
  NetworkClientSecure sec;
  HTTPClient http;
  http.setTimeout(5000);
  // Capture the /flights/* bucket's remaining credits for the Credits screen.
  const char* hdrKeys[] = { "X-Rate-Limit-Remaining" };
  http.collectHeaders(hdrKeys, 1);
  String authHdr;
  if (openskyEnsureToken()) authHdr = "Bearer " + g_osToken;
  const char* routeHdrs[] = { "Authorization", authHdr.c_str(), nullptr };
  int code = httpsRequestRetry(http, sec, url.c_str(), HTTPS_METHOD_GET, "", routeHdrs, false);
  String rem = http.header("X-Rate-Limit-Remaining");
  if (rem.length()) g_flightsCredits = rem.toInt();
  bool parsedOk = false;
  if (code == HTTP_CODE_OK) {
    HttpBodyStream body(http);
    BoundedAllocator routeAlloc(2048);
    JsonDocument doc(&routeAlloc);
    int first = body.read();
    while (first == ' ' || first == '\r' || first == '\n' || first == '\t') first = body.read();
    if (first == '[') {
      // The route endpoint returns an array; only its first object is needed.
      if (nextElement(body, doc)) {
        JsonObject flight = doc.as<JsonObject>();
        if (!flight.isNull()) {
          const char* dep = flight["estDepartureAirport"] | "";
          const char* dst = flight["estArrivalAirport"]   | "";
          g_routeOrigin = dep;
          g_routeDest   = dst;
        }
      }
      parsedOk = body.drain();
    } else if (first == 'n') {
      parsedOk = body.drain();  // valid empty/null response
    }
  }
  http.end();
  // Cache a valid response. Also mark as fetched for TLS errors, 429 (out of
  // credits), or truncated/bad framing, so we do not keep retrying and burning
  // credits for the same overhead plane.
  if (code < 0 || code == HTTP_CODE_TOO_MANY_REQUESTS || (code == HTTP_CODE_OK && !parsedOk)) {
    g_routeFetched = true;
    if (code == HTTP_CODE_TOO_MANY_REQUESTS) g_flightsCredits = 0;
  }
  if (code >= 0 && parsedOk) g_routeFetched = true;
  if (code == HTTP_CODE_OK) {
    if (isDevBuild()) Serial.printf("[net] route ok origin=%s dest=%s free=%u\n", g_routeOrigin.c_str(), g_routeDest.c_str(), (unsigned)ESP.getFreeHeap());
  }
  g_routeBusy = false;
}

// Fetch the real ground track (past positions + current bearing) for the given
// aircraft via the OpenSky /tracks endpoint, into g_trackPts (bounded, points
// within ~2x radar range of the observer) and g_trackBearingDeg (the true-track
// bearing of the newest near point, used to dead-reckon the blip along the
// actual path). Also captures the /tracks/* bucket's remaining credits for the
// Credits screen. A failed/empty/no-credit response simply leaves g_trackCount
// = 0, so no track is drawn and dead-reckoning falls back to heading/speed.
void fetchTrack(const char* icao24) {
  g_trackBusy = true;
  portENTER_CRITICAL(&g_trackMux);
  g_trackCount = 0;
  g_trackBearingDeg = -1.0f;
  portEXIT_CRITICAL(&g_trackMux);
  if (WiFi.status() != WL_CONNECTED) { g_trackFetched = false; g_trackBusy = false; return; }
  time_t now = time(nullptr);
  String url = String("https://opensky-network.org/api/tracks/all?icao24=") + icao24
               + "&time=" + String((long)now);
  NetworkClientSecure sec;
  HTTPClient http;
  http.setTimeout(5000);
  const char* hdrKeys[] = { "X-Rate-Limit-Remaining" };
  http.collectHeaders(hdrKeys, 1);
  String authHdr;
  if (openskyEnsureToken()) authHdr = "Bearer " + g_osToken;
  const char* trackHdrs[] = { "Authorization", authHdr.c_str(), nullptr };
  int code = httpsRequestRetry(http, sec, url.c_str(), HTTPS_METHOD_GET, "", trackHdrs, false);
  String rem = http.header("X-Rate-Limit-Remaining");
  if (rem.length()) g_tracksCredits = rem.toInt();
  bool parsedOk = false;
  if (code == HTTP_CODE_OK) {
    HttpBodyStream body(http);
    BoundedAllocator ptAlloc(1024);
    JsonDocument pt(&ptAlloc);
    if (!seekArray(body, "\"path\"")) {
      if (body.peek() == 'n' && body.drain()) {
        parsedOk = true;  // OpenSky uses path:null for a valid empty track.
        if (isDevBuild()) Serial.printf("[net] track path null len=%u\n", (unsigned)body.bytesRead());
      } else {
        if (isDevBuild()) Serial.printf("[net] track json no path len=%u\n", (unsigned)body.bytesRead());
      }
      portENTER_CRITICAL(&g_trackMux);
      g_trackCount = 0;
      portEXIT_CRITICAL(&g_trackMux);
    } else {
      const float maxRange = max(g_radiusMi * 2.0f, 8.0f);
      const float cosLat = cosf(g_lat * PI / 180.0f);
      TrackPoint ring[MAX_TRACK_PTS];
      int written = 0;
      int kept = 0;
      float bearing = -1.0f;
      bool haveBearing = false;
      for (;;) {
        if (!nextElement(body, pt)) break;
        if (!pt.is<JsonArray>() || pt.size() < 5) continue;
        float lat = pt[1].as<float>();
        float lon = pt[2].as<float>();
        float dlat = lat - g_lat;
        float dlon = (lon - g_lon) * cosLat;
        float dxMi = dlon * 69.0f;
        float dyMi = dlat * 69.0f;
        if (sqrtf(dxMi * dxMi + dyMi * dyMi) > maxRange) continue;
        ring[written].dxMi = dxMi;
        ring[written].dyMi = dyMi;
        written = (written + 1) % MAX_TRACK_PTS;
        if (kept < MAX_TRACK_PTS) kept++;
        // Keep the newest near point's true-track as the follow bearing.
        if (!pt[4].isNull()) { bearing = pt[4].as<float>(); haveBearing = true; }
      }
      bool bodyComplete = body.drain();
      if (bodyComplete) {
        int start = (written - kept + MAX_TRACK_PTS) % MAX_TRACK_PTS;
        portENTER_CRITICAL(&g_trackMux);
        for (int i = 0; i < kept; i++) {
          g_trackPts[i] = ring[(start + i) % MAX_TRACK_PTS];
        }
        if (haveBearing) g_trackBearingDeg = bearing;
        g_trackCount = kept;
        portEXIT_CRITICAL(&g_trackMux);
        parsedOk = true;
      } else {
        portENTER_CRITICAL(&g_trackMux);
        g_trackCount = 0;
        portEXIT_CRITICAL(&g_trackMux);
        if (isDevBuild()) Serial.printf("[net] track json short len=%u/%ld%s\n", (unsigned)body.bytesRead(), body.contentLength(), body.stalled() ? " stalled" : "");
        http.end();
        g_trackFetched = true;  // do not retry this plane for a truncated body
        g_trackBusy = false;
        return;
      }
    }
    if (isDevBuild()) Serial.printf("[net] track ok free=%u pts=%d\n", (unsigned)ESP.getFreeHeap(), (int)g_trackCount);
  }
  http.end();
  // Cache a valid response. Also mark as fetched for TLS errors, 429 (out of
  // credits), or truncated/bad framing, so we do not keep retrying and burning
  // credits for the same overhead plane. A pure no-WiFi exit is handled above.
  if (code < 0 || code == HTTP_CODE_TOO_MANY_REQUESTS || (code == HTTP_CODE_OK && !parsedOk)) {
    g_trackFetched = true;
    if (code == HTTP_CODE_TOO_MANY_REQUESTS) g_tracksCredits = 0;
  }
  if (code >= 0 && parsedOk) g_trackFetched = true;
  g_trackBusy = false;
}

