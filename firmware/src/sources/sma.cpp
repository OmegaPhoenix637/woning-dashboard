// Zonnepanelen: 2x SMA Sunny Boy (SB3.0-1AV-40 + SB4.0-1AV-40). Vier manieren (instelling sma_mode):
//  0 = lokaal inloggen als 'Gebruiker' op de webinterface van de omvormer (geen installateur nodig)
//  1 = Modbus TCP (moet door installateur aangezet worden)
//  2 = SMA-cloud via de officiële SMA Monitoring API (developer.sma.de, experimenteel)
//  3 = HomeWizard kWh-meter op de groep van de zonnepanelen
#include "../net.h"
#include "../sampler.h"
#include "../settings.h"
#include "../state.h"
#include "sources.h"

static inline float fnum(JsonVariantConst v) { return v.is<float>() ? v.as<float>() : NAN; }

// Ruwe schatting van zonsopkomst/-ondergang voor Nederland (genoeg om 'slapen' van een fout te onderscheiden).
static bool isNight() {
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  float season = cosf(2 * PI * (tm.tm_yday + 1 - 172) / 365.0f);
  float dayLen = 12.2f + 4.4f * season;
  float noon = 12.67f + (tm.tm_isdst > 0 ? 1 : 0);
  float h = tm.tm_hour + tm.tm_min / 60.0f;
  return h < noon - dayLen / 2 - 0.3f || h > noon + dayLen / 2 + 0.3f;
}

static void publish(bool anyErr, const String& lastErr) {
  float s = 0;
  double tot = 0;
  bool okW = false, okT = true, anyT = false;
  for (int i = 0; i < 2; i++) {
    if (!isnan(live.smaW[i])) s += live.smaW[i], okW = true;
    if (!isnan(live.smaTotalKwh[i])) tot += live.smaTotalKwh[i], anyT = true;
  }
  if (cfg("sma1_ip").length() && isnan(live.smaTotalKwh[0])) okT = false;
  if (cfg("sma2_ip").length() && isnan(live.smaTotalKwh[1])) okT = false;
  live.solarW = okW ? s : NAN;
  live.solarTotalKwh = (okT && anyT) ? tot : NAN;
  samplerAdd(CH_SOLAR_W, live.solarW);
  if (anyErr) markErr(S_SMA, lastErr);
  else if (okW) markOk(S_SMA);
}

// ---------------- 0: lokaal, webinterface (zoals de Home Assistant SMA-integratie) ----------------
// Keys: 6100_40263F00 = AC-vermogen (W), 6400_00260100 = totale opbrengst (Wh)
static String sid[2];
static String scheme[2] = {"http://", "http://"};  // jullie omvormers draaien op http; https als reserve

static bool webLogin(int i, const String& ip, String& err) {
  String pass = (i == 1 && cfgHas("sma_pass2")) ? cfg("sma_pass2") : cfg("sma_pass");
  if (!pass.length()) {
    err = "wachtwoord 'Gebruiker' nog niet ingevuld";
    return false;
  }
  JsonDocument body;
  body["right"] = "usr";
  body["pass"] = pass;
  String b;
  serializeJson(body, b);
  JsonDocument doc;
  int code = httpJson("POST", scheme[i] + ip + "/dyn/login.json", b, doc, nullptr, {{"Content-Type", "application/json"}}, &err, 8000);
  if (code < 0) {  // geen verbinding: probeer het andere protocol
    scheme[i] = scheme[i] == "http://" ? "https://" : "http://";
    code = httpJson("POST", scheme[i] + ip + "/dyn/login.json", b, doc, nullptr, {{"Content-Type", "application/json"}}, &err, 8000);
  }
  if (code != 200) return false;
  if (doc["result"]["sid"].is<const char*>()) {
    sid[i] = doc["result"]["sid"].as<String>();
    return true;
  }
  int e = doc["err"] | 0;
  err = e == 401 ? "wachtwoord klopt niet" : e == 503 ? "te veel sessies, probeer over 10 min opnieuw" : "inloggen mislukt (" + String(e) + ")";
  return false;
}

static bool webRead(int i, const String& ip, String& err) {
  if (!sid[i].length() && !webLogin(i, ip, err)) return false;
  for (int attempt = 0; attempt < 2; attempt++) {
    JsonDocument doc;
    int code = httpJson("POST", scheme[i] + ip + "/dyn/getValues.json?sid=" + sid[i],
                        "{\"destDev\":[],\"keys\":[\"6100_40263F00\",\"6400_00260100\"]}", doc, nullptr,
                        {{"Content-Type", "application/json"}}, &err, 10000);
    if (code == 200 && doc["result"].is<JsonObjectConst>()) {
      for (JsonPairConst dev : doc["result"].as<JsonObjectConst>()) {
        float w = fnum(dev.value()["6100_40263F00"]["1"][0]["val"]);
        float wh = fnum(dev.value()["6400_00260100"]["1"][0]["val"]);
        live.smaW[i] = isnan(w) ? 0 : max(0.0f, w);  // 's nachts geeft hij null
        if (!isnan(wh)) live.smaTotalKwh[i] = wh / 1000.0;
        return true;
      }
    }
    // Sessie verlopen → opnieuw inloggen
    sid[i] = "";
    if (attempt == 0 && !webLogin(i, ip, err)) return false;
  }
  if (!err.length()) err = "geen waarden ontvangen";
  return false;
}

// ---------------- 1: Modbus TCP ----------------
static bool modbusReadSma(int i, const String& ip, String& err) {
  uint8_t unit = cfgInt("sma_unit", 3);
  ModbusConn mb;
  if (!mb.open(ip, 502, err)) return false;
  uint16_t r[2];
  if (!mb.read(unit, 3, 30775, 2, r, err)) return false;
  int32_t w = mbS32(r);
  live.smaW[i] = (w == (int32_t)0x80000000 || w < 0) ? 0 : (float)w;
  if (mb.read(unit, 3, 30529, 2, r, err)) {
    uint32_t wh = mbU32(r);
    if (wh != 0xFFFFFFFF && wh != 0x80000000) live.smaTotalKwh[i] = wh / 1000.0;
  }
  return true;
}

// ---------------- 2: SMA-cloud (Monitoring API, OAuth2) ----------------
static const char* SMA_AUTH = "https://auth.smaapis.de/oauth2";
static const char* SMA_API = "https://monitoring.smaapis.de/v1";
static String smaAccess;
static uint32_t smaExpires = 0, lastCloud = 0;

String smaCloudAuthUrl() {
  return String(SMA_AUTH) + "/auth?response_type=code&client_id=" + urlEncode(cfg("smac_cid")) +
         "&redirect_uri=" + urlEncode(cfg("on_redirect")) + "&scope=" + urlEncode("offline_access monitoringApi:read");
}

static bool smaToken(const String& form, String& err) {
  JsonDocument doc;
  int code = httpJson("POST", String(SMA_AUTH) + "/token", form, doc, nullptr, {{"Content-Type", "application/x-www-form-urlencoded"}}, &err);
  if (code != 200 || !doc["access_token"].is<const char*>()) return false;
  smaAccess = doc["access_token"].as<String>();
  smaExpires = millis() / 1000 + (doc["expires_in"] | 3600) - 60;
  if (doc["refresh_token"].is<const char*>()) cfgSet("smac_refresh", doc["refresh_token"].as<String>());
  return true;
}

bool smaCloudExchangeCode(const String& code, String& err) {
  return smaToken("grant_type=authorization_code&code=" + urlEncode(code) + "&redirect_uri=" + urlEncode(cfg("on_redirect")) +
                      "&client_id=" + urlEncode(cfg("smac_cid")) + "&client_secret=" + urlEncode(cfg("smac_secret")),
                  err);
}

static bool cloudRead(String& err) {
  if (millis() - lastCloud < IV_SMA_CLOUD && lastCloud) return !isnan(live.solarW);  // niet vaker dan elke 5 min
  lastCloud = millis();
  if (!smaAccess.length() || millis() / 1000 > smaExpires) {
    if (!cfgHas("smac_refresh")) {
      err = "SMA-cloud nog niet gekoppeld";
      return false;
    }
    if (!smaToken("grant_type=refresh_token&refresh_token=" + urlEncode(cfg("smac_refresh")) + "&client_id=" + urlEncode(cfg("smac_cid")) +
                      "&client_secret=" + urlEncode(cfg("smac_secret")),
                  err))
      return false;
  }
  JsonDocument doc;
  if (!cfgHas("smac_plant")) {
    if (httpJson("GET", String(SMA_API) + "/plants", "", doc, nullptr, {{"Authorization", "Bearer " + smaAccess}}, &err) != 200) return false;
    String id = doc["plants"][0]["plantId"] | "";
    if (!id.length()) {
      err = "geen installatie gevonden";
      return false;
    }
    cfgSet("smac_plant", id);
  }
  int code = httpJson("GET", String(SMA_API) + "/plants/" + cfg("smac_plant") + "/measurements/sets/EnergyAndPowerPv/Recent", "", doc, nullptr,
                      {{"Authorization", "Bearer " + smaAccess}}, &err);
  if (code == 401) smaAccess = "";
  if (code != 200) return false;
  JsonArrayConst set = doc["set"].as<JsonArrayConst>();
  if (!set.size()) {
    err = "geen meetwaarden";
    return false;
  }
  JsonObjectConst last = set[set.size() - 1].as<JsonObjectConst>();
  float w = NAN;
  for (JsonPairConst kv : last) {
    String k = kv.key().c_str();
    k.toLowerCase();
    if (k.indexOf("power") >= 0 && kv.value().is<float>()) w = kv.value().as<float>();
  }
  if (isnan(w) && last["pvGeneration"].is<float>()) w = last["pvGeneration"].as<float>() * 12;  // Wh per 5 min → W
  live.smaW[0] = isnan(w) ? NAN : max(0.0f, w);
  live.smaW[1] = NAN;
  live.smaTotalKwh[0] = live.smaTotalKwh[1] = NAN;  // dagtotaal wordt dan uit het vermogen berekend
  return !isnan(w);
}

// ---------------- 3: HomeWizard kWh-meter op de zonnepanelengroep ----------------
static bool hwRead(String& err) {
  JsonDocument doc;
  if (httpJson("GET", "http://" + cfg("pv_hw_ip") + "/api/v1/data", "", doc, nullptr, {}, &err, 5000) != 200) return false;
  float w = fnum(doc["active_power_w"]);
  double exp = doc["total_power_export_kwh"] | NAN, imp = doc["total_power_import_kwh"] | NAN;
  live.smaW[0] = isnan(w) ? NAN : fabsf(w);
  live.smaW[1] = NAN;
  // Opbrengst staat meestal als 'export' (of 'import' als de meter andersom zit)
  live.smaTotalKwh[0] = (!isnan(exp) && (isnan(imp) || exp >= imp)) ? exp : imp;
  live.smaTotalKwh[1] = NAN;
  return !isnan(w);
}

void pollSma() {
  int mode = cfgInt("sma_mode", 0);
  String err;
  if (mode == 0 && (cfgHas("sma1_ip") || cfgHas("sma2_ip")) && !cfgHas("sma_pass")) {
    markErr(S_SMA, "wachtwoord 'Gebruiker' nog niet ingevuld");
    return;
  }
  if (mode == 2) {
    if (!cfgHas("smac_cid")) return;
    bool ok = cloudRead(err);
    publish(!ok, err);
    return;
  }
  if (mode == 3) {
    if (!cfgHas("pv_hw_ip")) return;
    bool ok = hwRead(err);
    publish(!ok, err);
    return;
  }
  const char* keys[2] = {"sma1_ip", "sma2_ip"};
  bool any = false, anyErr = false;
  String lastErr;
  for (int i = 0; i < 2; i++) {
    String ip = cfg(keys[i]);
    if (!ip.length()) continue;
    any = true;
    bool ok = mode == 1 ? modbusReadSma(i, ip, err) : webRead(i, ip, err);
    if (!ok) {
      anyErr = true;
      lastErr = String(i ? "SB4.0: " : "SB3.0: ") + err;
      live.smaW[i] = NAN;
    }
  }
  if (any && anyErr && isNight()) {
    // De communicatie van de Sunny Boy krijgt stroom van de panelen: 's nachts onbereikbaar = normaal.
    live.smaW[0] = cfg("sma1_ip").length() ? 0 : NAN;
    live.smaW[1] = cfg("sma2_ip").length() ? 0 : NAN;
    publish(false, "");
    strlcpy(live.health[S_SMA].err, "omvormer slaapt ('s nachts normaal)", sizeof(live.health[S_SMA].err));
    return;
  }
  if (any) publish(anyErr, lastErr);
}
