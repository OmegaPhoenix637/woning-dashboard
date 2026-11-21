#include "state.h"
#include <time.h>

LiveState live;
char g_lastReset[64] = "";
uint32_t g_boots = 0;
String g_resetHist = "[]";

uint32_t nowUnix() { return (uint32_t)time(nullptr); }

bool timeValid() { return time(nullptr) > 1700000000; }

void markOk(SrcId s) {
  live.health[s].enabled = true;
  live.health[s].ok = true;
  live.health[s].last = nowUnix();
  live.health[s].err[0] = 0;
}

void markErr(SrcId s, const String& err) {
  live.health[s].enabled = true;
  live.health[s].ok = false;
  strlcpy(live.health[s].err, err.c_str(), sizeof(live.health[s].err));
  Serial.printf("[%s] fout: %s\n", SRC_KEYS[s], err.c_str());
}
