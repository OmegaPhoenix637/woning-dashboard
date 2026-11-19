#pragma once
#include <Arduino.h>

// Instellingen worden bewaard in NVS (flashgeheugen van de ESP32), nooit in de broncode.
// Elke instelling heeft een sleutel (max 15 tekens), label en groep voor de instelpagina.

enum FieldType : uint8_t { F_TEXT, F_SECRET, F_NUMBER, F_BOOL };

struct FieldDef {
  const char* key;
  const char* label;
  const char* group;
  FieldType type;
  const char* def;   // standaardwaarde
  const char* hint;  // uitleg op de instelpagina
};

extern const FieldDef FIELDS[];
extern const size_t N_FIELDS;

void settingsLoad();
String cfg(const char* key);
int cfgInt(const char* key, int def = 0);
bool cfgBool(const char* key);
bool cfgHas(const char* key);
bool cfgSet(const char* key, const String& value);  // direct opslaan (bv. nieuw refresh-token)
void settingsConsole();                             // instellen via USB
