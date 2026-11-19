#pragma once
#include <Arduino.h>

// Elke bron heeft één poll-functie die live-waarden bijwerkt en samples doorgeeft aan de sampler.
void pollP1();
void pollEv();
void pollSma();
void pollBatteries();
void pollOnecta();
void pollHomeConnect();
void pollAgua();
void pollEurom();
void pollWeather();

bool hcAnyRunning();
void resetDailyCalls();

// Koppelen via de instelpagina
String onectaAuthUrl();
bool onectaExchangeCode(const String& code, String& err);
bool hcDeviceStart(String& userCode, String& verifyUrl, String& err);
int hcDevicePoll(String& err);  // 1 = gekoppeld, 0 = wacht nog, -1 = fout
