# ESP32-datalogger: stap voor stap

De ESP32 leest alle apparaten uit (alleen lezen, er wordt **niets** aangestuurd), maakt er
5-minutenwaarden van en zet die in de privé-repo `woning-data` op GitHub. De website leest daar.

Werk de stappen van boven naar beneden af. Na elke stap kun je op de instelpagina van de ESP
bij **Status** zien of het werkt (✓) of wat er mis is (✕).

---

## Stap 0 – Wat heb je nodig

- De ESP32 (al geflasht met de firmware).
- Een USB-voeding (telefoonlader, 5 V) en een plek met goede wifi.
- Een laptop of telefoon.
- Inloggegevens: router, GitHub, Daikin Onecta, Home Connect, Nordic Fire 2.0 en (optioneel) Smart Life.

---

## Stap 1 – ESP aan je wifi

1. Steek de ESP in de USB-voeding. Wacht 15 seconden.
2. Zoek op je telefoon of laptop naar wifi **Woning-Setup** en verbind. Wachtwoord: `woning1234`.
3. Open in de browser **http://192.168.4.1**.
4. Vul bij **Wifi** de naam en het wachtwoord van jullie wifi in.
   Let op: de ESP32 werkt alleen op **2,4 GHz**. Heeft je router twee netwerken (2,4 en 5 GHz),
   kies dan het 2,4 GHz-netwerk.
5. Klik onderaan op **Opslaan** en daarna bovenaan op **Herstarten**.
6. Verbind je telefoon/laptop weer met je eigen wifi en open **http://energie.local**.
   Werkt die naam niet (sommige Android-telefoons), zoek dan in de router het IP-adres van
   het apparaat **energie** en open `http://<dat-ip-adres>`.

> Tip: vul bij *Wifi* ook een **wachtwoord voor de instelpagina** in. Gebruikersnaam is dan `admin`.

---

## Stap 2 – Vaste IP-adressen in de router

De ESP praat met de apparaten via hun IP-adres. Zorg dat die nooit veranderen:

1. Log in op je router (meestal `http://192.168.1.1` of `http://192.168.178.1`, wachtwoord staat
   vaak op een sticker op de router).
2. Zoek de lijst met verbonden apparaten (*Apparaten*, *DHCP*, *LAN* of *Thuisnetwerk*).
3. Zoek en noteer de IP-adressen van:
   - **HomeWizard** P1-meter (heet vaak `p1meter-…` of `HW-P1…`)
   - **SMA** SB3.0 en SB4.0 (heten vaak `SMA…` gevolgd door het serienummer)
   - **Marstek** 1 en 2 (heten vaak `Marstek…` of `VenusE…`)
   - de **ESP** zelf (`energie`)
4. Zet bij elk apparaat **"Altijd hetzelfde IP-adres"** / **DHCP-reservering** aan.

---

## Stap 3 – GitHub (opslag van de data)

De ESP heeft een "sleutel" (token) nodig om data in de privé-repo te zetten.

1. Ga naar **github.com** en log in (OmegaPhoenix637).
2. Klik rechtsboven op je profielfoto → **Settings**.
3. Helemaal onderaan links: **Developer settings** → **Personal access tokens** → **Fine-grained tokens**.
4. Klik **Generate new token**.
   - *Token name*: `ESP32 woning`
   - *Expiration*: zo lang mogelijk (bijv. 1 jaar; zet een herinnering in je agenda)
   - *Repository access*: **Only select repositories** → kies **woning-data**
   - *Permissions* → *Repository permissions* → **Contents** → **Read and write**
5. Klik **Generate token** en kopieer de code (begint met `github_pat_`). Je ziet hem maar één keer!
6. Instelpagina ESP → **GitHub (opslag)**: gebruiker en repo staan al goed, plak het token → **Opslaan**.
7. Klik bovenaan op **Nu uploaden**. Bij Status moet *github* op ✓ komen.

---

## Stap 4 – HomeWizard P1 (stroom en gas)

1. Open de **HomeWizard Energy**-app.
2. Ga naar **Instellingen** (tandwiel) → **Meters** → kies de **P1-meter**.
3. Zet **Lokale API** aan.
4. Instelpagina ESP → **HomeWizard P1**: vul het IP-adres in (stap 2) → **Opslaan**.
5. Binnen 10 seconden staat *p1* op ✓.

> De P1-meter geeft alleen het **net** (afname/teruglevering) en **gas**. De zonne-opbrengst die je
> in de HomeWizard-app ziet, komt via de cloud van SMA en is niet uit HomeWizard op te halen.
> Daarom leest de ESP de zonnepanelen zelf uit (stap 5).

---

## Stap 5 – Zonnepanelen (SMA SB3.0 + SB4.0)

Er zijn vier manieren. **Kies manier 0**, dat lukt zonder installateur.

### Manier 0 – Inloggen als "Gebruiker" (aanbevolen)

Zo kijk je of je kunt inloggen:

1. Open op een laptop in je thuisnetwerk: `https://<IP-adres van de SB3.0>` (let op: **https**).
2. Je krijgt een waarschuwing over de verbinding. Klik **Geavanceerd** → **Doorgaan naar … (onveilig)**.
   Dat is normaal: de omvormer heeft geen officieel certificaat.
3. Kies taal **Nederlands**. Bij *Gebruikersgroep* kies **Gebruiker** (niet Installateur).
4. Vul het wachtwoord in en klik **Inloggen**. Zie je het startscherm met het vermogen? Dan werkt het.
5. Doe hetzelfde met de SB4.0 (vaak hetzelfde wachtwoord).

**Wachtwoord niet bekend?**
- Vaak heeft de installateur (Natuma) het ingesteld: vraag om het wachtwoord van de groep
  **Gebruiker**. Daar hoef je geen installateur voor te zijn.
- Of klik op het inlogscherm op **Wachtwoord vergeten?**. Je hebt dan de **PUK** of de
  **Device Key (DEV KEY)** nodig. Die staat op de sticker op de zijkant van de omvormer, of je
  vraagt de PUK aan bij SMA Service (met het serienummer).

Daarna in de ESP:
1. Instelpagina → **Zonnepanelen**: *Hoe uitlezen?* = `0`.
2. Vul beide IP-adressen in en het wachtwoord van **Gebruiker**.
   Is het wachtwoord van de SB4.0 anders? Vul dat in bij *Wachtwoord SB4.0*.
3. **Opslaan**. Binnen een minuut staat *sma* op ✓.

> "Te veel sessies"? De omvormer laat maar een paar gelijktijdige logins toe. Log in de browser uit
> en wacht 10 minuten.

### Manier 1 – Modbus TCP
Alleen als de installateur **Modbus TCP-server** aanzet (webinterface → *Apparaatparameters* →
*Externe communicatie* → *Modbus*). Kies dan `1`, IP-adressen invullen, unit-id `3`.

### Manier 2 – SMA-cloud (experimenteel)
Dezelfde bron als de SMA Energy-app en HomeWizard. Kan wat langer duren om goedgekeurd te worden.
1. Maak een account op **developer.sma.de** en vraag toegang tot de **Monitoring API**.
2. Maak een applicatie aan met Redirect URI:
   `https://omegaphoenix637.github.io/woning-dashboard/oauth.html`
3. Kies op de ESP `2`, vul Client ID en secret in → **Opslaan**.
4. Klik bovenaan bij *Koppelen* op **Inloggen bij SMA**, log in met je SMA-account (zelfde als de
   SMA Energy-app), kopieer de code die je ziet en plak hem bij *Plak de koppelcode* → **Koppelen**.

### Manier 3 – HomeWizard kWh-meter
Als er ooit een HomeWizard kWh-meter op de groep van de zonnepanelen komt: kies `3` en vul het
IP-adres in (lokale API aanzetten in de app, net als bij de P1).

---

## Stap 6 – Batterijen (2× Marstek Venus E V3)

1. Instelpagina → **Batterijen**: vul beide IP-adressen in (stap 2). Laat *Verbinding* op `0`
   (Modbus TCP, poort 502) → **Opslaan**.
2. Kijk na een minuut bij Status: staan *bat1* en *bat2* op ✓?
3. **Werkt het niet?** Zet in de **Marstek**-app per batterij de **Open API** / lokale API aan
   (bij de apparaatinstellingen). Zet op de ESP *Verbinding* op `1` en *Poort* op `30000`.
4. **Controleer 48 uur**: vergelijk het percentage en het vermogen op de website met de
   Marstek-app. Staat *laden* op de website als *ontladen* (andersom)? Zet dan
   **Teken vermogen omdraaien** aan.

---

## Stap 7 – Daikin airco's (Onecta)

1. Ga naar **developer.cloud.daikineurope.com** en log in met hetzelfde account als de
   **Onecta**-app (of maak een developer-account met dat e-mailadres).
2. Klik **Apps** → **Create app** (naam bijv. `Mijn Woning`).
3. Redirect URI (precies zo, zonder spatie):
   `https://omegaphoenix637.github.io/woning-dashboard/oauth.html`
4. Na het aanmaken zie je **Client ID** en **Client secret**. Kopieer ze.
5. Instelpagina ESP → **Daikin Onecta**: Client ID en secret plakken → **Opslaan**.
6. Bovenaan bij *Koppelen*: klik **1. Inloggen bij Daikin**. Er opent een Daikin-pagina: log in met je
   Onecta-account en geef toestemming.
7. Je komt op een pagina **"Koppelcode ontvangen"**. Klik **Kopiëren**.
8. Ga terug naar de instelpagina, plak de code bij **2. Plak de koppelcode** en klik **Koppelen**.
   Doe dit binnen een paar minuten, de code verloopt snel.
9. Binnen 10 minuten staat *onecta* op ✓. De 7 namen staan al goed (Kamer, Keuken, Studio,
   Slaapkamer Fedde/Jurre/Remko/C&R). Heet een airco in de app anders? Pas de naam hier aan.

> Daikin staat 200 verzoeken per dag toe; de ESP gebruikt er ±144 (elke 10 minuten).

---

## Stap 8 – Wasmachine en vaatwasser (Home Connect)

1. Ga naar **developer.home-connect.com** en log in met (of registreer) hetzelfde e-mailadres als
   de **Home Connect**-app.
2. Klik **Applications** → **Register Application**.
   - *Application ID*: `mijn-woning`
   - *OAuth Flow*: **Device Flow**
   - Laat de rest leeg / standaard → **Save**.
3. Kopieer de **Client ID**.
4. Instelpagina ESP → **Home Connect**: Client ID plakken → **Opslaan**.
5. Bovenaan bij *Koppelen*: klik **Koppel Home Connect**. Je krijgt een code en een knop
   **Open Home Connect**.
6. Log in met je Home Connect-account, controleer dat de code klopt en klik **Toestaan/Approve**.
7. De ESP-pagina meldt **✓ Gekoppeld**. Wasmachine en vaatwasser worden vanzelf gevonden.

---

## Stap 9 – Nordic Fire pelletkachel (experimenteel)

De Nordic Fire 2.0-app gebruikt het Micronova "Agua IOT"-platform (geen officiële API).
1. Instelpagina → **Nordic Fire**: e-mail en wachtwoord van de **Nordic Fire 2.0**-app.
2. *Customer code* en *Brand id*: zoek op internet naar de Home Assistant-integratie
   **"Micronova Agua IOT"**. In die integratie staat een lijst met merken; zoek **Nordic Fire 2.0**
   en neem daar de customer code en brand id over. *API-adres* laten staan, tenzij daar een ander
   adres bij staat.
3. **Opslaan**. Lukt het niet, dan staat de kachel op de site als "niet gekoppeld". Stuur me dan de
   foutmelding.

---

## Stap 10 – Eurom kachels (experimenteel)

Werkt alleen als de Eurom-kachels ook via **Tuya / Smart Life** werken.
1. Installeer de **Smart Life**-app en probeer een Eurom-kachel toe te voegen. Lukt dat niet,
   sla deze stap dan over.
2. Ga naar **iot.tuya.com**, maak een account → **Cloud** → **Create Cloud Project**
   (datacenter: **Central Europe**).
3. In het project: **Devices** → **Link App Account** → scan de QR-code met de Smart Life-app.
4. Kopieer bij *Overview* de **Access ID** en **Access Secret**, en bij *Devices* de
   **Device ID** van beide kachels.
5. Instelpagina → **Eurom**: regio `eu`, Access ID, secret en de twee device-id's → **Opslaan**.

---

## Stap 11 – Weer

1. Open Google Maps, klik met rechts op jullie huis en klik op de coördinaten (worden gekopieerd).
2. Instelpagina → **Weer**: eerste getal bij *Breedtegraad*, tweede bij *Lengtegraad*
   (2 decimalen is genoeg, bijv. `50.90` en `6.03`) → **Opslaan**.

---

## Stap 12 – De website koppelen

Doe dit pas als *github* op ✓ staat en er een paar minuten data is.

1. Maak op GitHub een **tweede** token (zoals stap 3), naam `Website`, ook alleen **woning-data**
   met **Contents: Read and write**.
2. Open de website: **https://omegaphoenix637.github.io/woning-dashboard/**
3. Ga naar **Instellingen** → **Koppeling**: gebruiker `OmegaPhoenix637`, repo `woning-data`,
   plak het token → **Koppelen**.
4. De site toont nu echte data in plaats van demodata.
5. Klik **Koppellink kopiëren** en stuur die link (bijv. via WhatsApp) naar de iPhone van je vader.
6. Op de iPhone: open de link in **Safari** → tik **Deel** → **Zet op beginscherm** → **Voeg toe**.

---

## Problemen oplossen

| Melding bij Status | Oplossing |
|---|---|
| `geen wifi` | ESP te ver van de router, of 5 GHz-netwerk ingevuld. |
| `geen verbinding met …` | IP-adres klopt niet of het apparaat staat uit. Controleer stap 2. |
| `wachtwoord klopt niet` (SMA) | Wachtwoord van groep **Gebruiker** controleren in de browser (stap 5). |
| `te veel sessies` (SMA) | 10 minuten wachten, niet tegelijk in de browser ingelogd blijven. |
| `nog niet gekoppeld` | De koppelstap (Daikin / Home Connect / SMA-cloud) nog uitvoeren. |
| `HTTP 401` bij GitHub | Token verlopen of verkeerd: maak een nieuw token (stap 3). |
| `geen airco's herkend` | Namen bij Daikin moeten exact gelijk zijn aan de Onecta-app. |

Meekijken met de laptop (USB): in de map `firmware`:
```bash
python -m platformio device monitor -p COM11
```

## Firmware bijwerken (USB)

```bash
cd firmware
python -m platformio run -t upload --upload-port COM11
```
Instellingen en opgeslagen data blijven bewaard. Alles wissen: `python -m platformio run -t erase`.

## Bekend

- Cloudverbindingen zijn versleuteld, maar certificaten worden nog niet gecontroleerd.
- Marstek-registers, SMA-cloud, Nordic Fire en Eurom zijn nog niet met de echte apparaten getest.
