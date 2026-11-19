#include "github.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>
#include <vector>
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

// Blob maken: de inhoud wordt twee keer gegenereerd (eerst tellen, dan base64 streamen).
static bool createBlob(std::function<void(Print&)> writer, String& sha, String& err) {
  CountPrint cnt;
  writer(cnt);
  size_t b64 = 4 * ((cnt.n + 2) / 3);
  const char* pre = "{\"encoding\":\"base64\",\"content\":\"";
  const char* post = "\"}";
  size_t total = strlen(pre) + b64 + strlen(post);

  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(30);
  if (!c.connect(GH_HOST, 443)) {
    err = "geen verbinding met GitHub";
    return false;
  }
  c.printf("POST %s/git/blobs HTTP/1.0\r\n", repoPath().c_str());
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
  String status = c.readStringUntil('\n');
  int code = status.length() > 12 ? status.substring(9, 12).toInt() : 0;
  while (c.connected() || c.available()) {
    String line = c.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }
  JsonDocument filter;
  filter["sha"] = true;
  filter["message"] = true;
  JsonDocument doc;
  deserializeJson(doc, c, DeserializationOption::Filter(filter));
  c.stop();
  if (code != 201 || !doc["sha"].is<const char*>()) {
    err = "blob: HTTP " + String(code) + " " + (doc["message"] | "");
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
  std::vector<Entry> entries;
  String sha;
  if (!createBlob([](Print& p) { writeLiveJson(p); }, sha, err)) return false;
  entries.push_back({"live.json", sha});
  if (!createBlob([](Print& p) { writeTodayJson(p); }, sha, err)) return false;
  entries.push_back({"today.json", sha});
  if (!commitTree("live", entries, false, "live", err)) return false;
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
