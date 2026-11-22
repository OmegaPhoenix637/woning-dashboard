#pragma once
// Mijn Woning – vaste instellingen van de firmware.
// Wachtwoorden, tokens en IP-adressen staan NIET hier maar in het flashgeheugen (NVS),
// in te stellen via de instelpagina http://energie.local

#define FW_VERSION "1.1.0"

// Tijd (Europe/Amsterdam) en tijdservers
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP1 "pool.ntp.org"
#define NTP2 "time.google.com"

// Opslag van de dag: trage kanalen (temperaturen e.d.) per 5 minuten,
// vermogens (net, zon, batterij, fasen) per minuut.
#define STEP_S 300
#define MAX_STEPS 300  // 288 normaal, 300 bij wintertijdwissel
#define FAST_STEP_S 60
#define MAX_FAST 1500  // 1440 normaal, 1500 bij wintertijdwissel

// Hoe vaak elke bron wordt uitgelezen (ms)
// P1, SMA en batterijen worden SAMEN uitgelezen (binnen ~1-2 s), zodat de waarden bij elkaar passen.
// Anders ontstaan schijnbare stromen, zoals "zon -> net" terwijl de batterij eigenlijk al laadt.
#define IV_LOCAL 10000UL
#define STALE_S 45  // waarden ouder dan dit worden niet meer als live getoond
#define IV_SMA_CLOUD 300000UL  // SMA-cloud: elke 5 min
#define IV_ONECTA 600000UL     // 144 calls/dag (limiet Daikin: 200)
#define IV_HC_IDLE 900000UL    // Home Connect zonder lopend programma
#define IV_HC_RUN 300000UL     // Home Connect als er iets draait
#define IV_AGUA 300000UL
#define IV_EUROM 300000UL
#define IV_WEATHER 1800000UL
// GitHub staat ~500 schrijfacties per uur toe.
//  - elke 10 s live.json bijwerken met 1 actie (Contents API)          = 360/uur
//  - elke 5 min live + today.json als nieuwe commit zonder geschiedenis = 4 acties -> 48/uur
#define IV_LIVE_UPLOAD 19000UL  // ~20 s (rustiger voor voeding en warmte; een upload duurt zelf 1-3 s)
#define IV_TODAY_UPLOAD 300UL  // seconden
#define IV_RETRY_DAYS 600000UL   // opnieuw proberen van achterstallige dagen

// Buffer: oude, al geüploade dagen worden lokaal opgeruimd. Niet-geüploade blijven tot het geheugen vol is.
#define MAX_PENDING_DAYS 80

// Instelpagina als er geen wifi is
#define AP_SSID "Woning-Setup"
#define AP_PASS "woning1234"
#define MDNS_NAME "energie"

// ---- Vaste id's (moeten overeenkomen met de website: web/src/lib/types.ts) ----
#define N_CLIMATE 7
static const char* const CLIMATE_IDS[N_CLIMATE] = {"woonkamer", "keuken", "studio", "slaap1", "slaap2", "slaap3", "slaap4"};
// Standaard-namen zoals in de Onecta-app (koppeling id ↔ Daikin-unit gaat via deze naam)
static const char* const CLIMATE_ONECTA_NAMES[N_CLIMATE] = {"Kamer", "Keuken", "Studio", "Slaapkamer Fedde",
                                                            "Slaapkamer Jurre", "Slaapkamer Remko", "Slaapkamer C&R"};

#define N_HEATER 3
static const char* const HEATER_IDS[N_HEATER] = {"pellet", "eurom_slaap", "eurom_bad"};
static const char* const HEATER_KINDS[N_HEATER] = {"pellet", "eurom", "eurom"};

#define N_APPL 2
static const char* const APPL_IDS[N_APPL] = {"wasmachine", "vaatwasser"};
static const char* const APPL_KINDS[N_APPL] = {"washer", "dishwasher"};

#define N_BAT 2
static const float BAT_CAPACITY_KWH = 5.12f;

// ---- Kanalen in de dagbestanden (5-min) ----
enum Agg : uint8_t { AGG_AVG, AGG_MAX, AGG_LAST };
enum ChIdx {
  CH_GRID_W, CH_GRID_MAX, CH_SOLAR_W, CH_BAT1_W, CH_BAT2_W, CH_BAT1_SOC, CH_BAT2_SOC, CH_T_OUT, CH_CLOUD,
  CH_AC_BASE,  // per airco: _t, _set, _mode
  CH_PELLET_T = CH_AC_BASE + N_CLIMATE * 3,
  CH_PELLET_ON,
  CH_EUROM_SLAAP_ON,
  CH_EUROM_BAD_ON,
  CH_EUROM_SLAAP_T,
  CH_EUROM_BAD_T,
  CH_L1,  // vermogen per fase (W, + afname / - teruglevering)
  CH_L2,
  CH_L3,
  N_CH
};

// Kanalen die per minuut worden opgeslagen
#define N_FAST 8
static const uint8_t FAST_CH[N_FAST] = {CH_GRID_W, CH_GRID_MAX, CH_SOLAR_W, CH_BAT1_W, CH_BAT2_W, CH_L1, CH_L2, CH_L3};
