#pragma once
#include <Arduino.h>
#include "config.h"

// Actuele waarden van alle bronnen (wordt elke 5 min als live.json geüpload).

enum SrcId { S_P1, S_SMA, S_BAT1, S_BAT2, S_ONECTA, S_HC, S_AGUA, S_EUROM, S_WEATHER, S_GITHUB, N_SRC };
static const char* const SRC_KEYS[N_SRC] = {"p1", "sma", "bat1", "bat2", "onecta", "homeconnect",
                                            "aguaiot", "eurom", "weather", "github"};

struct SrcHealth {
  bool enabled = false;
  bool ok = false;
  uint32_t last = 0;  // unix-tijd van laatste succes
  char err[64] = "";
  uint16_t calls = 0;  // API-calls vandaag
  uint16_t limit = 0;
};

// Heater states: 0 off, 1 starting, 2 on, 3 cooling, 4 unknown
// Appliance states: 0 off, 1 idle, 2 running, 3 paused, 4 finished
struct LiveState {
  // Net (HomeWizard P1)
  float gridW = NAN, l1 = NAN, l2 = NAN, l3 = NAN, voltage = NAN;
  double importKwh = NAN, exportKwh = NAN, gasM3 = NAN;
  // Zon (2x SMA)
  float smaW[2] = {NAN, NAN};
  double smaTotalKwh[2] = {NAN, NAN};
  float solarW = NAN;
  double solarTotalKwh = NAN;
  // Batterijen (+ = laden)
  float batSoc[N_BAT] = {NAN, NAN};
  float batW[N_BAT] = {NAN, NAN};
  double batChgKwh[N_BAT] = {NAN, NAN};  // tellers (als het apparaat ze geeft)
  double batDisKwh[N_BAT] = {NAN, NAN};
  // Airco's
  struct Ac {
    bool seen = false;
    float temp = NAN, target = NAN, kwhToday = NAN;
    uint8_t mode = 0;  // 0 off 1 heat 2 cool 3 auto 4 dry 5 fan
  } ac[N_CLIMATE];
  // Kachels
  struct Heater {
    bool seen = false;
    uint8_t state = 4;
    float temp = NAN, target = NAN;
    int8_t level = -1;
  } heater[N_HEATER];
  // Witgoed
  struct Appl {
    bool seen = false;
    uint8_t state = 0;
    char program[48] = "";
    int16_t remainingMin = -1;
    int8_t progress = -1;
  } appl[N_APPL];
  // Weer
  float tOut = NAN, cloud = NAN;

  SrcHealth health[N_SRC];
};

extern LiveState live;

uint32_t nowUnix();
bool timeValid();
void markOk(SrcId s);
void markErr(SrcId s, const String& err);
