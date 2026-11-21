// Mijn Woning – ESP32-WROOM-32 datalogger (alleen lezen, stuurt niets aan).
// Leest P1, SMA, Marstek, Daikin, Home Connect, Nordic Fire, Eurom en weer uit,
// maakt 5-minutenwaarden en zet die op GitHub voor de website.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "github.h"
#include "portal.h"
#include "sampler.h"
#include "settings.h"
#include "sources/sources.h"
#include "state.h"

struct Task {
  const char* name;
  void (*fn)();
  unsigned long interval;
  unsigned long last;
};

static Task tasks[] = {
    {"p1", pollP1, IV_P1, 0},
    {"sma", pollSma, IV_SMA, 0},
    {"bat", pollBatteries, IV_BAT, 0},
    {"weer", pollWeather, IV_WEATHER, 0},
    {"onecta", pollOnecta, IV_ONECTA, 0},
    {"homeconnect", pollHomeConnect, IV_HC_IDLE, 0},
    {"nordicfire", pollAgua, IV_AGUA, 0},
    {"eurom", pollEurom, IV_EUROM, 0},
};
static const size_t N_TASKS = sizeof(tasks) / sizeof(tasks[0]);

static bool apMode = false;

static const char* resetText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "stroom aangesloten";
    case ESP_RST_BROWNOUT: return "stroomdip (voeding of kabel te zwak)";
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT: return "waakhond (iets bleef hangen)";
    case ESP_RST_PANIC: return "crash in de software";
    case ESP_RST_SW: return "herstart door software/instellingen";
    case ESP_RST_EXT: return "reset-knop";
    default: return "onbekend";
  }
}

// Bij elke start: reden + teller bewaren; het tijdstip wordt ingevuld zodra de klok bekend is.
static void recordBoot() {
  strlcpy(g_lastReset, resetText(esp_reset_reason()), sizeof(g_lastReset));
  Preferences p;
  p.begin("diag", false);
  g_boots = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", g_boots);
  g_resetHist = p.getString("hist", "[]");
  p.end();
  Serial.printf("Start nr. %u, reden: %s\n", g_boots, g_lastReset);
}

static void recordBootTime() {
  static bool done = false;
  if (done || !timeValid()) return;
  done = true;
  JsonDocument d;
  deserializeJson(d, g_resetHist);
  JsonArray a = d.is<JsonArray>() ? d.as<JsonArray>() : d.to<JsonArray>();
  JsonObject o = a.add<JsonObject>();
  o["t"] = nowUnix();
  o["r"] = g_lastReset;
  while (a.size() > 8) a.remove(0);  // laatste 8 bewaren
  g_resetHist = "";
  serializeJson(d, g_resetHist);
  Preferences p;
  p.begin("diag", false);
  p.putString("hist", g_resetHist);
  p.end();
}
static int lastDiscReason = 0;
static String wifiScan;  // zichtbare netwerken (voor de instelpagina)

static const char* reasonText(int r) {
  switch (r) {
    case 0: return "";
    case 201: return "netwerk niet gevonden (verkeerde naam of alleen 5 GHz?)";
    case 15: case 204: case 2: case 202: case 14: case 203:
      return "wachtwoord klopt niet (of router weigert, bv. alleen WPA3)";
    case 205: return "verbinding mislukt";
    case 8: return "afgemeld door de router";
    case 3: case 200: return "router reageert niet (te ver weg?)";
    default: return "verbinding mislukt";
  }
}

// Voor de instelpagina
String wifiDiagnose() {
  if (WiFi.status() == WL_CONNECTED) return "";
  String o;
  const char* t = reasonText(lastDiscReason);
  if (t[0]) o += String("Reden: ") + t + " (code " + lastDiscReason + ").";
  if (wifiScan.length()) o += "<br>Zichtbare netwerken: " + wifiScan;
  return o;
}

static void scanNetworks() {
  int n = WiFi.scanNetworks(false, true);
  wifiScan = "";
  String want = cfg("wifi_ssid");
  bool seen = false;
  for (int i = 0; i < n && i < 15; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) ssid = "(verborgen)";
    const char* enc = "";
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN: enc = "open"; break;
      case WIFI_AUTH_WPA2_PSK: enc = "WPA2"; break;
      case WIFI_AUTH_WPA_WPA2_PSK: enc = "WPA/WPA2"; break;
      case WIFI_AUTH_WPA3_PSK: enc = "WPA3"; break;
      case WIFI_AUTH_WPA2_WPA3_PSK: enc = "WPA2/WPA3"; break;
      default: enc = "anders"; break;
    }
    if (ssid == want) seen = true;
    Serial.printf("  zichtbaar: %-24s kanaal %2d  %4d dBm  %s\n", ssid.c_str(), WiFi.channel(i), WiFi.RSSI(i), enc);
    if (wifiScan.length()) wifiScan += ", ";
    wifiScan += ssid + " (" + enc + ", " + String(WiFi.RSSI(i)) + " dBm)";
  }
  if (want.length())
    Serial.printf("  '%s' %s\n", want.c_str(), seen ? "IS zichtbaar op 2,4 GHz" : "is NIET zichtbaar (alleen 5 GHz? andere naam? hoofdletters?)");
  WiFi.scanDelete();
}
static unsigned long lastLiveUpload = 0, lastPendingTry = 0, lastWifiTry = 0;

// Verbinden met de sterkste zender van dit netwerk (bij mesh-netwerken zijn er meerdere), met herhaalpogingen.
static bool connectWifi(const String& ssid, const String& pass, int tries) {
  for (int t = 0; t < tries; t++) {
    int n = WiFi.scanNetworks(false, false);
    int best = -1;
    for (int i = 0; i < n; i++)
      if (WiFi.SSID(i) == ssid && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
    if (best >= 0) {
      uint8_t bssid[6];
      memcpy(bssid, WiFi.BSSID(best), 6);
      int ch = WiFi.channel(best);
      Serial.printf("Verbinden met '%s' via zender %02X:%02X:%02X:%02X:%02X:%02X (kanaal %d, %d dBm), poging %d",
                    ssid.c_str(), bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], ch, WiFi.RSSI(best), t + 1);
      WiFi.scanDelete();
      WiFi.begin(ssid.c_str(), pass.c_str(), ch, bssid);
    } else {
      WiFi.scanDelete();
      Serial.printf("Verbinden met '%s', poging %d", ssid.c_str(), t + 1);
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      Serial.print('.');
      settingsConsole();
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.printf("  mislukt: %s (code %d)\n", reasonText(lastDiscReason), lastDiscReason);
    WiFi.disconnect();
    delay(400);
  }
  return false;
}

static void startWifi() {
  String ssid = cfg("wifi_ssid");
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) { lastDiscReason = info.wifi_sta_disconnected.reason; },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  WiFi.disconnect(true);
  delay(100);
  if (ssid.length()) connectWifi(ssid, cfg("wifi_pass"), 4);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wifi OK, IP %s\n", WiFi.localIP().toString().c_str());
    WiFi.setSleep(false);  // radio altijd paraat: snelle uploads
    WiFi.setTxPower(WIFI_POWER_15dBm);  // iets minder zendvermogen = kleinere stroompieken (signaal is ruim genoeg)
  } else if (ssid.length()) {
    // Wifi is ingesteld maar lukt (nog) niet: geen instelnetwerk, gewoon blijven proberen (instellen kan via USB).
    Serial.printf("Verbinden met '%s' mislukt: %s (code %d). Opnieuw proberen over 30 s.\n", ssid.c_str(),
                  reasonText(lastDiscReason), lastDiscReason);
    scanNetworks();
  } else {
    // Geen wifi: eigen netwerk opzetten zodat je de instellingen kunt invullen (http://192.168.4.1)
    apMode = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("Instelmodus: verbind met wifi '%s' (wachtwoord %s) en open http://192.168.4.1\n", AP_SSID, AP_PASS);
  }
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(160);  // ruim genoeg; koeler en zuiniger dan 240 MHz
  recordBoot();
  delay(200);
  Serial.printf("\n=== Mijn Woning datalogger %s ===\n", FW_VERSION);
  if (!LittleFS.begin(true)) Serial.println("LittleFS mislukt");
  settingsLoad();
  startWifi();
  configTzTime(TZ_INFO, NTP1, NTP2);
  for (int i = 0; i < 20 && !timeValid(); i++) delay(500);
  Serial.println(timeValid() ? "Tijd gesynchroniseerd" : "Tijd nog niet bekend");
  samplerBegin();
  portalBegin();
  esp_task_wdt_init(120, true);  // herstart als iets langer dan 2 minuten blijft hangen
  esp_task_wdt_add(NULL);
  // Bronnen gespreid laten starten
  unsigned long now = millis();
  for (size_t i = 0; i < N_TASKS; i++) tasks[i].last = now - tasks[i].interval + 2000 + i * 3000;
  lastLiveUpload = now - IV_LIVE_UPLOAD + 60000;
  lastPendingTry = now - IV_RETRY_DAYS + 90000;
}

void loop() {
  esp_task_wdt_reset();
  portalLoop();
  settingsConsole();
  unsigned long now = millis();

  // Wifi bewaken
  if (!apMode && WiFi.status() != WL_CONNECTED && now - lastWifiTry > 30000) {
    lastWifiTry = now;
    Serial.println("Wifi kwijt, opnieuw verbinden");
    connectWifi(cfg("wifi_ssid"), cfg("wifi_pass"), 1);
  }
  if (apMode && cfgHas("wifi_ssid") && now - lastWifiTry > 60000) {
    lastWifiTry = now;
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Wifi alsnog verbonden, IP %s. Instelnetwerk gaat uit.\n", WiFi.localIP().toString().c_str());
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apMode = false;
    } else {
      Serial.println("Opnieuw proberen te verbinden met de wifi...");
      if (connectWifi(cfg("wifi_ssid"), cfg("wifi_pass"), 1)) {
        Serial.printf("Wifi verbonden, IP %s. Instelnetwerk gaat uit.\n", WiFi.localIP().toString().c_str());
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        apMode = false;
      }
    }
  }

  samplerTick();
  recordBootTime();
  if (!timeValid() || WiFi.status() != WL_CONNECTED) {
    delay(20);
    return;
  }

  // Home Connect vaker als er iets draait
  for (auto& t : tasks)
    if (t.fn == pollHomeConnect) t.interval = hcAnyRunning() ? IV_HC_RUN : IV_HC_IDLE;

  // Eén bron per rondje, zodat de instelpagina bereikbaar blijft
  for (auto& t : tasks) {
    if (now - t.last >= t.interval) {
      t.last = now;
      t.fn();
      samplerTick();
      return;
    }
  }

  // Uploads
  if (now - lastLiveUpload >= IV_LIVE_UPLOAD || portalWantsUpload()) {
    lastLiveUpload = now;
    String err;
    unsigned long t0 = millis();
    bool ok = ghPublishLive(err);
    Serial.printf("[github] live %s in %lu ms (vrij geheugen %u)\n", ok ? "ok" : err.c_str(), millis() - t0, ESP.getFreeHeap());
    if (ok) markOk(S_GITHUB);
    else markErr(S_GITHUB, err);
    return;
  }
  if (pendingCount() > 0 && now - lastPendingTry >= IV_RETRY_DAYS) {
    lastPendingTry = now;
    String err;
    if (!ghPublishPending(err)) markErr(S_GITHUB, "dagen: " + err);
    else if (pendingCount() > 0) lastPendingTry = now - IV_RETRY_DAYS + 20000;  // snel door met de rest
    return;
  }
  delay(10);
}
