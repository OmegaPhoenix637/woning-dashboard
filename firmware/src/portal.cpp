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

static WebServer server(80);
static bool uploadNow = false;

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
code{background:#0b0b0d;padding:2px 6px;border-radius:6px}
)CSS";

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
  if (server.hasArg("saved")) b += F("<div class=note>✓ Opgeslagen. Wifi-wijzigingen werken na een herstart.</div>");
  b += F("<section><h2>Status</h2><table id=st><tr><td>Laden…</td></tr></table><div class=row>"
         "<form method=post action=/upload><button class=btn>Nu uploaden</button></form>"
         "<form method=post action=/restart><button class=btn>Herstarten</button></form>"
         "<a class=btn href=/api/live target=_blank>live.json</a></div></section>");
  // Koppelen
  b += F("<section><h2>Koppelen</h2>"
         "<p><b>Daikin Onecta</b>: vul hieronder Client ID, secret en Redirect URI in en sla op. Daarna:</p>"
         "<div class=row><a class='btn p' href=/onecta/login target=_blank>1. Inloggen bij Daikin</a></div>"
         "<form method=post action=/onecta/code><label>2. Plak de koppelcode</label><input name=code required>"
         "<button class='btn p'>Koppelen</button></form>"
         "<p style='margin-top:18px'><b>Home Connect</b> (Bosch/Siemens): vul de Client ID in en sla op. Daarna:</p>"
         "<form method=post action=/hc/start><button class='btn p'>Koppel Home Connect</button></form></section>");
  // Formulier
  b += F("<form method=post action=/save>");
  String group;
  for (size_t i = 0; i < N_FIELDS; i++) {
    const FieldDef& f = FIELDS[i];
    if (group != f.group) {
      if (group.length()) b += F("</section>");
      group = f.group;
      b += "<section><h2>" + esc(group) + "</h2>";
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
  b += F("</section><button class='btn p' style='width:100%'>Opslaan</button></form>");
  b += F("<script>"
         "async function s(){try{const d=await (await fetch('/api/live')).json();const h=d.health;let r='';"
         "r+=`<tr><td>Firmware</td><td>${h.fw} · ${Math.round(h.uptime/3600)} u aan · ${Math.round(h.heap/1024)} KB vrij · wifi ${h.rssi} dBm</td></tr>`;"
         "r+=`<tr><td>Wachtende dagen</td><td>${h.pending}</td></tr>`;"
         "for(const[k,v]of Object.entries(h.sources)){const age=Math.round((Date.now()/1000-v.last)/60);"
         "r+=`<tr><td>${k}</td><td class=${v.ok?'ok':'bad'}>${v.ok?'✓ werkt':'✕ '+(v.err||'fout')}${v.last?' · '+age+' min geleden':''}${v.limit?' · '+v.calls+'/'+v.limit:''}</td></tr>`}"
         "document.getElementById('st').innerHTML=r}catch(e){}}s();setInterval(s,10000)</script>");
  sendPage("Mijn Woning ESP32", b);
}

static void handleSave() {
  if (!auth()) return;
  for (size_t i = 0; i < N_FIELDS; i++) {
    const FieldDef& f = FIELDS[i];
    if (f.type == F_BOOL) {
      if (server.hasArg(String(f.key) + "__b")) cfgSet(f.key, server.hasArg(f.key) ? "1" : "0");
      continue;
    }
    if (!server.hasArg(f.key)) continue;
    String v = server.arg(f.key);
    v.trim();
    if (f.type == F_SECRET && !v.length()) continue;  // leeg = niet wijzigen
    if (v != cfg(f.key)) cfgSet(f.key, v);
  }
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
