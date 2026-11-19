#include "github.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>
#include <vector>
#include "config.h"
#include "jsonout.h"
#include "net.h"
#include "sampler.h"
#include "settings.h"
#include "state.h"

static const char* GH_HOST = "api.github.com";
static uint32_t lastLiveOk = 0;
uint32_t ghLastLiveOk() { return lastLiveOk; }

static String repoPath() { return "/repos/" + cfg("gh_owner") + "/" + cfg("gh_repo"); }

static int ghApi(const char* method, const String& path, const String& body, JsonDocument& out, String& err,
                 const JsonDocument* filter = nullptr, const char* accept = "application/vnd.github+json") {
  return httpJson(method, String("https://") + GH_HOST + path, body, out, filter,
                  {{"Authorization", "Bearer " + cfg("gh_token")},
                   {"Accept", accept},
                   {"X-GitHub-Api-Version", "2022-11-28"},
                   {"Content-Type", "application/json"}},
                  &err, 30000);
}

// Verzoek met gestreamde inhoud: body = pre + base64(writer) + post. De inhoud wordt twee keer gemaakt
// (eerst tellen voor Content-Length, dan versturen), zodat hij nooit in zijn geheel in het geheugen staat.
static int streamB64(const char* method, const String& path, const String& pre, std::function<void(Print&)> writer,
                     const String& post, JsonDocument& out, const JsonDocument& filter, String& err) {
  uint32_t t0 = millis();
  CountPrint cnt;
  writer(cnt);
  size_t total = pre.length() + 4 * ((cnt.n + 2) / 3) + post.length();
  uint32_t tCount = millis();
  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(30);
  if (!c.connect(GH_HOST, 443)) {
    err = "geen verbinding met GitHub";
    return -1;
  }
  uint32_t tConn = millis();
  c.printf("%s %s HTTP/1.0\r\n", method, path.c_str());
  c.printf("Host: %s\r\n", GH_HOST);
  c.printf("Authorization: Bearer %s\r\n", cfg("gh_token").c_str());
  c.print("User-Agent: mijn-woning-esp32\r\nAccept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n");
  c.printf("Content-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n", (unsigned)total);
  {
    BufferedPrint bp(c);
    bp.print(pre);
    Base64Print enc(bp);
    writer(enc);
    enc.finish();
    bp.print(post);
    bp.flush();
  }
  uint32_t tSent = millis();
  c.setTimeout(20);                            // socket
  static_cast<Stream&>(c).setTimeout(20000);   // en de Stream-functies (readStringUntil, JSON)
  while (!c.available() && c.connected() && millis() - tSent < 20000) delay(10);
  String status = c.readStringUntil('\n');
  uint32_t tStatus = millis();
  int code = status.length() > 12 ? status.substring(9, 12).toInt() : 0;
  while (c.connected() || c.available()) {
    String line = c.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }
  deserializeJson(out, c, DeserializationOption::Filter(filter));
  uint32_t tBody = millis();
  c.stop();
  if (code < 200 || code >= 300)
    Serial.printf("[github] %s %s -> %d na %lu ms\n", method, path.c_str(), code, millis() - t0);
  if (code < 200 || code >= 300) err = "HTTP " + String(code) + " " + (out["message"] | "");
  return code;
}

// Blob maken (Git Data API)
static bool createBlob(std::function<void(Print&)> writer, String& sha, String& err) {
  JsonDocument filter, doc;
  filter["sha"] = true;
  filter["message"] = true;
  int code = streamB64("POST", repoPath() + "/git/blobs", "{\"encoding\":\"base64\",\"content\":\"", writer, "\"}", doc, filter, err);
  if (code != 201 || !doc["sha"].is<const char*>()) {
    err = "blob: " + err;
    return false;
  }
  sha = doc["sha"].as<String>();
  return true;
}

struct Entry {
  String path, sha;
};

static bool commitTree(const String& branch, const std::vector<Entry>& entries, bool keepHistory, const String& message, String& err) {
  JsonDocument out;
  String parent, baseTree;
  if (keepHistory) {
    int code = ghApi("GET", repoPath() + "/git/ref/heads/" + branch, "", out, err);
    if (code == 200) {
      parent = out["object"]["sha"] | "";
      if (ghApi("GET", repoPath() + "/git/commits/" + parent, "", out, err) != 200) return false;
      baseTree = out["tree"]["sha"] | "";
    } else if (code != 404) {
      return false;
    }
  }
  JsonDocument body;
  if (baseTree.length()) body["base_tree"] = baseTree;
  JsonArray tree = body["tree"].to<JsonArray>();
  for (const Entry& e : entries) {
    JsonObject o = tree.add<JsonObject>();
    o["path"] = e.path;
    o["mode"] = "100644";
    o["type"] = "blob";
    o["sha"] = e.sha;
  }
  String b;
  serializeJson(body, b);
  if (ghApi("POST", repoPath() + "/git/trees", b, out, err) != 201) return false;
  String treeSha = out["sha"] | "";

  body.clear();
  body["message"] = message;
  body["tree"] = treeSha;
  JsonArray parents = body["parents"].to<JsonArray>();
  if (parent.length()) parents.add(parent);
  b = "";
  serializeJson(body, b);
  if (ghApi("POST", repoPath() + "/git/commits", b, out, err) != 201) return false;
  String commitSha = out["sha"] | "";

  body.clear();
  body["sha"] = commitSha;
  body["force"] = !keepHistory;
  b = "";
  serializeJson(body, b);
  int code = ghApi("PATCH", repoPath() + "/git/refs/heads/" + branch, b, out, err);
  if (code == 200) return true;
  // Branch bestaat nog niet: aanmaken
  body.clear();
  body["ref"] = "refs/heads/" + branch;
  body["sha"] = commitSha;
  b = "";
  serializeJson(body, b);
  return ghApi("POST", repoPath() + "/git/refs", b, out, err) == 201;
}

bool ghPublishLive(String& err) {
  if (!cfgHas("gh_token")) {
    err = "geen GitHub-token";
    return false;
  }
  static String liveSha;     // blob-sha van live.json zoals hij nu op de live-branch staat
  static uint32_t fullAt = 0;  // laatste volledige upload (live + today, zonder geschiedenis)

  // Snel: alleen live.json bijwerken met één verzoek (Contents API)
  if (liveSha.length() && nowUnix() - fullAt < IV_TODAY_UPLOAD) {
    JsonDocument filter, doc;
    filter["content"]["sha"] = true;
    filter["message"] = true;
    int code = streamB64("PUT", repoPath() + "/contents/live.json",
                         "{\"message\":\"live\",\"branch\":\"live\",\"sha\":\"" + liveSha + "\",\"content\":\"",
                         [](Print& p) { writeLiveJson(p); }, "\"}", doc, filter, err);
    if (code == 200 && doc["content"]["sha"].is<const char*>()) {
      liveSha = doc["content"]["sha"].as<String>();
      lastLiveOk = nowUnix();
      return true;
    }
    liveSha = "";  // iets mis (bv. sha klopt niet meer): hieronder volledig opnieuw
  }

  // Volledig: live + today als nieuwe commit zonder geschiedenis (houdt de repo klein)
  std::vector<Entry> entries;
  String sha, lsha;
  if (!createBlob([](Print& p) { writeLiveJson(p); }, lsha, err)) return false;
  entries.push_back({"live.json", lsha});
  if (!createBlob([](Print& p) { writeTodayJson(p); }, sha, err)) return false;
  entries.push_back({"today.json", sha});
  if (!commitTree("live", entries, false, "live", err)) return false;
  liveSha = lsha;
  fullAt = nowUnix();
  lastLiveOk = nowUnix();
  return true;
}

// Haal eenmalig de bestaande jaarsamenvatting van GitHub op, zodat een lege ESP (nieuw of gewist)
// nooit de geschiedenis op GitHub overschrijft. De dagen worden als losse regels opgeslagen.
static bool bootstrapSummary(int year, String& err) {
  String flag = "/sum/" + String(year) + ".ok";
  if (LittleFS.exists(flag)) return true;
  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(30);
  if (!c.connect(GH_HOST, 443)) {
    err = "geen verbinding met GitHub";
    return false;
  }
  c.printf("GET %s/contents/summary/%d.json?ref=main HTTP/1.0\r\nHost: %s\r\n", repoPath().c_str(), year, GH_HOST);
  c.printf("Authorization: Bearer %s\r\n", cfg("gh_token").c_str());
  c.print("User-Agent: mijn-woning-esp32\r\nAccept: application/vnd.github.raw+json\r\nX-GitHub-Api-Version: 2022-11-28\r\nConnection: close\r\n\r\n");
  static_cast<Stream&>(c).setTimeout(20000);
  { uint32_t w = millis(); while (!c.available() && c.connected() && millis() - w < 20000) delay(10); }
  String status = c.readStringUntil('\n');
  int code = status.length() > 12 ? status.substring(9, 12).toInt() : 0;
  while (c.connected() || c.available()) {
    String line = c.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }
  if (code == 404) {
    c.stop();
    File f = LittleFS.open(flag, "w");
    if (f) f.close();
    return true;
  }
  if (code != 200) {
    err = "samenvatting ophalen: HTTP " + String(code);
    c.stop();
    return false;
  }
  // Lokale regels bewaren, dan remote dagen (die lokaal ontbreken) ervoor zetten.
  String path = "/sum/" + String(year) + ".jsonl";
  String local;
  File lf = LittleFS.open(path, "r");
  if (lf) {
    local = lf.readString();
    lf.close();
  }
  File tmp = LittleFS.open("/sum/tmp.jsonl", "w");
  // Eenvoudige scanner: elk object op diepte 2 binnen "days":[...] is één dag.
  int depth = 0;
  bool inStr = false, esc = false, inDays = false;
  String cur, key;
  uint32_t t0 = millis();
  while ((c.connected() || c.available()) && millis() - t0 < 30000) {
    if (!c.available()) {
      delay(2);
      continue;
    }
    char ch = c.read();
    if (depth >= 2 && inDays) cur += ch;
    if (inStr) {
      if (esc) esc = false;
      else if (ch == '\\') esc = true;
      else if (ch == '"') inStr = false;
      continue;
    }
    if (ch == '"') {
      inStr = true;
      continue;
    }
    if (ch == '[' && depth == 1) inDays = true;
    if (ch == '{') {
      depth++;
      if (depth == 2 && inDays) cur = "{";
    } else if (ch == '}') {
      if (depth == 2 && inDays) {
        int p = cur.indexOf("\"date\":\"");
        String date = p >= 0 ? cur.substring(p + 8, p + 18) : "";
        if (date.length() && local.indexOf("\"date\":\"" + date + "\"") < 0) tmp.println(cur);
        cur = "";
      }
      depth--;
    }
  }
  c.stop();
  tmp.print(local);
  tmp.close();
  LittleFS.remove(path);
  LittleFS.rename("/sum/tmp.jsonl", path);
  File f = LittleFS.open(flag, "w");
  if (f) f.close();
  return true;
}

bool ghPublishPending(String& err) {
  if (!cfgHas("gh_token")) {
    err = "geen GitHub-token";
    return false;
  }
  std::vector<String> dates;
  // Verzamel tot 3 oudste dagen
  for (int i = 0; i < 3; i++) {
    String d;
    if (!pendingOldest(d)) break;
    bool dup = false;
    for (auto& x : dates) dup = dup || x == d;
    if (dup) break;
    dates.push_back(d);
    // tijdelijk hernoemen zodat pendingOldest de volgende vindt
    LittleFS.rename("/p/" + d + ".bin", "/p/" + d + ".busy");
  }
  auto restore = [&]() {
    for (auto& d : dates) LittleFS.rename("/p/" + d + ".busy", "/p/" + d + ".bin");
  };
  if (dates.empty()) return true;
  restore();

  std::vector<Entry> entries;
  std::vector<int> years;
  for (auto& d : dates) {
    int y = d.substring(0, 4).toInt();
    bool seen = false;
    for (int x : years) seen = seen || x == y;
    if (!seen) years.push_back(y);
    String sha;
    if (!createBlob([&](Print& p) { writePendingDayJson(d, p); }, sha, err)) return false;
    entries.push_back({"days/" + String(y) + "/" + d + ".json", sha});
  }
  for (int y : years) {
    if (!bootstrapSummary(y, err)) return false;
    String sha;
    if (!createBlob([y](Print& p) { writeSummaryJson(y, p); }, sha, err)) return false;
    entries.push_back({"summary/" + String(y) + ".json", sha});
  }
  String msg = "Dag " + dates.front() + (dates.size() > 1 ? " t/m " + dates.back() : "");
  if (!commitTree("main", entries, true, msg, err)) return false;
  for (auto& d : dates) pendingDone(d);
  Serial.printf("[github] %u dag(en) geüpload\n", (unsigned)dates.size());
  return true;
}
