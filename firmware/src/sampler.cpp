#include "sampler.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include "settings.h"
#include "sources/sources.h"
#include "state.h"

// Dagbuffer: kanalen x 5-minutenstappen als int16 (temperaturen/percentages x10). ~22 KB RAM.
#define MISSING INT16_MIN
static int16_t V[N_CH][MAX_STEPS];

struct Acc {
  double sum = 0;
  uint16_t n = 0;
  float mx = -1e9f, mn = 1e9f, last = NAN;
};
static Acc acc[N_CH];
static float lastV[N_CH];
static uint32_t lastT[N_CH];

// Vermogens per minuut (net, zon, batterijen, fasen). ~24 KB RAM.
static int16_t F[N_FAST][MAX_FAST];
struct AccF {
  double sum = 0;
  uint16_t n = 0;
  float mx = -1e9f;
};
static AccF accF[N_FAST];
static int fastIdx(int ch) {
  for (int f = 0; f < N_FAST; f++)
    if (FAST_CH[f] == ch) return f;
  return -1;
}
static int16_t fastOrSlow(int f, int s);
static void healCounters();
// Tellers: tijdstip van de laatste geaccepteerde stand en sinds wanneer de teller lager staat (vervangen meter?)
static uint32_t cT1[9], cLowSince[9];

struct Run {
  uint8_t appl;
  uint16_t start, end;  // minuten na middernacht; end = 0xFFFF zolang hij loopt
  char program[40];
};

struct Meta {
  char date[11] = "";
  uint32_t start = 0;
  uint16_t n = 288;
  int16_t curIdx = -1;
  uint16_t nF = 1440;
  int16_t curF = -1;
  uint32_t tC = 0;  // tijdstip van de laatste geaccepteerde tellerstand
  // Tellerstanden bij begin (c0) en laatst gezien (c1) van de dag
  double c0[9], c1[9];
  double intChgWh[N_BAT] = {0, 0}, intDisWh[N_BAT] = {0, 0};
  float peakImport = 0, peakExport = 0;
  float climateKwh[N_CLIMATE];
  Run runs[12];
  uint8_t nRuns = 0;
};
static Meta M;
// Indexen van de tellers in c0/c1
enum { K_IMP, K_EXP, K_SOLAR, K_GAS, K_CHG1, K_CHG2, K_DIS1, K_DIS2, N_K };

static double counterNow(int k) {
  switch (k) {
    case K_IMP: return live.importKwh;
    case K_EXP: return live.exportKwh;
    case K_SOLAR: return live.solarTotalKwh;
    case K_GAS: return live.gasM3;
    case K_CHG1: return live.batChgKwh[0];
    case K_CHG2: return live.batChgKwh[1];
    case K_DIS1: return live.batDisKwh[0];
    case K_DIS2: return live.batDisKwh[1];
  }
  return NAN;
}

// ---------- Kanaaleigenschappen ----------
static const char* const BASE_NAMES[] = {"grid_w", "grid_max", "solar_w", "bat1_w", "bat2_w",
                                         "bat1_soc", "bat2_soc", "t_out", "cloud"};

const char* chName(int ch, char* buf, size_t len) {
  if (ch < CH_AC_BASE) return BASE_NAMES[ch];
  if (ch < CH_PELLET_T) {
    int i = (ch - CH_AC_BASE) / 3, k = (ch - CH_AC_BASE) % 3;
    snprintf(buf, len, "ac_%s_%s", CLIMATE_IDS[i], k == 0 ? "t" : k == 1 ? "set" : "mode");
    return buf;
  }
  static const char* const rest[] = {"pellet_t", "pellet_on", "eurom_slaap_on", "eurom_bad_on", "eurom_slaap_t", "eurom_bad_t",
                                     "grid_l1", "grid_l2", "grid_l3"};
  return rest[ch - CH_PELLET_T];
}

static bool isPower(int ch) {
  return ch == CH_GRID_W || ch == CH_GRID_MAX || ch == CH_SOLAR_W || ch == CH_BAT1_W || ch == CH_BAT2_W || ch == CH_L1 ||
         ch == CH_L2 || ch == CH_L3;
}
static uint8_t chScale(int ch) {
  if (isPower(ch) || ch == CH_CLOUD || ch == CH_PELLET_ON || ch == CH_EUROM_SLAAP_ON || ch == CH_EUROM_BAD_ON) return 1;
  if (ch >= CH_AC_BASE && ch < CH_PELLET_T && (ch - CH_AC_BASE) % 3 == 2) return 1;  // modus
  return 10;
}
static Agg chAgg(int ch) {
  if (ch == CH_GRID_MAX || ch == CH_PELLET_ON || ch == CH_EUROM_SLAAP_ON || ch == CH_EUROM_BAD_ON) return AGG_MAX;
  if (ch == CH_BAT1_SOC || ch == CH_BAT2_SOC) return AGG_LAST;
  if (ch >= CH_AC_BASE && ch < CH_PELLET_T && (ch - CH_AC_BASE) % 3 != 0) return AGG_LAST;
  return AGG_AVG;
}

static int16_t enc(float v, uint8_t scale) {
  if (isnan(v)) return MISSING;
  float s = roundf(v * scale);
  return (int16_t)constrain(s, -32767.0f, 32767.0f);
}

static void printScaled(Print& out, int16_t v, uint8_t scale) {
  if (v == MISSING) {
    out.print("null");
    return;
  }
  if (scale == 1) {
    out.print(v);
    return;
  }
  int a = abs((int)v);
  if (v < 0) out.print('-');
  out.print(a / 10);
  if (a % 10) {
    out.print('.');
    out.print(a % 10);
  }
}

// ---------- Datum/tijd ----------
static void localDate(time_t t, char* out) {
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(out, 11, "%Y-%m-%d", &tm);
}
static uint32_t midnightOf(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return (uint32_t)mktime(&tm);
}
String todayDate() { return String(M.date); }

// ---------- Opslag ----------
static void saveMeta() {
  JsonDocument d;
  d["date"] = M.date;
  d["start"] = M.start;
  d["n"] = M.n;
  d["cur"] = M.curIdx;
  d["nch"] = N_CH;
  d["nf"] = M.nF;
  d["curf"] = M.curF;
  d["nfa"] = N_FAST;
  d["tc"] = M.tC;
  JsonArray c0 = d["c0"].to<JsonArray>(), c1 = d["c1"].to<JsonArray>();
  for (int k = 0; k < N_K; k++) {
    if (isnan(M.c0[k])) c0.add(nullptr);
    else c0.add(M.c0[k]);
    if (isnan(M.c1[k])) c1.add(nullptr);
    else c1.add(M.c1[k]);
  }
  for (int b = 0; b < N_BAT; b++) {
    d["ic"][b] = M.intChgWh[b];
    d["id"][b] = M.intDisWh[b];
  }
  d["pi"] = M.peakImport;
  d["pe"] = M.peakExport;
  for (int i = 0; i < N_CLIMATE; i++) {
    if (isnan(M.climateKwh[i])) d["ck"][i] = nullptr;
    else d["ck"][i] = M.climateKwh[i];
  }
  for (int r = 0; r < M.nRuns; r++) {
    JsonObject o = d["runs"].add<JsonObject>();
    o["a"] = M.runs[r].appl;
    o["s"] = M.runs[r].start;
    o["e"] = M.runs[r].end;
    o["p"] = M.runs[r].program;
  }
  File f = LittleFS.open("/today/meta.json", "w");
  if (f) {
    serializeJson(d, f);
    f.close();
  }
}

static void appendRow(int idx) {
  File f = LittleFS.open("/today/rows.bin", "a");
  if (!f) return;
  uint16_t i = idx;
  f.write((uint8_t*)&i, 2);
  for (int ch = 0; ch < N_CH; ch++) f.write((uint8_t*)&V[ch][idx], 2);
  f.close();
}

static void appendFast(int idx) {
  File f = LittleFS.open("/today/fast.bin", "a");
  if (!f) return;
  uint16_t i = idx;
  f.write((uint8_t*)&i, 2);
  for (int k = 0; k < N_FAST; k++) f.write((uint8_t*)&F[k][idx], 2);
  f.close();
}

// carry: tellerstanden aan het eind van de vorige dag. Die worden het beginpunt van deze dag,
// zodat er niets verloren gaat als de ESP rond middernacht even uit stond.
static void newDay(uint32_t now, const double* carry = nullptr) {
  M = Meta();
  localDate(now, M.date);
  M.start = midnightOf(now);
  uint32_t next = midnightOf(M.start + 26 * 3600);
  M.n = constrain((next - M.start) / STEP_S, 1, MAX_STEPS);
  M.nF = constrain((next - M.start) / FAST_STEP_S, 1, MAX_FAST);
  for (int k = 0; k < N_K; k++) M.c0[k] = M.c1[k] = carry ? carry[k] : NAN;
  for (int f = 0; f < N_FAST; f++)
    for (int s = 0; s < MAX_FAST; s++) F[f][s] = MISSING;
  LittleFS.remove("/today/fast.bin");
  for (int i = 0; i < N_CLIMATE; i++) M.climateKwh[i] = NAN;
  for (int ch = 0; ch < N_CH; ch++)
    for (int s = 0; s < MAX_STEPS; s++) V[ch][s] = MISSING;
  LittleFS.remove("/today/rows.bin");
  saveMeta();
  Serial.printf("[sampler] nieuwe dag %s (%u stappen)\n", M.date, M.n);
}

static void restore() {
  if (!LittleFS.exists("/today/meta.json")) return;
  File f = LittleFS.open("/today/meta.json", "r");
  if (!f) return;
  JsonDocument d;
  if (deserializeJson(d, f)) {
    f.close();
    return;
  }
  f.close();
  M = Meta();
  strlcpy(M.date, d["date"] | "", sizeof(M.date));
  M.start = d["start"] | 0;
  M.n = d["n"] | 288;
  M.curIdx = d["cur"] | -1;
  M.nF = d["nf"] | (uint16_t)(M.n * (STEP_S / FAST_STEP_S));
  M.curF = d["curf"] | -1;
  M.tC = d["tc"] | 0;
  for (int k = 0; k < N_K; k++) {
    M.c0[k] = d["c0"][k].is<double>() ? d["c0"][k].as<double>() : NAN;
    M.c1[k] = d["c1"][k].is<double>() ? d["c1"][k].as<double>() : NAN;
  }
  for (int b = 0; b < N_BAT; b++) {
    M.intChgWh[b] = d["ic"][b] | 0.0;
    M.intDisWh[b] = d["id"][b] | 0.0;
  }
  M.peakImport = d["pi"] | 0.0f;
  M.peakExport = d["pe"] | 0.0f;
  for (int i = 0; i < N_CLIMATE; i++) M.climateKwh[i] = d["ck"][i].is<float>() ? d["ck"][i].as<float>() : NAN;
  for (JsonVariantConst r : d["runs"].as<JsonArrayConst>()) {
    if (M.nRuns >= 12) break;
    Run& x = M.runs[M.nRuns++];
    x.appl = r["a"] | 0;
    x.start = r["s"] | 0;
    x.end = r["e"] | 0xFFFF;
    strlcpy(x.program, r["p"] | "", sizeof(x.program));
  }
  for (int ch = 0; ch < N_CH; ch++)
    for (int s = 0; s < MAX_STEPS; s++) V[ch][s] = MISSING;
  bool sameLayout = (d["nch"] | 0) == N_CH;
  if (!sameLayout) Serial.println("[sampler] nieuwe firmware met andere kanalen: meetpunten van vandaag opnieuw beginnen");
  File rf = (sameLayout && LittleFS.exists("/today/rows.bin")) ? LittleFS.open("/today/rows.bin", "r") : File();
  if (rf) {
    uint16_t idx;
    while (rf.read((uint8_t*)&idx, 2) == 2) {
      for (int ch = 0; ch < N_CH; ch++) {
        int16_t v;
        if (rf.read((uint8_t*)&v, 2) != 2) break;
        if (idx < MAX_STEPS) V[ch][idx] = v;
      }
    }
    rf.close();
  }
  for (int f = 0; f < N_FAST; f++)
    for (int s = 0; s < MAX_FAST; s++) F[f][s] = MISSING;
  File ff = ((d["nfa"] | 0) == N_FAST && LittleFS.exists("/today/fast.bin")) ? LittleFS.open("/today/fast.bin", "r") : File();
  int nFast = 0;
  if (ff) {
    uint16_t idx;
    int16_t row[N_FAST];
    while (ff.read((uint8_t*)&idx, 2) == 2 && ff.read((uint8_t*)row, sizeof(row)) == sizeof(row)) {
      if (idx < MAX_FAST) {
        for (int f = 0; f < N_FAST; f++) F[f][idx] = row[f];
        nFast++;
      }
    }
    ff.close();
  }
  Serial.printf("[sampler] dag %s hersteld (%d minuten)\n", M.date, nFast);
}

// ---------- Totalen ----------
static double integ(int ch, int sign) {
  int fi = fastIdx(ch);
  if (fi >= 0) {
    double wh = 0;
    for (int s = 0; s < M.nF; s++) {
      int16_t v = F[fi][s];
      if (v == MISSING || (sign > 0 && v < 0) || (sign < 0 && v > 0)) continue;
      wh += fabs((double)v) * FAST_STEP_S / 3600.0;
    }
    return wh / 1000.0;
  }
  double wh = 0;
  for (int s = 0; s < M.n; s++) {
    int16_t v = V[ch][s];
    if (v == MISSING) continue;
    if (sign > 0 && v < 0) continue;
    if (sign < 0 && v > 0) continue;
    wh += fabs((double)v) * STEP_S / 3600.0;
  }
  return wh / 1000.0;
}
static double diffK(int k) {
  double a = M.c0[k], b = M.c1[k];
  return (!isnan(a) && !isnan(b) && b >= a) ? b - a : NAN;
}
static double r3(double v) { return round(v * 1000.0) / 1000.0; }

static void buildTotals(JsonDocument& t, bool final) {
  t["date"] = M.date;
  // Teller (exact), behalve als die ontbreekt of duidelijk lager is dan gemeten
  auto pick = [](double counter, double measured) {
    return (isnan(counter) || (measured > 0.3 && counter < measured * 0.9 - 0.3)) ? measured : counter;
  };
  double imp = pick(diffK(K_IMP), integ(CH_GRID_W, 1));
  double exp = pick(diffK(K_EXP), integ(CH_GRID_W, -1));
  double sol = pick(diffK(K_SOLAR), integ(CH_SOLAR_W, 1));
  double chg = 0, dis = 0;
  for (int b = 0; b < N_BAT; b++) {
    double c = diffK(b == 0 ? K_CHG1 : K_CHG2), d = diffK(b == 0 ? K_DIS1 : K_DIS2);
    chg += isnan(c) ? M.intChgWh[b] / 1000.0 : c;
    dis += isnan(d) ? M.intDisWh[b] / 1000.0 : d;
  }
  t["importKwh"] = r3(imp);
  t["exportKwh"] = r3(exp);
  t["solarKwh"] = r3(sol);
  t["batChargeKwh"] = r3(chg);
  t["batDischargeKwh"] = r3(dis);
  double gas = diffK(K_GAS);
  if (isnan(gas)) t["gasM3"] = nullptr;
  else t["gasM3"] = r3(gas);
  t["evKwh"] = nullptr;
  t["peakImportW"] = (int)M.peakImport;
  t["peakExportW"] = (int)M.peakExport;
  // Buitentemperatuur en bewolking
  float tMin = 1e9, tMax = -1e9;
  double tSum = 0, cSum = 0;
  int tN = 0, cN = 0;
  for (int s = 0; s < M.n; s++) {
    if (V[CH_T_OUT][s] != MISSING) {
      float v = V[CH_T_OUT][s] / 10.0f;
      tMin = min(tMin, v), tMax = max(tMax, v), tSum += v, tN++;
    }
    if (V[CH_CLOUD][s] != MISSING) cSum += V[CH_CLOUD][s], cN++;
  }
  if (tN) {
    t["tOutMin"] = round(tMin * 10) / 10.0;
    t["tOutMax"] = round(tMax * 10) / 10.0;
    t["tOutAvg"] = round(tSum / tN * 10) / 10.0;
  } else {
    t["tOutMin"] = t["tOutMax"] = t["tOutAvg"] = nullptr;
  }
  if (cN) t["cloudAvg"] = (int)round(cSum / cN);
  else t["cloudAvg"] = nullptr;
  JsonObject ck = t["climateKwh"].to<JsonObject>();
  for (int i = 0; i < N_CLIMATE; i++)
    if (!isnan(M.climateKwh[i])) ck[CLIMATE_IDS[i]] = r3(M.climateKwh[i]);
  JsonObject hh = t["heaterHours"].to<JsonObject>();
  const int onCh[N_HEATER] = {CH_PELLET_ON, CH_EUROM_SLAAP_ON, CH_EUROM_BAD_ON};
  for (int h = 0; h < N_HEATER; h++) {
    int cnt = 0;
    for (int s = 0; s < M.n; s++)
      if (V[onCh[h]][s] != MISSING && V[onCh[h]][s] > 0) cnt++;
    hh[HEATER_IDS[h]] = r3(cnt * STEP_S / 3600.0);
  }
  JsonArray runs = t["runs"].to<JsonArray>();
  for (int r = 0; r < M.nRuns; r++) {
    JsonObject o = runs.add<JsonObject>();
    o["id"] = APPL_IDS[M.runs[r].appl];
    o["start"] = M.runs[r].start;
    uint16_t end = M.runs[r].end;
    if (end == 0xFFFF) end = final ? 1440 : (uint16_t)((nowUnix() - M.start) / 60);
    o["end"] = end;
    o["program"] = M.runs[r].program;
  }
}

// ---------- Emmers (5 minuten) ----------
static void closeBucket() {
  int idx = M.curIdx;
  if (idx < 0 || idx >= M.n) return;
  uint32_t now = nowUnix();
  for (int ch = 0; ch < N_CH; ch++) {
    Acc& a = acc[ch];
    float v = NAN;
    if (a.n) {
      Agg g = chAgg(ch);
      v = g == AGG_MAX ? a.mx : g == AGG_LAST ? a.last : (float)(a.sum / a.n);
    } else if (!isPower(ch) && lastT[ch] && now - lastT[ch] < 2400) {
      v = lastV[ch];  // trage bronnen: laatste waarde doortrekken (max 40 min)
    }
    if (!isnan(v)) V[ch][idx] = enc(v, chScale(ch));
  }
  if (acc[CH_GRID_MAX].n) M.peakImport = max(M.peakImport, acc[CH_GRID_MAX].mx);
  if (acc[CH_GRID_W].n && acc[CH_GRID_W].mn < 0) M.peakExport = max(M.peakExport, -acc[CH_GRID_W].mn);
  appendRow(idx);
  healCounters();
  saveMeta();
  for (int ch = 0; ch < N_CH; ch++) acc[ch] = Acc();
}

static void closeFast() {
  int idx = M.curF;
  bool any = false;
  if (idx >= 0 && idx < M.nF) {
    for (int f = 0; f < N_FAST; f++) {
      AccF& a = accF[f];
      if (!a.n) continue;
      float v = FAST_CH[f] == CH_GRID_MAX ? a.mx : (float)(a.sum / a.n);
      F[f][idx] = enc(v, 1);
      any = true;
    }
    if (any) appendFast(idx);
  }
  for (int f = 0; f < N_FAST; f++) accF[f] = AccF();
}

// ---------- Tellers ----------
// Alleen geloofwaardige standen accepteren: een teller loopt nooit terug en springt niet ineens enorm.
static char counterNote[96] = "";
const char* samplerCounterNote() { return counterNote; }
static const char* const K_NAMES[N_K] = {"import", "export", "zon", "gas", "laden1", "laden2", "ontladen1", "ontladen2"};

// Regels (een dagtotaal mag nooit verloren gaan):
//  - een stand die meer stijgt dan fysiek kan (> 20 kW sinds de vorige stand) is een leesfout: negeren
//  - een stand die LAGER is dan eerder is een storing: negeren. Blijft hij 2 uur lager (nieuwe meter),
//    dan verder tellen vanaf de nieuwe stand, met behoud van wat er vandaag al geteld was.
static void updateCounters(uint32_t now) {
  for (int k = 0; k < N_K; k++) {
    double c = counterNow(k);
    if (isnan(c) || c < 0) continue;
    if (isnan(M.c0[k]) || isnan(M.c1[k])) {
      M.c0[k] = M.c1[k] = c;
      cT1[k] = M.tC = now;
      continue;
    }
    if (c < M.c1[k] - 0.005) {
      if (!cLowSince[k]) cLowSince[k] = now;
      if (now - cLowSince[k] > 7200) {
        double done = M.c1[k] - M.c0[k];
        snprintf(counterNote, sizeof(counterNote), "%s: meter lijkt vervangen (%.2f -> %.2f), doorgeteld", K_NAMES[k], M.c1[k], c);
        Serial.printf("[sampler] %s\n", counterNote);
        M.c1[k] = c;
        M.c0[k] = c - done;
        cLowSince[k] = 0;
        cT1[k] = M.tC = now;
      }
      continue;
    }
    cLowSince[k] = 0;
    uint32_t since = cT1[k] ? cT1[k] : M.tC;
    double hours = since && now > since ? (now - since) / 3600.0 : 48;
    if (c - M.c1[k] > 0.1 + 20.0 * hours) {
      snprintf(counterNote, sizeof(counterNote), "%s: onmogelijke sprong genegeerd (%.2f -> %.2f)", K_NAMES[k], M.c1[k], c);
      Serial.printf("[sampler] %s\n", counterNote);
      continue;
    }
    M.c1[k] = c;
    cT1[k] = M.tC = now;
  }
}

// Zelfherstel: een tellertotaal kan nooit lager zijn dan wat er volgens de metingen minimaal doorheen ging
// (de metingen missen hooguit iets door gaten). Is dat wel zo, dan klopt de beginstand niet: die corrigeren.
static void healCounters() {
  struct Map {
    int k, ch, sign;
  };
  static const Map maps[] = {{K_IMP, CH_GRID_W, 1},   {K_EXP, CH_GRID_W, -1},  {K_SOLAR, CH_SOLAR_W, 1}, {K_CHG1, CH_BAT1_W, 1},
                             {K_CHG2, CH_BAT2_W, 1}, {K_DIS1, CH_BAT1_W, -1}, {K_DIS2, CH_BAT2_W, -1}};
  for (const Map& m : maps) {
    if (isnan(M.c0[m.k]) || isnan(M.c1[m.k])) continue;
    double counted = M.c1[m.k] - M.c0[m.k];
    double measured = integ(m.ch, m.sign);
    if (measured > 0.3 && counted < measured * 0.9 - 0.3) {
      snprintf(counterNote, sizeof(counterNote), "%s: beginstand hersteld (%.2f -> %.2f kWh vandaag)", K_NAMES[m.k], counted, measured);
      Serial.printf("[sampler] %s\n", counterNote);
      M.c0[m.k] = M.c1[m.k] - measured;
    }
  }
}

// ---------- Afgesloten dag wegschrijven ----------
static void finalizeDay() {
  if (!M.date[0]) return;
  LittleFS.mkdir("/p");
  LittleFS.mkdir("/sum");
  JsonDocument t;
  buildTotals(t, true);
  String line;
  serializeJson(t, line);

  String base = String("/p/") + M.date;
  File f = LittleFS.open(base + ".bin", "w");
  if (f) {
    uint16_t nch = N_CH;
    f.write((uint8_t*)&M.start, 4);
    f.write((uint8_t*)&M.n, 2);
    f.write((uint8_t*)&nch, 2);
    for (int ch = 0; ch < N_CH; ch++) f.write((uint8_t*)V[ch], M.n * 2);
    f.close();
  }
  for (int k = 0; k < N_FAST; k++)
    for (int x = 0; x < M.nF; x++) F[k][x] = fastOrSlow(k, x);
  M.curIdx = -1;
  f = LittleFS.open(base + ".fst", "w");
  if (f) {
    uint16_t nf = N_FAST;
    f.write((uint8_t*)&M.nF, 2);
    f.write((uint8_t*)&nf, 2);
    for (int k = 0; k < N_FAST; k++) f.write((uint8_t*)F[k], M.nF * 2);
    f.close();
  }
  f = LittleFS.open(base + ".tot", "w");
  if (f) {
    f.print(line);
    f.close();
  }
  // Jaarsamenvatting: één regel per dag (dubbele dag voorkomen)
  String sumPath = String("/sum/") + String(M.date).substring(0, 4) + ".jsonl";
  bool exists = false;
  File s = LittleFS.open(sumPath, "r");
  if (s) {
    String needle = String("\"date\":\"") + M.date + "\"";
    while (s.available() && !exists) exists = s.readStringUntil('\n').indexOf(needle) >= 0;
    s.close();
  }
  if (!exists) {
    s = LittleFS.open(sumPath, "a");
    if (s) {
      s.println(line);
      s.close();
    }
  }
  // Te veel achterstand? Oudste weggooien.
  while (pendingCount() > MAX_PENDING_DAYS) {
    String old;
    if (!pendingOldest(old)) break;
    pendingDone(old);
  }
  resetDailyCalls();
  Serial.printf("[sampler] dag %s afgesloten\n", M.date);
}

// ---------- Publieke functies ----------
void samplerBegin() {
  LittleFS.mkdir("/today");
  LittleFS.mkdir("/p");
  LittleFS.mkdir("/sum");
  for (int ch = 0; ch < N_CH; ch++) lastV[ch] = NAN, lastT[ch] = 0;
  for (int k = 0; k < N_K; k++) M.c0[k] = M.c1[k] = NAN;
  for (int i = 0; i < N_CLIMATE; i++) M.climateKwh[i] = NAN;
  for (int ch = 0; ch < N_CH; ch++)
    for (int s = 0; s < MAX_STEPS; s++) V[ch][s] = MISSING;
  for (int f = 0; f < N_FAST; f++)
    for (int s = 0; s < MAX_FAST; s++) F[f][s] = MISSING;
  restore();
}

void samplerAdd(int ch, float v) {
  if (ch < 0 || ch >= N_CH || isnan(v)) return;
  uint32_t now = nowUnix();
  if (ch == CH_BAT1_W || ch == CH_BAT2_W) {  // reserve: zelf laden/ontladen optellen
    int b = ch == CH_BAT1_W ? 0 : 1;
    if (lastT[ch] && now > lastT[ch]) {
      double wh = v * (double)min(now - lastT[ch], (uint32_t)120) / 3600.0;
      if (wh > 0) M.intChgWh[b] += wh;
      else M.intDisWh[b] -= wh;
    }
  }
  int fi = fastIdx(ch);
  if (fi >= 0) {
    accF[fi].sum += v;
    accF[fi].n++;
    accF[fi].mx = max(accF[fi].mx, v);
  }
  Acc& a = acc[ch];
  a.sum += v;
  a.n++;
  a.mx = max(a.mx, v);
  a.mn = min(a.mn, v);
  a.last = v;
  lastV[ch] = v;
  lastT[ch] = now;
}

void samplerSetClimateKwh(int idx, float kwh) {
  if (idx >= 0 && idx < N_CLIMATE && !isnan(kwh)) M.climateKwh[idx] = kwh;
}

void samplerRunStart(int appl, const char* program) {
  if (!timeValid() || M.nRuns >= 12) return;
  Run& r = M.runs[M.nRuns++];
  r.appl = appl;
  r.start = (nowUnix() - M.start) / 60;
  r.end = 0xFFFF;
  strlcpy(r.program, program ? program : "", sizeof(r.program));
  saveMeta();
}

void samplerRunEnd(int appl) {
  for (int r = M.nRuns - 1; r >= 0; r--)
    if (M.runs[r].appl == appl && M.runs[r].end == 0xFFFF) {
      M.runs[r].end = (nowUnix() - M.start) / 60;
      saveMeta();
      return;
    }
}

void samplerTick() {
  if (!timeValid()) return;
  uint32_t now = nowUnix();
  char d[11];
  localDate(now, d);
  if (!M.date[0]) newDay(now);
  if (strcmp(d, M.date) != 0) {
    updateCounters(now);
    closeFast();
    closeBucket();
    finalizeDay();
    // Aansluitende dag: eindstand van gisteren = beginstand van vandaag
    double carry[N_K];
    bool consecutive = midnightOf(now) - M.start <= 25 * 3600;
    for (int k = 0; k < N_K; k++) carry[k] = M.c1[k];
    newDay(now, consecutive ? carry : nullptr);
  }
  updateCounters(now);
  int fidx = min((int)((now - M.start) / FAST_STEP_S), (int)M.nF - 1);
  if (M.curF < 0) M.curF = fidx;
  if (fidx != M.curF) {
    closeFast();
    M.curF = fidx;
  }
  int idx = min((int)((now - M.start) / STEP_S), (int)M.n - 1);
  if (M.curIdx < 0) M.curIdx = idx;
  if (idx != M.curIdx) {
    closeBucket();
    M.curIdx = idx;
  }
}

static void writeHeader(Print& out, const char* date, uint32_t start, uint16_t n) {
  out.print("{\"date\":\"");
  out.print(date);
  out.print("\",\"start\":");
  out.print(start);
  out.print(",\"step\":");
  out.print(STEP_S);
  out.print(",\"n\":");
  out.print(n);
  out.print(",\"ch\":{");
}

// Minuutwaarde, of (als die ontbreekt) het 5-minutengemiddelde van dat moment. Zo vallen er geen gaten
// bij een korte herstart of bij dagen van vóór de minuutopslag.
static int16_t fastOrSlow(int f, int s) {
  int16_t v = F[f][s];
  if (v != MISSING) return v;
  int si = s * FAST_STEP_S / STEP_S;
  return (si < M.n && si != M.curIdx) ? V[FAST_CH[f]][si] : MISSING;
}

static void writeFastJson(Print& out, uint16_t nF, File* src) {
  out.print("},\"fast\":{\"step\":");
  out.print(FAST_STEP_S);
  out.print(",\"n\":");
  out.print(nF);
  out.print(",\"ch\":{");
  char nb[32];
  int16_t buf[60];
  for (int f = 0; f < N_FAST; f++) {
    if (f) out.print(',');
    out.print('"');
    out.print(chName(FAST_CH[f], nb, sizeof(nb)));
    out.print("\":[");
    for (int s = 0; s < nF;) {
      int k = min(60, nF - s);
      if (src) {
        if (src->read((uint8_t*)buf, k * 2) != k * 2)
          for (int j = 0; j < k; j++) buf[j] = MISSING;
      } else {
        for (int j = 0; j < k; j++) buf[j] = fastOrSlow(f, s + j);
      }
      for (int j = 0; j < k; j++, s++) {
        if (s) out.print(',');
        printScaled(out, buf[j], 1);
      }
    }
    out.print(']');
  }
  out.print('}');  // einde fast.ch (de aanroeper sluit "fast" af)
}

void writeTodayJson(Print& out) {
  char nb[32];
  writeHeader(out, M.date, M.start, M.n);
  bool first = true;
  for (int ch = 0; ch < N_CH; ch++) {
    if (fastIdx(ch) >= 0) continue;  // staat per minuut in "fast"
    if (!first) out.print(',');
    first = false;
    out.print('"');
    out.print(chName(ch, nb, sizeof(nb)));
    out.print("\":[");
    uint8_t sc = chScale(ch);
    for (int s = 0; s < M.n; s++) {
      if (s) out.print(',');
      printScaled(out, V[ch][s], sc);
    }
    out.print(']');
  }
  writeFastJson(out, M.nF, nullptr);
  out.print("},\"totals\":");
  JsonDocument t;
  buildTotals(t, false);
  serializeJson(t, out);
  out.print('}');
}

int pendingCount() {
  int n = 0;
  File dir = LittleFS.open("/p");
  if (!dir) return 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile())
    if (String(f.name()).endsWith(".bin")) n++;
  return n;
}

bool pendingOldest(String& date) {
  date = "";
  File dir = LittleFS.open("/p");
  if (!dir) return false;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (!n.endsWith(".bin")) continue;
    n = n.substring(0, n.length() - 4);
    if (!date.length() || n < date) date = n;
  }
  return date.length() > 0;
}

bool writePendingDayJson(const String& date, Print& out) {
  File f = LittleFS.open("/p/" + date + ".bin", "r");
  if (!f) return false;
  uint32_t start = 0;
  uint16_t n = 0;
  uint16_t nch = 0;
  f.read((uint8_t*)&start, 4);
  f.read((uint8_t*)&n, 2);
  f.read((uint8_t*)&nch, 2);
  char nb[32];
  File fst = LittleFS.open("/p/" + date + ".fst", "r");
  uint16_t fN = 0, fCh = 0;
  if (fst) {
    fst.read((uint8_t*)&fN, 2);
    fst.read((uint8_t*)&fCh, 2);
    if (fCh != N_FAST || !fN) {
      fst.close();
      fst = File();
    }
  }
  writeHeader(out, date.c_str(), start, n);
  int16_t buf[48];
  bool first = true;
  for (int ch = 0; ch < N_CH; ch++) {
    if (fst && fastIdx(ch) >= 0) {  // staat per minuut in "fast": overslaan
      if (ch < nch) f.seek(f.position() + n * 2);
      continue;
    }
    if (!first) out.print(',');
    first = false;
    out.print('"');
    out.print(chName(ch, nb, sizeof(nb)));
    out.print("\":[");
    uint8_t sc = chScale(ch);
    int left = n, s = 0;
    if (ch >= nch) {  // kanaal bestond nog niet toen deze dag werd opgeslagen
      for (int j = 0; j < n; j++) out.print(j ? ",null" : "null");
      left = 0;
    }
    while (left > 0) {
      int k = min(left, 48);
      f.read((uint8_t*)buf, k * 2);
      for (int j = 0; j < k; j++, s++) {
        if (s) out.print(',');
        printScaled(out, buf[j], sc);
      }
      left -= k;
    }
    out.print(']');
  }
  f.close();
  if (fst) {
    writeFastJson(out, fN, &fst);
    fst.close();
  }
  out.print("},\"totals\":");
  File t = LittleFS.open("/p/" + date + ".tot", "r");
  if (t) {
    while (t.available()) out.write(t.read());
    t.close();
  } else {
    out.print("null");
  }
  out.print('}');
  return true;
}

void writeSummaryJson(int year, Print& out) {
  out.print("{\"year\":");
  out.print(year);
  out.print(",\"days\":[");
  File s = LittleFS.open("/sum/" + String(year) + ".jsonl", "r");
  bool first = true;
  if (s) {
    while (s.available()) {
      String line = s.readStringUntil('\n');
      line.trim();
      if (!line.length()) continue;
      if (!first) out.print(',');
      out.print(line);
      first = false;
    }
    s.close();
  }
  out.print("]}");
}

void pendingDone(const String& date) {
  LittleFS.remove("/p/" + date + ".bin");
  LittleFS.remove("/p/" + date + ".tot");
  LittleFS.remove("/p/" + date + ".fst");
}
