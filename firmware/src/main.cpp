// Mijn Woning – ESP32-WROOM-32 datalogger (alleen lezen, stuurt niets aan).
// Leest P1, SMA, Marstek, laadpaal, Daikin, Home Connect, Nordic Fire, Eurom en weer uit,
// maakt 5-minutenwaarden en zet die op GitHub voor de website.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
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
    {"ev", pollEv, IV_EV, 0},
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
static unsigned long lastLiveUpload = 0, lastPendingTry = 0, lastWifiTry = 0;

static void startWifi() {
  String ssid = cfg("wifi_ssid");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  if (ssid.length()) {
    WiFi.begin(ssid.c_str(), cfg("wifi_pass").c_str());
    Serial.printf("Verbinden met %s", ssid.c_str());
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      Serial.print('.');
    }
    Serial.println();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wifi OK, IP %s\n", WiFi.localIP().toString().c_str());
    WiFi.setSleep(false);
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
  esp_task_wdt_init(90, true);  // herstart als iets langer dan 90 s blijft hangen
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
  unsigned long now = millis();

  // Wifi bewaken
  if (!apMode && WiFi.status() != WL_CONNECTED && now - lastWifiTry > 30000) {
    lastWifiTry = now;
    Serial.println("Wifi kwijt, opnieuw verbinden");
    WiFi.reconnect();
  }
  if (apMode && cfgHas("wifi_ssid") && now > 600000) ESP.restart();  // na 10 min opnieuw proberen

  samplerTick();
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
    if (ghPublishLive(err)) markOk(S_GITHUB);
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
