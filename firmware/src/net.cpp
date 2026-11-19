#include "net.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

int httpJson(const char* method, const String& url, const String& body, JsonDocument& out, const JsonDocument* filter,
             std::initializer_list<Hdr> headers, String* errOut, uint32_t timeoutMs, const char* collectHeader,
             String* collectedValue) {
  if (WiFi.status() != WL_CONNECTED) {
    if (errOut) *errOut = "geen wifi";
    return -1;
  }
  bool tls = url.startsWith("https://");
  WiFiClient plain;
  WiFiClientSecure secure;
  // Let op: certificaten worden (nog) niet gecontroleerd. Verkeer is wel versleuteld.
  if (tls) secure.setInsecure();
  HTTPClient http;
  http.setReuse(false);
  http.useHTTP10(true);  // geen chunked encoding → antwoord kan direct gestreamd worden
  http.setTimeout(timeoutMs);
  bool lan = url.indexOf("://192.168.") > 0 || url.indexOf("://10.") > 0 || url.indexOf("://172.") > 0;
  http.setConnectTimeout(lan ? 3000 : 8000);
  if (lan && timeoutMs > 6000) timeoutMs = 6000;
  http.setTimeout(timeoutMs);
  bool ok = tls ? http.begin(secure, url) : http.begin(plain, url);
  if (!ok) {
    if (errOut) *errOut = "ongeldige url";
    return -2;
  }
  http.addHeader("User-Agent", "mijn-woning-esp32");
  for (const Hdr& h : headers) http.addHeader(h.name, h.value);
  if (collectHeader) {
    const char* keys[] = {collectHeader};
    http.collectHeaders(keys, 1);
  }
  int code = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
  if (code <= 0) {
    if (errOut) *errOut = HTTPClient::errorToString(code);
    http.end();
    return code;
  }
  if (collectHeader && collectedValue) *collectedValue = http.header(collectHeader);
  out.clear();
  int len = http.getSize();
  if (len != 0) {
    DeserializationError e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
                                    : deserializeJson(out, http.getStream());
    if (e && code >= 200 && code < 300 && e != DeserializationError::EmptyInput) {
      if (errOut) *errOut = String("json: ") + e.c_str();
      http.end();
      return -3;
    }
  }
  if ((code < 200 || code >= 300) && errOut) {
    const char* m = nullptr;
    for (const char* k : {"message", "error_description", "error", "msg"}) {
      if (!m && out[k].is<const char*>()) m = out[k].as<const char*>();
    }
    String msg = m ? String(m) : String();
    *errOut = "HTTP " + String(code) + (msg.length() ? ": " + msg.substring(0, 40) : "");
  }
  http.end();
  return code;
}

String urlEncode(const String& s) {
  String o;
  o.reserve(s.length() * 3);
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      o += c;
    } else {
      o += '%';
      o += hex[(c >> 4) & 0xF];
      o += hex[c & 0xF];
    }
  }
  return o;
}

bool modbusRead(const String& ip, uint16_t port, uint8_t unit, uint8_t fn, uint16_t reg, uint16_t count,
                uint16_t* out, String& err) {
  static uint16_t tid = 1;
  WiFiClient c;
  c.setTimeout(3);
  if (!c.connect(ip.c_str(), port, 3000)) {
    err = "geen verbinding met " + ip;
    return false;
  }
  uint16_t t = tid++;
  uint8_t req[12] = {(uint8_t)(t >> 8), (uint8_t)t, 0, 0, 0, 6, unit, fn,
                     (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(count >> 8), (uint8_t)count};
  c.write(req, sizeof(req));
  uint8_t hdr[9];
  uint32_t start = millis();
  size_t got = 0;
  while (got < 9 && millis() - start < 3000) {
    int a = c.available();
    if (a > 0) got += c.read(hdr + got, min((size_t)a, 9 - got));
    else delay(5);
  }
  if (got < 9) {
    err = "timeout";
    c.stop();
    return false;
  }
  if (hdr[7] & 0x80) {
    err = "modbus fout " + String(hdr[8]);
    c.stop();
    return false;
  }
  size_t n = hdr[8];
  if (n != count * 2u) {
    err = "onverwachte lengte";
    c.stop();
    return false;
  }
  uint8_t buf[256];
  got = 0;
  while (got < n && millis() - start < 3000) {
    int a = c.available();
    if (a > 0) got += c.read(buf + got, min((size_t)a, n - got));
    else delay(5);
  }
  c.stop();
  if (got < n) {
    err = "timeout data";
    return false;
  }
  for (uint16_t i = 0; i < count; i++) out[i] = ((uint16_t)buf[i * 2] << 8) | buf[i * 2 + 1];
  return true;
}

bool ModbusConn::open(const String& ip, uint16_t port, String& err) {
  c_.setTimeout(3);
  if (!c_.connect(ip.c_str(), port, 3000)) {
    err = "geen verbinding met " + ip;
    return false;
  }
  return true;
}

bool ModbusConn::read(uint8_t unit, uint8_t fn, uint16_t reg, uint16_t count, uint16_t* out, String& err) {
  if (!c_.connected()) {
    err = "verbinding verbroken";
    return false;
  }
  while (c_.available()) c_.read();  // oude restjes weg
  uint16_t t = tid_++;
  uint8_t req[12] = {(uint8_t)(t >> 8), (uint8_t)t, 0, 0, 0, 6, unit, fn,
                     (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(count >> 8), (uint8_t)count};
  c_.write(req, sizeof(req));
  uint8_t hdr[9];
  uint32_t start = millis();
  size_t got = 0;
  while (got < 9 && millis() - start < 3000) {
    int a = c_.available();
    if (a > 0) got += c_.read(hdr + got, min((size_t)a, 9 - got));
    else delay(5);
  }
  if (got < 9) {
    err = "timeout";
    return false;
  }
  if (hdr[7] & 0x80) {
    err = "modbus fout " + String(hdr[8]);
    return false;
  }
  size_t n = hdr[8];
  if (n != count * 2u || n > 250) {
    err = "onverwachte lengte";
    return false;
  }
  uint8_t buf[256];
  got = 0;
  while (got < n && millis() - start < 3000) {
    int a = c_.available();
    if (a > 0) got += c_.read(buf + got, min((size_t)a, n - got));
    else delay(5);
  }
  if (got < n) {
    err = "timeout data";
    return false;
  }
  for (uint16_t i = 0; i < count; i++) out[i] = ((uint16_t)buf[i * 2] << 8) | buf[i * 2 + 1];
  return true;
}
