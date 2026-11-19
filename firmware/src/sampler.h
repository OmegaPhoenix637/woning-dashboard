#pragma once
#include <Arduino.h>
#include "config.h"

// De sampler maakt van losse metingen 5-minutenwaarden, houdt de dag bij (ook na een herstart,
// via LittleFS) en maakt bij middernacht een afgesloten dag klaar voor upload.

void samplerBegin();
void samplerAdd(int ch, float v);
void samplerTick();  // aanroepen vanuit loop()
void samplerSetClimateKwh(int idx, float kwh);
void samplerRunStart(int appl, const char* program);
void samplerRunEnd(int appl);

const char* chName(int ch, char* buf, size_t len);

// Uitvoer voor GitHub (JSON, gestreamd naar een Print)
void writeTodayJson(Print& out);
bool pendingOldest(String& date);  // oudste nog niet geüploade dag
int pendingCount();
bool writePendingDayJson(const String& date, Print& out);
void writeSummaryJson(int year, Print& out);
void pendingDone(const String& date);
String todayDate();
