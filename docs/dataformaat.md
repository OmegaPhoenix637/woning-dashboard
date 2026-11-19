# Dataformaat (contract ESP32 ↔ website)

De ESP32 schrijft JSON-bestanden naar de **privé** repo `woning-data`. De website leest ze via de GitHub API.
De TypeScript-definities staan in [`web/src/lib/types.ts`](../web/src/lib/types.ts) en zijn leidend.

## Indeling van de repo

| Branch | Bestand | Wanneer | Inhoud |
|---|---|---|---|
| `live` | `live.json` | elke 5 min, overschreven | huidige waarden + systeemstatus (`LiveData`) |
| `live` | `today.json` | elke 5 min, overschreven | vandaag tot nu toe, 5-minutenreeksen (`DayData`) |
| `main` | `days/2026/2026-11-19.json` | 1× per dag, na middernacht | complete dag (`DayData`) |
| `main` | `summary/2026.json` | 1× per dag | alle dagtotalen van het jaar (`YearSummary`) |
| `main` | `settings.json` | door de website | tarieven (`Settings`) |

De `live`-branch wordt elke keer **zonder geschiedenis** overschreven (commit zonder ouder + force-update van de ref).
Zo groeit de repo niet. `main` krijgt maar ~2 commits per dag.

## Tekenconventies

- `grid.w` / `grid_w`: **+ = afname** van het net, **− = teruglevering**
- batterij `w`: **+ = laden**, **− = ontladen**
- Huisverbruik wordt berekend: `net + zon − batterij1 − batterij2`
- Tijden: unix-seconden; datums `YYYY-MM-DD` in lokale tijd (Europe/Amsterdam)

## Kanalen in `DayData.ch` (5 minuten, `null` = geen data)

| Kanaal | Eenheid | Bron |
|---|---|---|
| `grid_w`, `grid_max` | W (gemiddelde, piek binnen 5 min) | HomeWizard P1 |
| `solar_w` | W | SMA SB3.0 + SB4.0 (samen) |
| `ev_w` | W | Laadpaal (als die uitleesbaar is) |
| `bat1_w`, `bat2_w`, `bat1_soc`, `bat2_soc` | W, % | Marstek 1/2 |
| `t_out`, `cloud` | °C, % | Open-Meteo |
| `ac_<id>_t`, `ac_<id>_set`, `ac_<id>_mode` | °C, °C, code | Daikin Onecta |
| `pellet_t`, `pellet_on` | °C, stand 0-5 | Nordic Fire |
| `eurom_<id>_t`, `eurom_<id>_on` | °C, 0/1 | Eurom |

Modus-codes: `0 uit, 1 verwarmen, 2 koelen, 3 auto, 4 drogen, 5 ventileren`.
Kamer-id's (7 airco's): `woonkamer, keuken, studio, slaap1, slaap2, slaap3, slaap4`.
Verwarming: `pellet, eurom_slaap, eurom_bad`. Apparaten: `wasmachine, vaatwasser`. Batterijen: `bat1, bat2`.
De echte namen staan alleen in de privé `settings.json` (`names`), niet in de openbare code.

## Dagtotalen (`DayTotals`)

`gasM3` (back-up gas, HomeWizard P1) en `evKwh` (laadpaal) staan er ook in.

Komen uit de **kWh-tellers** van de apparaten (verschil tussen 00:00 en 24:00), niet uit het optellen van wattages.
Dat is nauwkeuriger. Wasbeurten staan als `runs: [{id, start, end, program}]` in minuten na middernacht.
