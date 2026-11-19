// Lokale bronnen: HomeWizard P1 en Marstek (Modbus TCP / Open API). SMA staat in sma.cpp.
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
