// Lokale bronnen: HomeWizard P1, HomeWizard kWh-meter (laadpaal), SMA (Modbus TCP), Marstek (Modbus TCP / Open API).
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../net.h"
#include "../sampler.h"
#include "../settings.h"
#include "../state.h"
#include "sources.h"

static inline float fnum(JsonVariantConst v) { return v.is<float>() ? v.as<float>() : NAN; }

// ---------------- HomeWizard P1 (lokale API v1) ----------------
void pollP1() {
  String ip = cfg("p1_ip");
  if (!ip.length()) return;
  JsonDocument doc;
  String err;
  int code = httpJson("GET", "http://" + ip + "/api/v1/data", "", doc, nullptr, {}, &err, 5000);
  if (code != 200) return markErr(S_P1, err);
  live.gridW = fnum(doc["active_power_w"]);
  live.l1 = fnum(doc["active_power_l1_w"]);
  live.l2 = fnum(doc["active_power_l2_w"]);
  live.l3 = fnum(doc["active_power_l3_w"]);
  live.voltage = fnum(doc["active_voltage_l1_v"]);
  live.importKwh = doc["total_power_import_kwh"] | NAN;
  live.exportKwh = doc["total_power_export_kwh"] | NAN;
  live.gasM3 = doc["total_gas_m3"] | NAN;
  samplerAdd(CH_GRID_W, live.gridW);
  samplerAdd(CH_GRID_MAX, live.gridW);
  markOk(S_P1);
}

// ---------------- Laadpaal via HomeWizard kWh-meter ----------------
void pollEv() {
  String ip = cfg("ev_ip");
  if (!ip.length()) return;
  JsonDocument doc;
  String err;
  int code = httpJson("GET", "http://" + ip + "/api/v1/data", "", doc, nullptr, {}, &err, 5000);
  if (code != 200) return markErr(S_EV, err);
  float w = fnum(doc["active_power_w"]);
  if (!isnan(w) && w < 0) w = 0;
  live.evW = w;
  live.evTotalKwh = doc["total_power_import_kwh"] | NAN;
  bool charging = !isnan(w) && w > 500;
  if (charging && live.evState != 1) live.evSessionStart = live.evTotalKwh;  // nieuwe laadsessie
  if (!charging && live.evState == 1) live.evState = 2;                      // klaar
  if (charging) live.evState = 1;
  samplerAdd(CH_EV_W, w);
  markOk(S_EV);
}

// ---------------- SMA Sunny Boy (Modbus TCP) ----------------
// 30775 = AC-vermogen (S32, W), 30529 = totale opbrengst (U32, Wh). 's Nachts geeft 30775 NaN (0x80000000).
void pollSma() {
  const char* keys[2] = {"sma1_ip", "sma2_ip"};
  uint8_t unit = cfgInt("sma_unit", 3);
  bool any = false, anyErr = false;
  String lastErr;
  for (int i = 0; i < 2; i++) {
    String ip = cfg(keys[i]);
    if (!ip.length()) continue;
    any = true;
    uint16_t r[2];
    String err;
    if (modbusRead(ip, 502, unit, 3, 30775, 2, r, err)) {
      int32_t w = mbS32(r);
      live.smaW[i] = (w == (int32_t)0x80000000 || w < 0) ? 0 : (float)w;
    } else {
      anyErr = true;
      lastErr = "SMA " + String(i + 1) + ": " + err;
      live.smaW[i] = NAN;
      continue;
    }
    if (modbusRead(ip, 502, unit, 3, 30529, 2, r, err)) {
      uint32_t wh = mbU32(r);
      if (wh != 0xFFFFFFFF && wh != 0x80000000) live.smaTotalKwh[i] = wh / 1000.0;
    }
  }
  if (!any) return;
  float s = 0;
  double tot = 0;
  bool okW = false, okT = true;
  for (int i = 0; i < 2; i++) {
    if (!isnan(live.smaW[i])) s += live.smaW[i], okW = true;
    if (cfg(keys[i]).length()) {
      if (isnan(live.smaTotalKwh[i])) okT = false;
      else tot += live.smaTotalKwh[i];
    }
  }
  live.solarW = okW ? s : NAN;
  live.solarTotalKwh = okT ? tot : NAN;
  samplerAdd(CH_SOLAR_W, live.solarW);
  if (anyErr) markErr(S_SMA, lastErr);
  else markOk(S_SMA);
}

// ---------------- Marstek Venus E V3 ----------------
// Modbus-registers (controleer met de marstek-modbus-suite en de Marstek-app, fase 3):
//   32102 batterijvermogen (S32, W)  32104 SoC (U16, %)
//   33000 totaal geladen (U32, 0,01 kWh)  33002 totaal ontladen (U32, 0,01 kWh)
static const uint16_t MS_REG_POWER = 32102, MS_REG_SOC = 32104, MS_REG_ENERGY = 33000;

static bool marstekModbus(int i, const String& ip, String& err) {
  uint16_t port = cfgInt("bat_port", 502);
  uint8_t unit = cfgInt("bat_unit", 1);
  uint16_t r[4];
  if (!modbusRead(ip, port, unit, 3, MS_REG_POWER, 2, r, err)) return false;
  float w = (float)mbS32(r);
  if (!modbusRead(ip, port, unit, 3, MS_REG_SOC, 1, r, err)) return false;
  live.batSoc[i] = r[0];
  if (modbusRead(ip, port, unit, 3, MS_REG_ENERGY, 4, r, err)) {
    live.batChgKwh[i] = mbU32(r) / 100.0;
    live.batDisKwh[i] = mbU32(r + 2) / 100.0;
  }
  live.batW[i] = cfgBool("bat_invert") ? -w : w;
  return true;
}

// Marstek Open API (UDP, JSON-RPC): ES.GetStatus geeft bat_soc en bat_power.
static bool marstekUdp(int i, const String& ip, String& err) {
  uint16_t port = cfgInt("bat_port", 30000);
  WiFiUDP udp;
  udp.begin(30000 + 10 + i);
  udp.beginPacket(ip.c_str(), port);
  udp.print("{\"id\":1,\"method\":\"ES.GetStatus\",\"params\":{\"id\":0}}");
  udp.endPacket();
  uint32_t t0 = millis();
  while (millis() - t0 < 2500) {
    int n = udp.parsePacket();
    if (n > 0) {
      JsonDocument doc;
      if (deserializeJson(doc, udp)) {
        err = "ongeldig antwoord";
        udp.stop();
        return false;
      }
      JsonVariantConst res = doc["result"];
      live.batSoc[i] = fnum(res["bat_soc"]);
      float w = fnum(res["bat_power"]);
      live.batW[i] = cfgBool("bat_invert") ? -w : w;
      udp.stop();
      return !isnan(live.batSoc[i]);
    }
    delay(20);
  }
  udp.stop();
  err = "geen antwoord (Open API aan in de Marstek-app?)";
  return false;
}

void pollBatteries() {
  const char* keys[N_BAT] = {"bat1_ip", "bat2_ip"};
  const SrcId src[N_BAT] = {S_BAT1, S_BAT2};
  for (int i = 0; i < N_BAT; i++) {
    String ip = cfg(keys[i]);
    if (!ip.length()) continue;
    String err;
    bool ok = cfgInt("bat_mode", 0) == 1 ? marstekUdp(i, ip, err) : marstekModbus(i, ip, err);
    if (!ok) {
      live.batW[i] = NAN;
      markErr(src[i], err);
      continue;
    }
    samplerAdd(i == 0 ? CH_BAT1_W : CH_BAT2_W, live.batW[i]);
    samplerAdd(i == 0 ? CH_BAT1_SOC : CH_BAT2_SOC, live.batSoc[i]);
    markOk(src[i]);
  }
}
