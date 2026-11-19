#include "settings.h"
#include <Preferences.h>
#include <map>

// Alle instellingen. Volgorde = volgorde op de instelpagina.
const FieldDef FIELDS[] = {
    // Wifi
    {"wifi_ssid", "Wifi-netwerk", "Wifi", F_TEXT, "", "Naam van het thuisnetwerk (2,4 GHz)."},
    {"wifi_pass", "Wifi-wachtwoord", "Wifi", F_SECRET, "", ""},
    {"portal_pass", "Wachtwoord instelpagina", "Wifi", F_SECRET, "", "Optioneel. Gebruikersnaam is dan 'admin'."},
    // GitHub
    {"gh_owner", "GitHub-gebruiker", "GitHub (opslag)", F_TEXT, "OmegaPhoenix637", ""},
    {"gh_repo", "Data-repo (privé)", "GitHub (opslag)", F_TEXT, "woning-data", ""},
    {"gh_token", "Token", "GitHub (opslag)", F_SECRET, "",
     "Fine-grained token met alleen toegang tot de data-repo, Contents: Read and write."},
    // Lokaal
    {"p1_ip", "HomeWizard P1 – IP-adres", "Meters & omvormers (lokaal)", F_TEXT, "",
     "Zet in de HomeWizard-app de 'Lokale API' aan."},
    {"sma1_ip", "SMA SB3.0 – IP-adres", "Meters & omvormers (lokaal)", F_TEXT, "", "Modbus TCP moet aan staan op de omvormer."},
    {"sma2_ip", "SMA SB4.0 – IP-adres", "Meters & omvormers (lokaal)", F_TEXT, "", ""},
    {"sma_unit", "SMA Modbus unit-id", "Meters & omvormers (lokaal)", F_NUMBER, "3", "Standaard 3."},
    {"ev_ip", "Laadpaal: HomeWizard kWh-meter – IP", "Meters & omvormers (lokaal)", F_TEXT, "",
     "Optioneel. Leeg = laadpaal niet uitlezen."},
    // Batterijen
    {"bat1_ip", "Marstek 1 – IP-adres", "Batterijen (Marstek Venus E V3)", F_TEXT, "", ""},
    {"bat2_ip", "Marstek 2 – IP-adres", "Batterijen (Marstek Venus E V3)", F_TEXT, "", ""},
    {"bat_mode", "Verbinding", "Batterijen (Marstek Venus E V3)", F_NUMBER, "0",
     "0 = Modbus TCP (poort 502), 1 = Marstek Open API (UDP)."},
    {"bat_port", "Poort", "Batterijen (Marstek Venus E V3)", F_NUMBER, "502", "Modbus: 502. Open API: 30000."},
    {"bat_unit", "Modbus unit-id", "Batterijen (Marstek Venus E V3)", F_NUMBER, "1", ""},
    {"bat_invert", "Teken vermogen omdraaien", "Batterijen (Marstek Venus E V3)", F_BOOL, "0",
     "Aanzetten als 'laden' en 'ontladen' omgedraaid op de site staan."},
    // Daikin
    {"on_cid", "Client ID", "Daikin Onecta (airco's)", F_TEXT, "", "Van developer.cloud.daikineurope.com."},
    {"on_secret", "Client secret", "Daikin Onecta (airco's)", F_SECRET, "", ""},
    {"on_redirect", "Redirect URI", "Daikin Onecta (airco's)", F_TEXT,
     "https://omegaphoenix637.github.io/woning-dashboard/oauth.html", "Precies zo invullen bij Daikin."},
    {"on_refresh", "Refresh-token", "Daikin Onecta (airco's)", F_SECRET, "", "Wordt automatisch ingevuld na inloggen."},
    {"on_n0", "Naam in Onecta: woonkamer", "Daikin Onecta (airco's)", F_TEXT, "Kamer", ""},
    {"on_n1", "Naam in Onecta: keuken", "Daikin Onecta (airco's)", F_TEXT, "Keuken", ""},
    {"on_n2", "Naam in Onecta: studio", "Daikin Onecta (airco's)", F_TEXT, "Studio", ""},
    {"on_n3", "Naam in Onecta: slaapkamer 1", "Daikin Onecta (airco's)", F_TEXT, "Slaapkamer Fedde", ""},
    {"on_n4", "Naam in Onecta: slaapkamer 2", "Daikin Onecta (airco's)", F_TEXT, "Slaapkamer Jurre", ""},
    {"on_n5", "Naam in Onecta: slaapkamer 3", "Daikin Onecta (airco's)", F_TEXT, "Slaapkamer Remko", ""},
    {"on_n6", "Naam in Onecta: slaapkamer 4", "Daikin Onecta (airco's)", F_TEXT, "Slaapkamer C&R", ""},
    // Home Connect
    {"hc_cid", "Client ID", "Home Connect (wasmachine & vaatwasser)", F_TEXT, "",
     "Van developer.home-connect.com (type: Device Flow)."},
    {"hc_secret", "Client secret", "Home Connect (wasmachine & vaatwasser)", F_SECRET, "", "Optioneel."},
    {"hc_refresh", "Refresh-token", "Home Connect (wasmachine & vaatwasser)", F_SECRET, "", "Wordt automatisch ingevuld."},
    {"hc_washer", "haId wasmachine", "Home Connect (wasmachine & vaatwasser)", F_TEXT, "", "Wordt automatisch gevonden."},
    {"hc_dish", "haId vaatwasser", "Home Connect (wasmachine & vaatwasser)", F_TEXT, "", "Wordt automatisch gevonden."},
    // Nordic Fire
    {"ag_email", "E-mail (Nordic Fire 2.0-app)", "Nordic Fire pelletkachel (experimenteel)", F_TEXT, "", ""},
    {"ag_pass", "Wachtwoord", "Nordic Fire pelletkachel (experimenteel)", F_SECRET, "", ""},
    {"ag_url", "API-adres", "Nordic Fire pelletkachel (experimenteel)", F_TEXT, "https://air-api.agua-iot.com", ""},
    {"ag_cust", "Customer code", "Nordic Fire pelletkachel (experimenteel)", F_TEXT, "",
     "Van het merk Nordic Fire 2.0 (zie docs/firmware.md)."},
    {"ag_brand", "Brand id", "Nordic Fire pelletkachel (experimenteel)", F_TEXT, "1", ""},
    // Eurom / Tuya
    {"ty_region", "Tuya-regio", "Eurom (via Tuya, experimenteel)", F_TEXT, "eu", "eu, us of cn."},
    {"ty_cid", "Access ID", "Eurom (via Tuya, experimenteel)", F_TEXT, "", "Van iot.tuya.com (Cloud project)."},
    {"ty_secret", "Access secret", "Eurom (via Tuya, experimenteel)", F_SECRET, "", ""},
    {"ty_dev1", "Device-id Eurom slaapkamer", "Eurom (via Tuya, experimenteel)", F_TEXT, "", ""},
    {"ty_dev2", "Device-id Eurom badkamer", "Eurom (via Tuya, experimenteel)", F_TEXT, "", ""},
    // Weer
    {"lat", "Breedtegraad", "Weer (Open-Meteo)", F_TEXT, "", "Bijv. 50.90 (2 decimalen is genoeg)."},
    {"lon", "Lengtegraad", "Weer (Open-Meteo)", F_TEXT, "", "Bijv. 6.03"},
};
const size_t N_FIELDS = sizeof(FIELDS) / sizeof(FIELDS[0]);

static std::map<String, String> values;
static Preferences prefs;

static const FieldDef* findField(const char* key) {
  for (size_t i = 0; i < N_FIELDS; i++)
    if (strcmp(FIELDS[i].key, key) == 0) return &FIELDS[i];
  return nullptr;
}

void settingsLoad() {
  prefs.begin("mw", false);  // maakt de opslag aan bij de eerste start
  for (size_t i = 0; i < N_FIELDS; i++) {
    const FieldDef& f = FIELDS[i];
    values[f.key] = prefs.isKey(f.key) ? prefs.getString(f.key, f.def) : String(f.def);
  }
  prefs.end();
}

String cfg(const char* key) {
  auto it = values.find(key);
  if (it != values.end()) return it->second;
  const FieldDef* f = findField(key);
  return f ? String(f->def) : String();
}

int cfgInt(const char* key, int def) {
  String v = cfg(key);
  return v.length() ? v.toInt() : def;
}

bool cfgBool(const char* key) {
  String v = cfg(key);
  return v == "1" || v == "true" || v == "on";
}

bool cfgHas(const char* key) { return cfg(key).length() > 0; }

void cfgSet(const char* key, const String& value) {
  values[key] = value;
  prefs.begin("mw", false);
  prefs.putString(key, value);
  prefs.end();
}
