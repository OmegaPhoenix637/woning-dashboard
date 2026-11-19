// Cloudbronnen: Daikin Onecta, Home Connect, Micronova Agua IOT (Nordic Fire), Tuya (Eurom), Open-Meteo.
#include <mbedtls/md.h>
#include "../net.h"
#include "../sampler.h"
#include "../settings.h"
#include "../state.h"
#include "sources.h"

static inline float fnum(JsonVariantConst v) { return v.is<float>() ? v.as<float>() : NAN; }

// =====================================================================================
// Daikin Onecta (officiële cloud-API, OAuth2). Limiet 200 calls/dag → elke 10 min.
// =====================================================================================
static const char* ON_IDP = "https://idp.onecta.daikineurope.com/v1/oidc";
static const char* ON_API = "https://api.onecta.daikineurope.com/v1";
static String onAccess;
static uint32_t onExpires = 0;

String onectaAuthUrl() {
  return String(ON_IDP) + "/authorize?response_type=code&client_id=" + urlEncode(cfg("on_cid")) +
         "&redirect_uri=" + urlEncode(cfg("on_redirect")) + "&scope=" + urlEncode("openid onecta:basic.integration");
}

static bool onectaToken(const String& form, String& err) {
  JsonDocument doc;
  int code = httpJson("POST", String(ON_IDP) + "/token", form, doc, nullptr,
                      {{"Content-Type", "application/x-www-form-urlencoded"}}, &err);
  if (code != 200 || !doc["access_token"].is<const char*>()) return false;
  onAccess = doc["access_token"].as<String>();
  onExpires = millis() / 1000 + (doc["expires_in"] | 3600) - 60;
  if (doc["refresh_token"].is<const char*>()) cfgSet("on_refresh", doc["refresh_token"].as<String>());  // roteert
  return true;
}

bool onectaExchangeCode(const String& code, String& err) {
  String form = "grant_type=authorization_code&code=" + urlEncode(code) + "&redirect_uri=" + urlEncode(cfg("on_redirect")) +
                "&client_id=" + urlEncode(cfg("on_cid")) + "&client_secret=" + urlEncode(cfg("on_secret"));
  return onectaToken(form, err);
}

static bool onectaEnsureToken(String& err) {
  if (onAccess.length() && millis() / 1000 < onExpires) return true;
  if (!cfgHas("on_refresh")) {
    err = "nog niet gekoppeld";
    return false;
  }
  String form = "grant_type=refresh_token&refresh_token=" + urlEncode(cfg("on_refresh")) + "&client_id=" + urlEncode(cfg("on_cid")) +
                "&client_secret=" + urlEncode(cfg("on_secret"));
  return onectaToken(form, err);
}

static uint8_t onectaMode(const char* m, bool on) {
  if (!on || !m) return 0;
  if (!strcmp(m, "heating")) return 1;
  if (!strcmp(m, "cooling")) return 2;
  if (!strcmp(m, "auto")) return 3;
  if (!strcmp(m, "dry")) return 4;
  if (!strcmp(m, "fanOnly")) return 5;
  return 3;
}

// Som van vandaag uit een 'd'-reeks (24 waarden in blokken van 2 uur: eerst gisteren, dan vandaag).
static float sumToday(JsonVariantConst d) {
  if (!d.is<JsonArrayConst>()) return 0;
  JsonArrayConst a = d.as<JsonArrayConst>();
  float s = 0;
  size_t i = 0;
  for (JsonVariantConst v : a) {
    if (i >= 12 && v.is<float>()) s += v.as<float>();
    i++;
  }
  return s;
}

void pollOnecta() {
  if (!cfgHas("on_cid")) return;
  String err;
  if (!onectaEnsureToken(err)) return markErr(S_ONECTA, err);

  // Alleen de velden die we nodig hebben bewaren (antwoord is groot)
  JsonDocument filter;
  JsonObject mp = filter[0]["managementPoints"][0].to<JsonObject>();
  mp["managementPointType"] = true;
  mp["name"]["value"] = true;
  mp["onOffMode"]["value"] = true;
  mp["operationMode"]["value"] = true;
  mp["sensoryData"]["value"]["roomTemperature"]["value"] = true;
  mp["sensoryData"]["value"]["outdoorTemperature"]["value"] = true;
  mp["temperatureControl"]["value"]["operationModes"] = true;
  mp["consumptionData"]["value"]["electrical"]["heating"]["d"] = true;
  mp["consumptionData"]["value"]["electrical"]["cooling"]["d"] = true;

  JsonDocument doc;
  String remaining;
  int code = httpJson("GET", String(ON_API) + "/gateway-devices", "", doc, &filter, {{"Authorization", "Bearer " + onAccess}},
                      &err, 30000, "X-RateLimit-Remaining-day", &remaining);
  live.health[S_ONECTA].limit = 200;
  if (remaining.length()) live.health[S_ONECTA].calls = 200 - remaining.toInt();
  else live.health[S_ONECTA].calls++;
  if (code == 401) onAccess = "";
  if (code != 200) return markErr(S_ONECTA, err);

  const char* nameKeys[N_CLIMATE] = {"on_n0", "on_n1", "on_n2", "on_n3", "on_n4", "on_n5", "on_n6"};
  int matched = 0;
  for (JsonVariantConst dev : doc.as<JsonArrayConst>()) {
    for (JsonVariantConst p : dev["managementPoints"].as<JsonArrayConst>()) {
      if (strcmp(p["managementPointType"] | "", "climateControl") != 0) continue;
      String name = p["name"]["value"] | "";
      int idx = -1;
      for (int i = 0; i < N_CLIMATE; i++)
        if (name.equalsIgnoreCase(cfg(nameKeys[i]))) idx = i;
      if (idx < 0) {
        Serial.printf("[onecta] onbekende unit '%s' (vul de naam in op de instelpagina)\n", name.c_str());
        continue;
      }
      auto& a = live.ac[idx];
      bool on = strcmp(p["onOffMode"]["value"] | "off", "on") == 0;
      const char* opm = p["operationMode"]["value"] | "";
      a.seen = true;
      a.mode = onectaMode(opm, on);
      a.temp = fnum(p["sensoryData"]["value"]["roomTemperature"]["value"]);
      a.target = on ? fnum(p["temperatureControl"]["value"]["operationModes"][opm]["setpoints"]["roomTemperature"]["value"]) : NAN;
      float out = fnum(p["sensoryData"]["value"]["outdoorTemperature"]["value"]);
      if (!isnan(out) && isnan(live.tOut)) live.tOut = out;  // reserve als het weer (nog) niet bekend is
      JsonVariantConst el = p["consumptionData"]["value"]["electrical"];
      a.kwhToday = sumToday(el["heating"]["d"]) + sumToday(el["cooling"]["d"]);
      samplerAdd(CH_AC_BASE + idx * 3, a.temp);
      samplerAdd(CH_AC_BASE + idx * 3 + 1, a.target);
      samplerAdd(CH_AC_BASE + idx * 3 + 2, a.mode);
      samplerSetClimateKwh(idx, a.kwhToday);
      matched++;
    }
  }
  if (!matched) return markErr(S_ONECTA, "geen airco's herkend (controleer de namen)");
  markOk(S_ONECTA);
}

// =====================================================================================
// Home Connect (Bosch / Siemens), OAuth2 device flow. Limiet 1000 calls/dag.
// =====================================================================================
static const char* HC = "https://api.home-connect.com";
static String hcAccess, hcDeviceCode;
static uint32_t hcExpires = 0;
static bool hcRunning[N_APPL] = {false, false};

static bool hcToken(const String& form, String& err) {
  JsonDocument doc;
  int code = httpJson("POST", String(HC) + "/security/oauth/token", form, doc, nullptr,
                      {{"Content-Type", "application/x-www-form-urlencoded"}}, &err);
  live.health[S_HC].calls++;
  if (code != 200 || !doc["access_token"].is<const char*>()) {
    if (doc["error"].is<const char*>()) err = doc["error"].as<String>();
    return false;
  }
  hcAccess = doc["access_token"].as<String>();
  hcExpires = millis() / 1000 + (doc["expires_in"] | 86400) - 120;
  if (doc["refresh_token"].is<const char*>()) cfgSet("hc_refresh", doc["refresh_token"].as<String>());
  return true;
}

bool hcDeviceStart(String& userCode, String& verifyUrl, String& err) {
  JsonDocument doc;
  String form = "client_id=" + urlEncode(cfg("hc_cid")) + "&scope=" + urlEncode("IdentifyAppliance Monitor");
  int code = httpJson("POST", String(HC) + "/security/oauth/device_authorization", form, doc, nullptr,
                      {{"Content-Type", "application/x-www-form-urlencoded"}}, &err);
  if (code != 200) return false;
  hcDeviceCode = doc["device_code"] | "";
  userCode = doc["user_code"] | "";
  verifyUrl = doc["verification_uri_complete"] | (doc["verification_uri"] | "");
  return hcDeviceCode.length() > 0;
}

int hcDevicePoll(String& err) {
  if (!hcDeviceCode.length()) {
    err = "eerst koppelen starten";
    return -1;
  }
  String form = "grant_type=device_code&device_code=" + urlEncode(hcDeviceCode) + "&client_id=" + urlEncode(cfg("hc_cid"));
  if (cfgHas("hc_secret")) form += "&client_secret=" + urlEncode(cfg("hc_secret"));
  if (hcToken(form, err)) {
    hcDeviceCode = "";
    return 1;
  }
  return (err == "authorization_pending" || err == "slow_down") ? 0 : -1;
}

static bool hcEnsureToken(String& err) {
  if (hcAccess.length() && millis() / 1000 < hcExpires) return true;
  if (!cfgHas("hc_refresh")) {
    err = "nog niet gekoppeld";
    return false;
  }
  String form = "grant_type=refresh_token&refresh_token=" + urlEncode(cfg("hc_refresh"));
  if (cfgHas("hc_secret")) form += "&client_secret=" + urlEncode(cfg("hc_secret"));
  return hcToken(form, err);
}

static int hcGet(const String& path, JsonDocument& doc, String& err) {
  live.health[S_HC].calls++;
  return httpJson("GET", String(HC) + path, "", doc, nullptr,
                  {{"Authorization", "Bearer " + hcAccess}, {"Accept", "application/vnd.bsh.sdk.v1+json"}, {"Accept-Language", "nl-NL"}},
                  &err);
}

// Zoek eenmalig de wasmachine en vaatwasser op.
static bool hcDiscover(String& err) {
  if (cfgHas("hc_washer") && cfgHas("hc_dish")) return true;
  JsonDocument doc;
  if (hcGet("/api/homeappliances", doc, err) != 200) return false;
  for (JsonVariantConst a : doc["data"]["homeappliances"].as<JsonArrayConst>()) {
    const char* type = a["type"] | "";
    String haId = a["haId"] | "";
    if (!strcmp(type, "Washer") || !strcmp(type, "WasherDryer")) cfgSet("hc_washer", haId);
    if (!strcmp(type, "Dishwasher")) cfgSet("hc_dish", haId);
  }
  return true;
}

static uint8_t hcState(const char* op) {
  String s = op ? op : "";
  if (s.endsWith(".Run") || s.endsWith(".DelayedStart")) return 2;
  if (s.endsWith(".Pause") || s.endsWith(".ActionRequired")) return 3;
  if (s.endsWith(".Finished")) return 4;
  if (s.endsWith(".Inactive") || s.endsWith(".Off")) return 0;
  return 1;  // Ready / overig
}

bool hcAnyRunning() { return hcRunning[0] || hcRunning[1]; }

void pollHomeConnect() {
  if (!cfgHas("hc_cid")) return;
  live.health[S_HC].limit = 1000;
  String err;
  if (!hcEnsureToken(err) || !hcDiscover(err)) return markErr(S_HC, err);
  const char* keys[N_APPL] = {"hc_washer", "hc_dish"};
  bool anyOk = false;
  for (int i = 0; i < N_APPL; i++) {
    String haId = cfg(keys[i]);
    if (!haId.length()) continue;
    JsonDocument doc;
    int code = hcGet("/api/homeappliances/" + haId + "/status", doc, err);
    auto& ap = live.appl[i];
    if (code == 409) {  // apparaat offline / uit
      ap.seen = true;
      ap.state = 0;
      anyOk = true;
      hcRunning[i] = false;
      continue;
    }
    if (code == 401) hcAccess = "";
    if (code != 200) continue;
    const char* op = nullptr;
    for (JsonVariantConst st : doc["data"]["status"].as<JsonArrayConst>())
      if (!strcmp(st["key"] | "", "BSH.Common.Status.OperationState")) op = st["value"];
    uint8_t newState = hcState(op);
    bool wasRunning = ap.state == 2;
    ap.seen = true;
    ap.state = newState;
    ap.remainingMin = -1;
    ap.progress = -1;
    if (newState == 2 || newState == 3) {
      JsonDocument prog;
      if (hcGet("/api/homeappliances/" + haId + "/programs/active", prog, err) == 200) {
        strlcpy(ap.program, prog["data"]["name"] | (prog["data"]["key"] | ""), sizeof(ap.program));
        for (JsonVariantConst o : prog["data"]["options"].as<JsonArrayConst>()) {
          const char* k = o["key"] | "";
          if (!strcmp(k, "BSH.Common.Option.RemainingProgramTime")) ap.remainingMin = (o["value"] | 0) / 60;
          if (!strcmp(k, "BSH.Common.Option.ProgramProgress")) ap.progress = o["value"] | 0;
        }
      }
    }
    if (newState == 2 && !wasRunning) samplerRunStart(i, ap.program);
    if (newState != 2 && newState != 3 && hcRunning[i]) samplerRunEnd(i);
    hcRunning[i] = newState == 2 || newState == 3;
    anyOk = true;
  }
  if (anyOk) markOk(S_HC);
  else markErr(S_HC, err.length() ? err : "geen apparaten");
}

// =====================================================================================
// Nordic Fire 2.0 via Micronova Agua IOT (niet-officieel, experimenteel)
// Gebaseerd op de werking van de open-source 'py-agua-iot' bibliotheek.
// =====================================================================================
static String agToken, agUuid, agDevice, agProduct;
struct AgReg {
  char key[20];
  uint16_t offset;
  uint16_t mask;
  char formula[16];
};
static AgReg agRegs[6];
static int agRegN = 0;

static int agPost(const String& path, const String& body, JsonDocument& doc, String& err, const JsonDocument* filter = nullptr) {
  String base = cfg("ag_url");
  return httpJson("POST", base + path, body, doc, filter,
                  {{"Content-Type", "application/json"},
                   {"Origin", "file://"},
                   {"id_brand", cfg("ag_brand")},
                   {"customer_code", cfg("ag_cust")},
                   {"local", "true"},
                   {"Authorization", agToken.length() ? agToken : agUuid}},
                  &err);
}

static float agFormula(const char* f, float x) {
  // Ondersteunt "#", "#/2", "#*0.1", "#-50", "#+1"
  if (!f || !f[0] || !strcmp(f, "#")) return x;
  const char* p = strchr(f, '#');
  if (!p || !p[1]) return x;
  float v = atof(p + 2);
  switch (p[1]) {
    case '/': return v ? x / v : x;
    case '*': return x * v;
    case '-': return x - v;
    case '+': return x + v;
  }
  return x;
}

static bool agSetup(String& err) {
  if (agToken.length() && agDevice.length() && agRegN) return true;
  if (!agUuid.length()) {
    char u[37];
    snprintf(u, sizeof(u), "%08lx-%04x-4%03x-a%03x-%012llx", (unsigned long)esp_random(), (unsigned)(esp_random() & 0xFFFF),
             (unsigned)(esp_random() & 0xFFF), (unsigned)(esp_random() & 0xFFF), (unsigned long long)ESP.getEfuseMac());
    agUuid = u;
    JsonDocument d;
    agPost("/appSignup",
           "{\"phone_type\":\"Android\",\"phone_id\":\"" + agUuid + "\",\"phone_version\":\"1.0\",\"language\":\"nl\",\"id_app\":\"" +
               agUuid + "\",\"push_notification_token\":\"" + agUuid + "\",\"push_notification_active\":false}",
           d, err);
  }
  JsonDocument doc;
  JsonDocument body;
  body["email"] = cfg("ag_email");
  body["password"] = cfg("ag_pass");
  String b;
  serializeJson(body, b);
  agToken = "";
  if (agPost("/userLogin", b, doc, err) != 200 || !doc["token"].is<const char*>()) return false;
  agToken = doc["token"].as<String>();
  if (agPost("/deviceList", "{}", doc, err) != 200) return false;
  JsonVariantConst dev = doc["device"][0];
  agDevice = dev["id_device"].as<String>();
  agProduct = dev["id_product"].as<String>();
  if (!agDevice.length()) {
    err = "geen kachel gevonden";
    return false;
  }
  JsonDocument filter;
  JsonObject r = filter["device_registers_map"]["registers_map"][0]["registers"][0].to<JsonObject>();
  r["reg_key"] = true;
  r["offset"] = true;
  r["mask"] = true;
  r["formula"] = true;
  r["reg_type"] = true;
  if (agPost("/deviceGetRegistersMap",
             "{\"id_device\":\"" + agDevice + "\",\"id_product\":\"" + agProduct + "\",\"last_update\":\"2018-06-03T08:59:54.043\"}",
             doc, err, &filter) != 200)
    return false;
  const char* want[] = {"status_get", "temp_air_get", "temp_air_set", "power_set", "real_power_get"};
  agRegN = 0;
  for (JsonVariantConst m : doc["device_registers_map"]["registers_map"].as<JsonArrayConst>())
    for (JsonVariantConst reg : m["registers"].as<JsonArrayConst>()) {
      const char* k = reg["reg_key"] | "";
      for (const char* w : want)
        if (!strcmp(k, w) && agRegN < 6) {
          AgReg& a = agRegs[agRegN++];
          strlcpy(a.key, k, sizeof(a.key));
          a.offset = reg["offset"] | 0;
          a.mask = reg["mask"] | 0xFFFF;
          strlcpy(a.formula, reg["formula"] | "#", sizeof(a.formula));
        }
    }
  if (!agRegN) {
    err = "registers niet gevonden";
    return false;
  }
  return true;
}

void pollAgua() {
  if (!cfgHas("ag_email") || !cfgHas("ag_cust")) return;
  String err;
  if (!agSetup(err)) {
    agToken = "";
    return markErr(S_AGUA, err);
  }
  JsonDocument doc;
  if (agPost("/deviceGetBufferReading", "{\"id_device\":\"" + agDevice + "\",\"id_product\":\"" + agProduct + "\",\"BufferId\":1}",
             doc, err) != 200) {
    agToken = "";
    return markErr(S_AGUA, err);
  }
  String job = doc["idRequest"] | "";
  JsonDocument res;
  bool done = false;
  for (int t = 0; t < 8 && !done; t++) {
    delay(1200);
    int code = httpJson("GET", cfg("ag_url") + "/deviceJobStatus/" + job, "", res, nullptr,
                        {{"Origin", "file://"}, {"id_brand", cfg("ag_brand")}, {"customer_code", cfg("ag_cust")}, {"local", "true"},
                         {"Authorization", agToken}},
                        &err);
    done = code == 200 && strcmp(res["jobAnswerStatus"] | "", "completed") == 0;
  }
  if (!done) return markErr(S_AGUA, "geen antwoord van kachel");
  JsonArrayConst items = res["jobAnswerData"]["Items"];
  JsonArrayConst vals = res["jobAnswerData"]["Values"];
  auto& h = live.heater[0];
  float status = NAN;
  for (int r = 0; r < agRegN; r++) {
    for (size_t j = 0; j < items.size(); j++) {
      if ((items[j] | -1) != agRegs[r].offset) continue;
      float v = agFormula(agRegs[r].formula, (float)((vals[j] | 0) & agRegs[r].mask));
      if (!strcmp(agRegs[r].key, "status_get")) status = v;
      else if (!strcmp(agRegs[r].key, "temp_air_get")) h.temp = v;
      else if (!strcmp(agRegs[r].key, "temp_air_set")) h.target = v;
      else if (!strcmp(agRegs[r].key, "power_set") || !strcmp(agRegs[r].key, "real_power_get")) h.level = (int8_t)v;
    }
  }
  // Micronova-status: 0 uit, 1-3 opstarten/ontsteken, 4-5 aan, 6+ afkoelen/reinigen
  h.seen = true;
  h.state = isnan(status) ? 4 : status <= 0 ? 0 : status <= 3 ? 1 : status <= 5 ? 2 : 3;
  samplerAdd(CH_PELLET_T, h.temp);
  samplerAdd(CH_PELLET_ON, h.state == 2 || h.state == 1 ? (h.level > 0 ? h.level : 1) : 0);
  markOk(S_AGUA);
}

// =====================================================================================
// Eurom via Tuya Cloud OpenAPI (alleen als de Eurom-kachels via Tuya werken; experimenteel)
// =====================================================================================
static String tyToken;
static uint32_t tyExpires = 0;

static String hexOf(const uint8_t* b, size_t n, bool upper) {
  String s;
  const char* hx = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  for (size_t i = 0; i < n; i++) s += hx[b[i] >> 4], s += hx[b[i] & 15];
  return s;
}
static String sha256Hex(const String& in) {
  uint8_t out[32];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t*)in.c_str(), in.length(), out);
  return hexOf(out, 32, false);
}
static String hmacUpper(const String& key, const String& msg) {
  uint8_t out[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t*)key.c_str(), key.length(),
                  (const uint8_t*)msg.c_str(), msg.length(), out);
  return hexOf(out, 32, true);
}

static int tyGet(const String& path, JsonDocument& doc, String& err, bool withToken) {
  String host = "https://openapi.tuya" + cfg("ty_region") + ".com";
  String t = String((uint64_t)nowUnix() * 1000ULL);
  String cid = cfg("ty_cid");
  String stringToSign = "GET\n" + sha256Hex("") + "\n\n" + path;
  String sign = hmacUpper(cfg("ty_secret"), cid + (withToken ? tyToken : String()) + t + stringToSign);
  if (withToken)
    return httpJson("GET", host + path, "", doc, nullptr,
                    {{"client_id", cid}, {"sign", sign}, {"t", t}, {"sign_method", "HMAC-SHA256"}, {"access_token", tyToken}}, &err);
  return httpJson("GET", host + path, "", doc, nullptr, {{"client_id", cid}, {"sign", sign}, {"t", t}, {"sign_method", "HMAC-SHA256"}},
                  &err);
}

void pollEurom() {
  if (!cfgHas("ty_cid") || !cfgHas("ty_secret")) return;
  String err;
  JsonDocument doc;
  if (!tyToken.length() || nowUnix() > tyExpires) {
    if (tyGet("/v1.0/token?grant_type=1", doc, err, false) != 200 || !(doc["success"] | false))
      return markErr(S_EUROM, doc["msg"] | err.c_str());
    tyToken = doc["result"]["access_token"].as<String>();
    tyExpires = nowUnix() + (doc["result"]["expire_time"] | 3600) - 60;
  }
  const char* devKeys[2] = {"ty_dev1", "ty_dev2"};
  const int heaterIdx[2] = {1, 2};
  const int chOn[2] = {CH_EUROM_SLAAP_ON, CH_EUROM_BAD_ON};
  const int chT[2] = {CH_EUROM_SLAAP_T, CH_EUROM_BAD_T};
  bool any = false;
  for (int i = 0; i < 2; i++) {
    String dev = cfg(devKeys[i]);
    if (!dev.length()) continue;
    if (tyGet("/v1.0/devices/" + dev + "/status", doc, err, true) != 200 || !(doc["success"] | false)) {
      if ((doc["code"] | 0) == 1010) tyToken = "";  // token verlopen
      continue;
    }
    auto& h = live.heater[heaterIdx[i]];
    bool on = false;
    for (JsonVariantConst s : doc["result"].as<JsonArrayConst>()) {
      String code = s["code"] | "";
      if (code.startsWith("switch") && s["value"].is<bool>()) on = on || s["value"].as<bool>();
      if (code == "temp_current" || code == "temp_current_f") h.temp = fnum(s["value"]);
      if (code == "temp_set") h.target = fnum(s["value"]);
    }
    if (!isnan(h.temp) && h.temp > 100) h.temp /= 10;  // sommige apparaten geven tienden
    if (!isnan(h.target) && h.target > 100) h.target /= 10;
    h.seen = true;
    h.state = on ? 2 : 0;
    samplerAdd(chOn[i], on ? 1 : 0);
    samplerAdd(chT[i], h.temp);
    any = true;
  }
  if (any) markOk(S_EUROM);
  else markErr(S_EUROM, err.length() ? err : "geen status");
}

// =====================================================================================
// Weer via Open-Meteo (gratis, geen sleutel)
// =====================================================================================
void pollWeather() {
  if (!cfgHas("lat") || !cfgHas("lon")) return;
  JsonDocument doc;
  String err;
  String lat = cfg("lat"), lon = cfg("lon");
  lat.replace(',', '.');  // 50,90 -> 50.90 (een komma betekent voor Open-Meteo 'meerdere locaties')
  lon.replace(',', '.');
  lat.trim();
  lon.trim();
  int code = httpJson("GET",
                      "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon +
                          "&current=temperature_2m,cloud_cover&timezone=Europe%2FAmsterdam",
                      "", doc, nullptr, {}, &err);
  if (code != 200) return markErr(S_WEATHER, err);
  if (!doc["current"].is<JsonObjectConst>()) return markErr(S_WEATHER, "geen weerbericht (coördinaten controleren)");
  live.tOut = fnum(doc["current"]["temperature_2m"]);
  live.cloud = fnum(doc["current"]["cloud_cover"]);
  samplerAdd(CH_T_OUT, live.tOut);
  samplerAdd(CH_CLOUD, live.cloud);
  markOk(S_WEATHER);
}

void resetDailyCalls() {
  for (int i = 0; i < N_SRC; i++) live.health[i].calls = 0;
}
