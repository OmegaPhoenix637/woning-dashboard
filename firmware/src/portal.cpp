// Instelpagina op het thuisnetwerk: http://energie.local (of het IP-adres van de ESP32).
#include "portal.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include "jsonout.h"
#include "sampler.h"
#include "settings.h"
#include "sources/sources.h"
#include "state.h"

String wifiDiagnose();

static WebServer server(80);
static bool uploadNow = false;
static String lastSaved;   // wat er bij de laatste keer opslaan is weggeschreven
static bool lastSaveOk = true;

bool portalWantsUpload() {
  bool u = uploadNow;
  uploadNow = false;
  return u;
}

static bool auth() {
  String p = cfg("portal_pass");
  if (!p.length()) return true;
  if (server.authenticate("admin", p.c_str())) return true;
  server.requestAuthentication();
  return false;
}

static String esc(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else if (c == '&') o += "&amp;";
    else o += c;
  }
  return o;
}

static const char CSS[] PROGMEM = R"CSS(
:root{color-scheme:dark}*{box-sizing:border-box}
body{margin:0;background:#000;color:#f5f5f7;font:16px/1.5 -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;padding:16px}
main{max-width:760px;margin:0 auto}h1{font-size:32px;margin:8px 0 2px;letter-spacing:-.02em}h2{font-size:19px;margin:0 0 10px}
p.sub{color:#a1a1a6;margin:0 0 18px}section{background:#1c1c1e;border-radius:20px;padding:18px;margin:14px 0}
label{display:block;font-size:13px;font-weight:600;color:#a1a1a6;margin:12px 0 4px}
input{width:100%;height:44px;border-radius:12px;border:1px solid #3a3a3c;background:#0b0b0d;color:#fff;padding:0 12px;font:inherit}
input[type=checkbox]{width:auto;height:auto}small{display:block;color:#86868b;font-size:12.5px;margin-top:3px}
.btn{display:inline-flex;align-items:center;justify-content:center;height:44px;padding:0 18px;border-radius:999px;border:0;background:#2c2c2e;color:#fff;font:inherit;font-weight:600;text-decoration:none;cursor:pointer;margin:6px 6px 0 0}
.btn.p{background:#3d8ef0}.row{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
table{width:100%;border-collapse:collapse;font-size:14px}td{padding:7px 4px;border-bottom:1px solid #2c2c2e}td:last-child{text-align:right;color:#a1a1a6}
.ok{color:#3ccf5e}.bad{color:#ec835a}.note{background:#2c2c2e;border-radius:14px;padding:12px;margin-top:10px}
code{background:#0b0b0d;padding:2px 6px;border-radius:6px;word-break:break-all}
.sticky{position:fixed;left:12px;right:12px;bottom:12px;max-width:736px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;gap:10px;background:#3a2f0b;border:1px solid #fab219;border-radius:18px;padding:8px 8px 8px 16px;font-weight:600}.sticky[hidden]{display:none}main{padding-bottom:90px}
details{background:#2c2c2e;border-radius:14px;padding:10px 14px;margin:0 0 6px}summary{cursor:pointer;font-weight:600;color:#74b3ff}
.help{color:#c7c7cc;font-size:14.5px}.help ol,.steps{padding-left:20px;margin:8px 0}.help li,.steps li{margin:5px 0}a{color:#74b3ff}
)CSS";


// Uitleg per onderdeel (uitklapbaar op de instelpagina). Zie ook docs/firmware.md.
static const char* groupHelp(const String& g) {
  if (g.startsWith("Wifi"))
    return "<ol><li>Vul de naam en het wachtwoord van jullie wifi in. Alleen <b>2,4 GHz</b> werkt.</li>"
           "<li>Klik onderaan op <b>Opslaan</b> en daarna bovenaan op <b>Herstarten</b>.</li>"
           "<li>Verbind weer met je eigen wifi en open <b>http://energie.local</b> (of het IP-adres uit de router).</li>"
           "<li>Optioneel: een wachtwoord voor deze pagina (gebruikersnaam is dan <code>admin</code>).</li></ol>";
  if (g.startsWith("GitHub"))
    return "<ol><li>github.com &rarr; profielfoto &rarr; <b>Settings</b> &rarr; <b>Developer settings</b> &rarr; <b>Fine-grained tokens</b> &rarr; <b>Generate new token</b>.</li>"
           "<li>Naam <code>ESP32 woning</code>, verloopt over 1 jaar.</li>"
           "<li>Repository access: <b>Only select repositories</b> &rarr; <b>woning-data</b>.</li>"
           "<li>Permissions &rarr; <b>Contents</b> &rarr; <b>Read and write</b> &rarr; <b>Generate token</b>.</li>"
           "<li>Kopieer de code (begint met <code>github_pat_</code>), plak hem hieronder, <b>Opslaan</b>, daarna bovenaan <b>Nu uploaden</b>.</li></ol>";
  if (g.startsWith("HomeWizard"))
    return "<ol><li>HomeWizard Energy-app &rarr; <b>Instellingen</b> &rarr; <b>Meters</b> &rarr; P1-meter &rarr; <b>Lokale API</b> aan.</li>"
           "<li>Zoek het IP-adres van de P1-meter in je router (en zet hem op een vast IP).</li>"
           "<li>Vul het IP-adres in en klik <b>Opslaan</b>.</li></ol>"
           "<p>De P1 geeft alleen net en gas. De zonnestroom in de HomeWizard-app komt uit de SMA-cloud; die leest de ESP hieronder zelf uit.</p>";
  if (g.startsWith("Zonnepanelen"))
    return "<p><b>Kies 0</b> (geen installateur nodig):</p><ol>"
           "<li>Open op een laptop <code>https://&lt;IP van de SB3.0&gt;</code> &rarr; bij de waarschuwing <b>Geavanceerd</b> &rarr; <b>Doorgaan</b>.</li>"
           "<li>Gebruikersgroep <b>Gebruiker</b> (niet Installateur), wachtwoord invullen. Zie je het vermogen? Dan werkt het wachtwoord.</li>"
           "<li>Wachtwoord onbekend? Vraag Natuma om het wachtwoord van groep <b>Gebruiker</b>, of gebruik <b>Wachtwoord vergeten</b> met de PUK/DEV KEY van de sticker op de omvormer.</li>"
           "<li>Hieronder: <i>Hoe uitlezen</i> = 0, beide IP-adressen en het wachtwoord invullen, <b>Opslaan</b>.</li></ol>"
           "<p>Keuze 2 (SMA-cloud): account op developer.sma.de, app met dezelfde redirect-URI als Daikin, dan bovenaan <b>Inloggen bij SMA</b>.</p>";
  if (g.startsWith("Batterijen"))
    return "<ol><li>Vul de IP-adressen van beide Marstek-batterijen in (uit de router) en klik <b>Opslaan</b>.</li>"
           "<li>Geen vinkje bij bat1/bat2? Zet in de Marstek-app de <b>Open API</b> aan en kies hier verbinding <b>1</b> en poort <b>30000</b>.</li>"
           "<li>Vergelijk 2 dagen met de Marstek-app. Laden/ontladen omgedraaid? Vink <b>Teken vermogen omdraaien</b> aan.</li></ol>";
  if (g.startsWith("Daikin"))
    return "<ol><li>Ga naar <b>developer.cloud.daikineurope.com</b>, log in met het Onecta-account &rarr; <b>Apps</b> &rarr; <b>Create app</b>.</li>"
           "<li>Redirect URI precies: <code>https://omegaphoenix637.github.io/woning-dashboard/oauth.html</code></li>"
           "<li>Kopieer <b>Client ID</b> en <b>Client secret</b> naar hieronder &rarr; <b>Opslaan</b>.</li>"
           "<li>Bovenaan bij <i>Koppelen</i>: <b>Inloggen bij Daikin</b> &rarr; inloggen &rarr; toestaan &rarr; op de pagina met de code <b>Kopi&euml;ren</b>.</li>"
           "<li>Plak de code bij <b>Plak de koppelcode</b> &rarr; <b>Koppelen</b> (binnen een paar minuten).</li>"
           "<li>De namen hieronder moeten precies zo heten als in de Onecta-app.</li></ol>";
  if (g.startsWith("Home Connect"))
    return "<ol><li>Ga naar <b>developer.home-connect.com</b> (zelfde e-mail als de Home Connect-app) &rarr; <b>Applications</b> &rarr; <b>Register Application</b>.</li>"
           "<li>Application ID <code>mijn-woning</code>, OAuth Flow <b>Device Flow</b> &rarr; <b>Save</b>.</li>"
           "<li>Kopieer de <b>Client ID</b> naar hieronder &rarr; <b>Opslaan</b>.</li>"
           "<li>Bovenaan: <b>Koppel Home Connect</b> &rarr; <b>Open Home Connect</b> &rarr; inloggen &rarr; code bevestigen &rarr; <b>Approve</b>.</li></ol>";
  if (g.startsWith("Nordic"))
    return "<ol><li>Vul e-mail en wachtwoord van de <b>Nordic Fire 2.0</b>-app in.</li>"
           "<li>Customer code en brand id: zoek de Home Assistant-integratie <b>Micronova Agua IOT</b>; daarin staat per merk (Nordic Fire 2.0) de customer code en brand id.</li>"
           "<li><b>Opslaan</b>. Experimenteel: geef de foutmelding door als het niet lukt.</li></ol>";
  if (g.startsWith("Eurom"))
    return "<ol><li>Werkt de Eurom-kachel ook in de <b>Smart Life</b>-app? Zo niet: sla dit over.</li>"
           "<li><b>iot.tuya.com</b> &rarr; Cloud &rarr; <b>Create Cloud Project</b> (datacenter Central Europe).</li>"
           "<li>Devices &rarr; <b>Link App Account</b> &rarr; QR-code scannen met Smart Life.</li>"
           "<li>Kopieer <b>Access ID</b>, <b>Access Secret</b> en de twee <b>Device ID</b>s naar hieronder &rarr; <b>Opslaan</b>.</li></ol>";
  if (g.startsWith("Weer"))
    return "<ol><li>Google Maps &rarr; rechtermuisklik op het huis &rarr; klik op de co&ouml;rdinaten (gekopieerd).</li>"
           "<li>Eerste getal = breedtegraad, tweede = lengtegraad. 2 decimalen is genoeg &rarr; <b>Opslaan</b>.</li></ol>";
  return "";
}

static void sendPage(const String& title, const String& body) {
  String h;
  h.reserve(body.length() + 2500);
  h += F("<!doctype html><html lang=nl><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>");
  h += title;
  h += F("</title><style>");
  h += FPSTR(CSS);
  h += F("</style></head><body><main>");
  h += body;
  h += F("</main></body></html>");
  server.send(200, "text/html; charset=utf-8", h);
}

static void handleRoot() {
  if (!auth()) return;
  String b;
  b += F("<h1>Mijn Woning · ESP32</h1><p class=sub>Instellingen van de datalogger. Alleen lezen: er wordt niets aangestuurd.</p>");
  if (server.hasArg("saved")) {
    if (!lastSaveOk)
      b += F("<div class=note style='background:#4a1f1f'>✕ Opslaan is (deels) mislukt. Probeer opnieuw, of stel in via de USB-kabel (zie docs/firmware.md).</div>");
    else if (lastSaved.length())
      b += "<div class=note>✓ Opgeslagen: " + esc(lastSaved) + ".<br>Wifi-wijzigingen werken pas na <b>Herstarten</b>.</div>";
    else
      b += F("<div class=note>Er was niets gewijzigd.</div>");
  }
  // Wifi-status in gewone taal
  if (WiFi.status() == WL_CONNECTED)
    b += "<div class=note>✓ Verbonden met wifi <b>" + esc(WiFi.SSID()) + "</b> · IP-adres <b>" + WiFi.localIP().toString() + "</b></div>";
  else if (cfgHas("wifi_ssid"))
    b += "<div class=note style='background:#4a3a14'>Nog niet verbonden met <b>" + esc(cfg("wifi_ssid")) +
         "</b>. Klopt het wachtwoord (hoofdlettergevoelig) en is het een 2,4 GHz-netwerk? Na opslaan: <b>Herstarten</b>.<br>" +
         wifiDiagnose() + "</div>";
  else
    b += F("<div class=note style='background:#4a3a14'>Instelmodus: vul hieronder bij <b>Wifi</b> jullie netwerk in, klik <b>Opslaan</b> en daarna <b>Herstarten</b>.</div>");
  b += F("<section><h2>Stappenplan</h2><ol class=steps>"
         "<li><a href='#s0'>Wifi</a> instellen en herstarten</li>"
         "<li>In de router alle apparaten een <b>vast IP-adres</b> geven en de IP-adressen noteren</li>"
         "<li><a href='#s1'>GitHub-token</a> invullen, daarna bovenaan <b>Nu uploaden</b></li>"
         "<li><a href='#s2'>HomeWizard P1</a></li>"
         "<li><a href='#s3'>Zonnepanelen (SMA)</a></li>"
         "<li><a href='#s4'>Batterijen (Marstek)</a></li>"
         "<li><a href='#s5'>Daikin</a> en <a href='#s6'>Home Connect</a>, daarna koppelen (hieronder)</li>"
         "<li><a href='#s7'>Nordic Fire</a> en <a href='#s8'>Eurom</a> (experimenteel)</li>"
         "<li><a href='#s9'>Weer</a></li>"
         "<li>Op de website: Instellingen &rarr; Koppeling (met een tweede GitHub-token)</li></ol>"
         "<p class=sub style='margin:8px 0 0'>Bij elk onderdeel staat <b>Hoe doe ik dit?</b>. Bij <b>Status</b> zie je per onderdeel of het werkt.</p></section>");
  b += F("<section><h2>Status</h2><table id=st><tr><td>Laden…</td></tr></table><div class=row>"
         "<form method=post action=/upload onsubmit=\"return act('upload')\"><button class=btn>Nu uploaden</button></form>"
         "<form method=post action=/restart onsubmit=\"return act('restart')\"><button class=btn>Herstarten</button></form>"
         "<a class=btn href=/api/live target=_blank>live.json</a></div></section>");
  // Koppelen
  b += F("<section><h2>Koppelen</h2>"
         "<p><b>Daikin Onecta</b>: vul hieronder Client ID, secret en Redirect URI in en sla op. Daarna:</p>"
         "<div class=row><a class='btn p' href=/onecta/login target=_blank>1. Inloggen bij Daikin</a></div>"
         "<form method=post action=/onecta/code><label>2. Plak de koppelcode</label><input name=code required>"
         "<button class='btn p'>Koppelen</button></form>"
         "<p style='margin-top:18px'><b>SMA-cloud</b> (alleen bij keuze 2): Client ID en secret invullen en opslaan. Daarna:</p>"
         "<div class=row><a class='btn p' href=/sma/login target=_blank>1. Inloggen bij SMA</a></div>"
         "<form method=post action=/sma/code><label>2. Plak de koppelcode</label><input name=code required>"
         "<button class='btn p'>Koppelen</button></form>"
         "<p style='margin-top:18px'><b>Home Connect</b> (Bosch/Siemens): vul de Client ID in en sla op. Daarna:</p>"
         "<form method=post action=/hc/start><button class='btn p'>Koppel Home Connect</button></form></section>");
  // Formulier
  b += F("<form id=cfg method=post action=/save><input type=hidden name=then id=then value=''>");
  String group;
  int gi = 0;
  for (size_t i = 0; i < N_FIELDS; i++) {
    const FieldDef& f = FIELDS[i];
    if (group != f.group) {
      if (group.length()) b += F("<button class='btn p'>Opslaan</button></section>");
      group = f.group;
      b += "<section id='s" + String(gi++) + "'><h2>" + esc(group) + "</h2>";
      const char* h = groupHelp(group);
      if (h[0]) b += String("<details><summary>Hoe doe ik dit?</summary><div class=help>") + h + "</div></details>";
    }
    String v = cfg(f.key);
    b += "<label for='" + String(f.key) + "'>" + esc(f.label) + "</label>";
    if (f.type == F_BOOL) {
      b += "<input type=checkbox id='" + String(f.key) + "' name='" + String(f.key) + "' value=1" + (v == "1" ? " checked" : "") + ">";
      b += "<input type=hidden name='" + String(f.key) + "__b' value=1>";
    } else if (f.type == F_SECRET) {
      b += "<input type=password id='" + String(f.key) + "' name='" + String(f.key) + "' placeholder='" +
           (v.length() ? "•••••• (ingesteld, leeg laten = niet wijzigen)" : "nog niet ingesteld") + "' autocomplete=off>";
    } else {
      b += "<input id='" + String(f.key) + "' name='" + String(f.key) + "' value='" + esc(v) + "'" +
           (f.type == F_NUMBER ? " inputmode=numeric" : "") + ">";
    }
    if (f.hint && f.hint[0]) b += "<small>" + esc(f.hint) + "</small>";
  }
  b += F("<button class='btn p'>Opslaan</button></section></form>"
         "<div id=dirty class=sticky hidden><span>Je hebt wijzigingen die nog niet zijn opgeslagen</span>"
         "<button class='btn p' form=cfg>Opslaan</button></div>"
         "<script>let dirty=false;const f=document.getElementById('cfg');"
         "f.addEventListener('input',()=>{dirty=true;document.getElementById('dirty').hidden=false});"
         "f.addEventListener('submit',()=>{dirty=false});"
         "window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue=''}});"
         "function act(a){if(!dirty)return true;document.getElementById('then').value=a;dirty=false;f.submit();return false}"
         "</script>");
  b += F("<script>"
         "async function s(){try{const d=await (await fetch('/api/live')).json();const h=d.health;let r='';"
         "r+=`<tr><td>Firmware</td><td>${h.fw} · ${Math.round(h.uptime/3600)} u aan · ${Math.round(h.heap/1024)} KB vrij · wifi ${h.rssi} dBm</td></tr>`;"
         "r+=`<tr><td>Wachtende dagen</td><td>${h.pending}</td></tr>`;"
         "if(!Object.keys(h.sources).length)r+=`<tr><td colspan=2>Nog geen apparaten actief. Eerst wifi instellen, daarna starten ze vanzelf.</td></tr>`;"
         "for(const[k,v]of Object.entries(h.sources)){const age=Math.round((Date.now()/1000-v.last)/60);"
         "r+=`<tr><td>${k}</td><td class=${v.ok?'ok':'bad'}>${v.ok?'✓ werkt':'✕ '+(v.err||'fout')}${v.last?' · '+age+' min geleden':''}${v.limit?' · '+v.calls+'/'+v.limit:''}</td></tr>`}"
         "document.getElementById('st').innerHTML=r}catch(e){}}s();setInterval(s,10000)</script>");
  sendPage("Mijn Woning ESP32", b);
}


static void handleSave() {
  if (!auth()) return;
  Serial.printf("[instellingen] formulier ontvangen: %d velden\n", server.args());
  lastSaved = "";
  lastSaveOk = true;
  int found = 0;
  for (size_t i = 0; i < N_FIELDS; i++) {
    const FieldDef& f = FIELDS[i];
    if (f.type == F_BOOL) {
      if (!server.hasArg(String(f.key) + "__b")) continue;
      found++;
      String v = server.hasArg(f.key) ? "1" : "0";
      if (v != cfg(f.key)) {
        lastSaveOk &= cfgSet(f.key, v);
        lastSaved += String(lastSaved.length() ? ", " : "") + f.label;
      }
      continue;
    }
    if (!server.hasArg(f.key)) continue;
    found++;
    String v = server.arg(f.key);
    v.trim();
    if (f.type == F_SECRET && !v.length()) continue;  // leeg = niet wijzigen
    if (v != cfg(f.key)) {
      lastSaveOk &= cfgSet(f.key, v);
      lastSaved += String(lastSaved.length() ? ", " : "") + f.label;
    }
  }
  Serial.printf("[instellingen] %d van %d velden herkend\n", found, (int)N_FIELDS);
  if (!found) lastSaveOk = false;
  String then = server.arg("then");
  if (then == "restart") {
    sendPage("Herstarten", "<h1>Opgeslagen en herstarten…</h1><p class=sub>Over ~20 seconden opnieuw openen. Is de wifi ingesteld, verbind dan weer met je eigen wifi en open <b>http://energie.local</b>.</p>");
    delay(600);
    ESP.restart();
  }
  if (then == "upload") uploadNow = true;
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
}

class StringPrint : public Print {
 public:
  String s;
  size_t write(uint8_t c) override {
    s += (char)c;
    return 1;
  }
};

void portalBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/live", HTTP_GET, []() {
    StringPrint p;
    p.s.reserve(4096);
    writeLiveJson(p);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", p.s);
  });
  server.on("/upload", HTTP_POST, []() {
    if (!auth()) return;
    uploadNow = true;
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/restart", HTTP_POST, []() {
    if (!auth()) return;
    sendPage("Herstarten", "<h1>Herstarten…</h1><p class=sub>Over ~15 seconden is de ESP32 weer bereikbaar.</p>");
    delay(500);
    ESP.restart();
  });
  server.on("/onecta/login", HTTP_GET, []() {
    if (!auth()) return;
    server.sendHeader("Location", onectaAuthUrl());
    server.send(302);
  });
  server.on("/onecta/code", HTTP_POST, []() {
    if (!auth()) return;
    String err;
    bool ok = onectaExchangeCode(server.arg("code"), err);
    sendPage("Daikin", ok ? "<h1>✓ Daikin gekoppeld</h1><p class=sub>De airco's verschijnen binnen 10 minuten.</p><a class=btn href=/>Terug</a>"
                          : "<h1>Koppelen mislukt</h1><p class=sub>" + esc(err) +
                                "</p><p>De code is maar kort geldig: log opnieuw in en plak de nieuwe code direct.</p><a class=btn href=/>Terug</a>");
  });
  server.on("/sma/login", HTTP_GET, []() {
    if (!auth()) return;
    server.sendHeader("Location", smaCloudAuthUrl());
    server.send(302);
  });
  server.on("/sma/code", HTTP_POST, []() {
    if (!auth()) return;
    String err;
    bool ok = smaCloudExchangeCode(server.arg("code"), err);
    sendPage("SMA", ok ? "<h1>✓ SMA-cloud gekoppeld</h1><p class=sub>De opbrengst verschijnt binnen 5 minuten.</p><a class=btn href=/>Terug</a>"
                       : "<h1>Koppelen mislukt</h1><p class=sub>" + esc(err) + "</p><a class=btn href=/>Terug</a>");
  });
  server.on("/hc/start", HTTP_POST, []() {
    if (!auth()) return;
    String code, url, err;
    if (!hcDeviceStart(code, url, err)) {
      sendPage("Home Connect", "<h1>Mislukt</h1><p class=sub>" + esc(err) + "</p><a class=btn href=/>Terug</a>");
      return;
    }
    sendPage("Home Connect",
             "<h1>Home Connect koppelen</h1><p class=sub>Open de link, log in met je Home Connect-account en bevestig de code.</p>"
             "<section><p>Code: <code style='font-size:22px'>" + esc(code) + "</code></p><a class='btn p' target=_blank href='" + esc(url) +
                 "'>Open Home Connect</a><p id=s>Wachten op bevestiging…</p></section><a class=btn href=/>Terug</a>"
                 "<script>const t=setInterval(async()=>{const r=await (await fetch('/hc/poll')).json();"
                 "if(r.status==1){document.getElementById('s').textContent='✓ Gekoppeld!';clearInterval(t)}"
                 "else if(r.status<0){document.getElementById('s').textContent='Fout: '+r.err;clearInterval(t)}},5000)</script>");
  });
  server.on("/hc/poll", HTTP_GET, []() {
    String err;
    int st = hcDevicePoll(err);
    server.send(200, "application/json", "{\"status\":" + String(st) + ",\"err\":\"" + esc(err) + "\"}");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Niet gevonden"); });
  server.begin();
}

void portalLoop() { server.handleClient(); }
