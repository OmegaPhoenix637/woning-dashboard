# ESP32-datalogger – installeren en koppelen

De ESP32-WROOM-32 leest alle apparaten uit (alleen lezen, er wordt niets aangestuurd), maakt er
5-minutenwaarden van en zet die in de privé-repo `woning-data`. De website leest daar.

## Wat doet hij?

| Bron | Hoe | Hoe vaak |
|---|---|---|
| HomeWizard P1 (stroom + gas) | lokale API | elke 10 s |
| SMA SB3.0 + SB4.0 | Modbus TCP (poort 502) | elke 30 s |
| 2× Marstek Venus E V3 | Modbus TCP of Open API (UDP) | elke 30 s |
| Laadpaal | HomeWizard kWh-meter (optioneel) | elke 15 s |
| 7 Daikin-airco's | Onecta cloud (limiet 200/dag) | elke 10 min |
| Bosch + Siemens | Home Connect cloud (limiet 1000/dag) | 5–15 min |
| Nordic Fire | Agua IOT cloud (experimenteel) | elke 5 min |
| Eurom | Tuya cloud (experimenteel) | elke 5 min |
| Weer | Open-Meteo | elke 30 min |

- Elke 5 min: `live.json` + `today.json` naar de branch `live` (wordt steeds overschreven).
- Rond middernacht: de afgesloten dag naar `days/JJJJ/datum.json` + `summary/JJJJ.json` op `main`.
- Geen internet? De dagen blijven bewaard in het geheugen (± 2 maanden) en gaan later alsnog mee.
- Na een stroomstoring gaat hij verder waar hij was (de dag staat in het flashgeheugen).

## 1. Flashen (één keer, via USB)

1. Installeer [VS Code](https://code.visualstudio.com) met de extensie **PlatformIO IDE**
   (of `pip install platformio`).
2. Sluit de ESP32 met een USB-kabel aan.
3. In de map `firmware`:
   ```bash
   pio run -t upload        # bouwen en flashen
   pio device monitor       # meekijken (115200 baud)
   ```
4. Daarna mag hij aan een gewone 5 V-telefoonlader.

Updaten gaat op dezelfde manier. Instellingen en opgeslagen data blijven bewaard.
Alles wissen: `pio run -t erase`.

## 2. Eerste keer instellen

1. Verbind je telefoon of laptop met wifi **Woning-Setup** (wachtwoord `woning1234`).
2. Open **http://192.168.4.1**, vul bij *Wifi* het thuisnetwerk in (2,4 GHz) en sla op.
3. Kies **Herstarten**. Daarna is de instelpagina bereikbaar op **http://energie.local**
   (of via het IP-adres dat je in de router ziet).
4. Tip: geef de ESP32 en alle apparaten een vast IP-adres in de router (DHCP-reservering).

## 3. Koppelingen

### GitHub (opslag)
GitHub → Settings → Developer settings → *Fine-grained tokens* → **Generate new token**:
alleen repository `woning-data`, permissie **Contents: Read and write**. Plak hem bij *GitHub*.
Gebruik een **ander** token dan dat van de website, dan kun je ze los intrekken.

### HomeWizard P1
HomeWizard-app → Instellingen → Meters → P1-meter → **Lokale API** aanzetten. IP invullen.

### SMA (twee omvormers)
Modbus TCP moet aan staan: webinterface van de omvormer → *Apparaatparameters* →
*Externe communicatie* → **Modbus TCP-server** aan (installateursrechten nodig, eventueel
via de installateur). Unit-id is standaard **3**. Vul beide IP-adressen in.

### Marstek Venus E V3
Beide batterijen via ethernet. Standaard **Modbus TCP** (poort 502). Werkt dat niet, zet dan in
de Marstek-app de *Open API* aan en kies verbinding `1` met poort `30000`.
**Controleer 48 uur** of SoC en vermogen kloppen met de Marstek-app. Staan laden en ontladen
omgedraaid? Zet dan *Teken vermogen omdraaien* aan. De registers staan bovenin
`firmware/src/sources/local.cpp` (vergelijk met de marstek-modbus-suite).

### Laadpaal (optioneel)
Het betrouwbaarst is een **HomeWizard kWh-meter (3-fase)** op de groep van de laadpaal
(installateur). Lokale API aan, IP invullen. Leeg laten = laadpaal niet uitlezen.

### Daikin Onecta (7 airco's)
1. Maak een account op **developer.cloud.daikineurope.com** en maak een app aan.
2. Redirect URI: `https://omegaphoenix637.github.io/woning-dashboard/oauth.html`
3. Vul Client ID en secret in op de instelpagina en sla op.
4. Klik **Inloggen bij Daikin**, log in met het Onecta-account. Je komt op een pagina met een
   code: kopieer die, plak hem bij *Plak de koppelcode* en klik **Koppelen** (binnen een paar minuten).
5. De namen moeten precies zo heten als in de Onecta-app (staan al goed ingevuld).

### Home Connect (Bosch wasmachine, Siemens vaatwasser)
1. Account op **developer.home-connect.com** → *Register application*, OAuth-flow **Device Flow**.
2. Client ID invullen, opslaan, **Koppel Home Connect** klikken.
3. Open de link, log in met het Home Connect-account en bevestig. De apparaten worden vanzelf gevonden.

### Nordic Fire pelletkachel (experimenteel)
De Nordic Fire 2.0-app gebruikt het Micronova *Agua IOT*-platform (geen officiële API).
Vul e-mail en wachtwoord van de app in, plus de *customer code* en *brand id* van het merk
Nordic Fire 2.0. Die staan in de merkenlijst van de open-source Home Assistant-integratie
"Micronova Agua IOT". Werkt het niet, dan blijft de kachel op de site "niet gekoppeld".

### Eurom (experimenteel)
Alleen als de Eurom-kachels ook in de **Smart Life / Tuya**-app werken. Dan: project op
**iot.tuya.com** (datacenter Europa), Smart Life-account koppelen, *Access ID/Secret* en de
twee *device-id's* invullen.

### Weer
Breedte- en lengtegraad van het huis (2 decimalen is genoeg).

## Status en problemen

- De instelpagina toont per bron ✓ of ✕ met de foutmelding, plus het aantal API-calls.
- Op de website: **Instellingen → Systeemstatus**.
- `pio device monitor` laat precies zien wat er gebeurt.

## Bekend

- Verbindingen naar de cloud zijn versleuteld, maar certificaten worden nog niet gecontroleerd.
- Marstek-registers, Nordic Fire en Eurom zijn nog niet met de echte apparaten getest.
