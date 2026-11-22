# Dataformaat (contract ESP32 ↔ website)

De ESP32 schrijft JSON-bestanden naar de **privé** repo `woning-data`. De website leest ze via de GitHub API.
De TypeScript-definities staan in [`web/src/lib/types.ts`](../web/src/lib/types.ts) en zijn leidend.

## Indeling van de repo

| Branch | Bestand | Wanneer | Inhoud |
|---|---|---|---|
| `live` | `live.json` | elke ~20 s, overschreven | huidige waarden + systeemstatus (`LiveData`) |
| `live` | `today.json` | elke 5 min, overschreven | vandaag tot nu toe (`DayData`, zie hieronder) |
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

## Resolutie: `ch` per 5 minuten, `fast` per minuut

Sinds firmware 1.1.0 staan de vermogens per **minuut** in een apart blok:

```json
{ "date": "2026-11-22", "start": 1795302000, "step": 300, "n": 288,
  "ch":   { "t_out": [...288], "bat1_soc": [...], "ac_woonkamer_t": [...], ... },
  "fast": { "step": 60, "n": 1440,
            "ch": { "grid_w": [...1440], "grid_max": [...], "solar_w": [...], "bat1_w": [...], "bat2_w": [...],
                    "grid_l1": [...], "grid_l2": [...], "grid_l3": [...] } },
  "totals": { ... } }
```

Oudere dagen hebben geen `fast` en alles per 5 minuten in `ch`. De website zet beide om naar één
minutenraster en trekt korte gaten (≤ 10 min) door (`web/src/lib/data/normalize.ts`).

**Dagtotalen** (`totals`) komen uit de tellerstanden van de apparaten (P1, SMA, Marstek), niet uit de grafiek:
gaten in de grafiek maken de kWh dus niet minder. De beginstand van een dag is de eindstand van de vorige dag.

## Kanalen in `DayData.ch` / `fast.ch` (`null` = geen data)

| Kanaal | Eenheid | Bron |
|---|---|---|
| `grid_w`, `grid_max` | W (gemiddelde, piek binnen de minuut) | HomeWizard P1 |
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
Standaardnamen staan in de code (zoals in de apps); eigen namen in `settings.json` (`names`) gaan voor.

## Dagtotalen (`DayTotals`)

`gasM3` (back-up gas, HomeWizard P1) en `evKwh` (laadpaal) staan er ook in.

Komen uit de **kWh-tellers** van de apparaten (verschil tussen 00:00 en 24:00), niet uit het optellen van wattages.
Dat is nauwkeuriger. Wasbeurten staan als `runs: [{id, start, end, program}]` in minuten na middernacht.
