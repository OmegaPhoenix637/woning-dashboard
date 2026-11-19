# Mijn Woning – energie- & klimaatdashboard

Alleen-lezen dashboard voor het hele huis: P1-meter (stroom + back-up gas), zonnepanelen (2× SMA Sunny Boy),
2× Marstek Venus E V3, 7 Daikin-airco's, pelletkachel, Eurom-kachels, Bosch/Siemens-witgoed en de laadpaal.
Werkt op pc en als app op het beginscherm van de iPhone.

```
apparaten ──► ESP32 (haalt data op) ──► privé GitHub-repo (opslag) ──► website op GitHub Pages
```

## Mappen

| Map | Wat |
|---|---|
| `web/` | De website (Svelte 5 + Vite + uPlot). Werkt meteen met demo-data. |
| `firmware/` | ESP32-code (volgt in fase 2) |
| `docs/dataformaat.md` | Welke JSON de ESP32 moet aanleveren |

## Website lokaal draaien

```bash
cd web
npm install
npm run dev        # open http://localhost:5173
npm test           # rekentests (tarieven/saldering)
npm run build      # productieversie in web/dist
node tools/screenshots.mjs   # screenshots in ../screenshots (na build, gebruikt Edge; niet in git)
```

Zonder koppeling toont de site **demo-data**: ruim 2 jaar realistische nepdata, live bewegend.

## Online zetten (GitHub)

- Code: https://github.com/OmegaPhoenix637/woning-dashboard (openbaar, zonder persoonlijke gegevens)
- Site: https://omegaphoenix637.github.io/woning-dashboard/
- Data: `OmegaPhoenix637/woning-data` (**privé**): live-data, dagbestanden, tarieven en de echte namen.

Site bijwerken na een wijziging: `cd web && npm run deploy` (bouwt, test en zet de site op de `gh-pages`-branch).

### Koppelcode maken (één keer)
1. GitHub → Settings → Developer settings → *Fine-grained tokens* → **Generate new token**.
2. Repository access: **Only select repositories** → `woning-data`.
3. Permissions → Repository → **Contents: Read and write**.
4. Open de site → Instellingen → Koppeling: gebruiker `OmegaPhoenix637`, repo `woning-data`, plak het token.
5. Daarna "Koppellink kopiëren" en die link op de iPhone openen: dan is de telefoon ook gekoppeld.

De ESP32 krijgt later een **eigen** token (ook alleen `woning-data`), zodat je die los kunt intrekken.
