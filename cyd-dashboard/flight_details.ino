// flight_details.ino - origin/destination + flight details view.
//
// When a plane is overhead and the user taps "Details", we look up its route
// (origin/destination airports) via the OpenSky `flights/aircraft` endpoint and
// show a details overlay. Airport ICAO codes are mapped to city names via a
// small embedded table; unknown codes fall back to showing the raw code. The
// route is fetched once per plane and cached, so it only costs credits once.

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
  {"KCLE", "Cleveland"},       {"KRDU", "Raleigh"},     {"KABQ", "Albuquerque"},
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

// Extract the 3-letter ICAO airline code from a callsign (e.g. "AAL1234" -> "AAL").
String airlineCode(const char* callsign) {
  if (!callsign || callsign[0] == 0) return "";
  String c = callsign;
  c.toUpperCase();
  return c.substring(0, 3);
}

// Returns true if the airline is known, filling `name` and `color`.
bool airlineInfo(const char* callsign, String& name, uint16_t& color) {
  String code = airlineCode(callsign);
  for (int i = 0; i < kNumAirlines; i++) {
    if (code == kAirlines[i].code) { name = kAirlines[i].name; color = kAirlines[i].color; return true; }
  }
  return false;
}

// findAirlineLogo() is defined in logos.ino: it loads logos from the dedicated
// LittleFS "logos" partition at runtime (bounded LRU cache, PSRAM-first), and
// is declared at the top of cyd-dashboard.ino.

// Top US airports by 2023 enplanements (ICAO codes), from FAA commercial
// service enplanement data. KDFW is included (it is a major hub); flights to or
// from it additionally get the red/green DFW flash. Used to flash the LED
// (blue) when an overhead flight involves one of these airports.
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
  if (WiFi.status() != WL_CONNECTED) { g_routeFetched = true; g_routeBusy = false; return; }
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
  if (!httpsBegin(http, sec, url.c_str(), kISRGRootCAs)) { g_routeFetched = true; g_routeBusy = false; return; }
  http.setTimeout(5000);
  if (openskyEnsureToken()) http.addHeader("Authorization", "Bearer " + g_osToken);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      JsonArray arr = doc.as<JsonArray>();
      if (arr.size() > 0) {
        const char* dep = arr[0]["estDepartureAirport"] | "";
        const char* dst = arr[0]["estArrivalAirport"]   | "";
        g_routeOrigin = dep;
        g_routeDest   = dst;
      }
    }
  }
  http.end();
  g_routeFetched = true;
  g_routeBusy = false;
}

