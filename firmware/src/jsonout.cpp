#include "jsonout.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "sampler.h"
#include "settings.h"
#include "sources/sources.h"
#include "state.h"

static void num(JsonVariant v, double x, int decimals = 1) {
  if (isnan(x)) v.set(nullptr);
  else {
    double p = pow(10, decimals);
    v.set(round(x * p) / p);
  }
}

static const char* const MODES[] = {"off", "heat", "cool", "auto", "dry", "fan"};
static const char* const HEATER_STATES[] = {"off", "starting", "on", "cooling", "unknown"};
static const char* const APPL_STATES[] = {"off", "idle", "running", "paused", "finished"};
static const char* const CLIMATE_NAMES[N_CLIMATE] = {"Kamer", "Keuken", "Studio", "Slaapkamer Fedde",
                                                     "Slaapkamer Jurre", "Slaapkamer Remko", "Slaapkamer C&R"};
static const char* const HEATER_NAMES[N_HEATER] = {"Nordic Fire pelletkachel", "Eurom slaapkamer C&R", "Eurom badkamer"};
static const char* const APPL_NAMES[N_APPL] = {"Bosch wasmachine", "Siemens vaatwasser"};

void writeLiveJson(Print& out) {
  JsonDocument d;
  d["ts"] = nowUnix();
  JsonObject g = d["grid"].to<JsonObject>();
  num(g["w"], live.gridW, 0);
  num(g["importKwh"], live.importKwh, 3);
  num(g["exportKwh"], live.exportKwh, 3);
  num(g["gasM3"], live.gasM3, 3);
  num(g["l1"], live.l1, 0);
  num(g["l2"], live.l2, 0);
  num(g["l3"], live.l3, 0);
  num(g["voltage"], live.voltage, 1);

  JsonObject s = d["solar"].to<JsonObject>();
  num(s["w"], live.solarW, 0);
  num(s["totalKwh"], live.solarTotalKwh, 2);
  JsonArray inv = s["inverters"].to<JsonArray>();
  const char* invNames[2] = {"SMA SB3.0", "SMA SB4.0"};
  for (int i = 0; i < 2; i++) {
    JsonObject o = inv.add<JsonObject>();
    o["id"] = i ? "sma2" : "sma1";
    o["name"] = invNames[i];
    num(o["w"], live.smaW[i], 0);
  }

  JsonArray bats = d["batteries"].to<JsonArray>();
  for (int i = 0; i < N_BAT; i++) {
    JsonObject b = bats.add<JsonObject>();
    b["id"] = i ? "bat2" : "bat1";
    b["name"] = i ? "Marstek 2" : "Marstek 1";
    num(b["soc"], live.batSoc[i], 1);
    num(b["w"], live.batW[i], 0);
    b["capacityKwh"] = BAT_CAPACITY_KWH;
  }

  d["ev"] = nullptr;  // geen laadpaal

  JsonArray cl = d["climate"].to<JsonArray>();
  for (int i = 0; i < N_CLIMATE; i++) {
    auto& a = live.ac[i];
    JsonObject c = cl.add<JsonObject>();
    c["id"] = CLIMATE_IDS[i];
    c["name"] = CLIMATE_NAMES[i];
    num(c["temp"], a.temp);
    num(c["target"], a.target);
    c["mode"] = MODES[a.mode < 6 ? a.mode : 0];
    c["on"] = a.mode != 0;
    num(c["kwhToday"], a.kwhToday, 2);
  }

  JsonArray hs = d["heaters"].to<JsonArray>();
  for (int i = 0; i < N_HEATER; i++) {
    auto& h = live.heater[i];
    JsonObject o = hs.add<JsonObject>();
    o["id"] = HEATER_IDS[i];
    o["name"] = HEATER_NAMES[i];
    o["kind"] = HEATER_KINDS[i];
    o["state"] = h.seen ? HEATER_STATES[h.state < 5 ? h.state : 4] : "unknown";
    num(o["temp"], h.temp);
    num(o["target"], h.target);
    if (h.level >= 0) o["level"] = h.level;
    else o["level"] = nullptr;
  }

  JsonArray as = d["appliances"].to<JsonArray>();
  for (int i = 0; i < N_APPL; i++) {
    auto& a = live.appl[i];
    JsonObject o = as.add<JsonObject>();
    o["id"] = APPL_IDS[i];
    o["name"] = APPL_NAMES[i];
    o["kind"] = APPL_KINDS[i];
    o["state"] = a.seen ? APPL_STATES[a.state < 5 ? a.state : 1] : "off";
    if (a.program[0]) o["program"] = a.program;
    else o["program"] = nullptr;
    if (a.remainingMin >= 0) o["remainingMin"] = a.remainingMin;
    else o["remainingMin"] = nullptr;
    if (a.progress >= 0) o["progress"] = a.progress;
    else o["progress"] = nullptr;
  }

  JsonObject w = d["weather"].to<JsonObject>();
  num(w["temp"], live.tOut);
  num(w["cloud"], live.cloud, 0);

  JsonObject h = d["health"].to<JsonObject>();
  JsonObject src = h["sources"].to<JsonObject>();
  for (int i = 0; i < N_SRC; i++) {
    auto& x = live.health[i];
    if (!x.enabled) continue;
    JsonObject o = src[SRC_KEYS[i]].to<JsonObject>();
    o["ok"] = x.ok;
    o["last"] = x.last;
    if (x.err[0]) o["err"] = x.err;
    if (x.limit) {
      o["calls"] = x.calls;
      o["limit"] = x.limit;
    }
  }
  h["rssi"] = WiFi.RSSI();
  h["heap"] = ESP.getFreeHeap();
  h["uptime"] = millis() / 1000;
  h["fw"] = FW_VERSION;
  h["pending"] = pendingCount();
  serializeJson(d, out);
}
