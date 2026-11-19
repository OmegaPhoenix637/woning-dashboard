#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <initializer_list>

struct Hdr {
  const char* name;
  String value;
};

// HTTP(S)-verzoek met JSON-antwoord. Het antwoord wordt al tijdens het binnenkomen gefilterd
// (belangrijk voor grote antwoorden zoals Daikin Onecta op een ESP32 met weinig geheugen).
// Geeft de HTTP-statuscode terug (negatief = verbindingsfout). errOut krijgt een korte foutmelding.
int httpJson(const char* method, const String& url, const String& body, JsonDocument& out,
             const JsonDocument* filter = nullptr, std::initializer_list<Hdr> headers = {},
             String* errOut = nullptr, uint32_t timeoutMs = 20000, const char* collectHeader = nullptr,
             String* collectedValue = nullptr);

String urlEncode(const String& s);

// Minimale Modbus TCP-client (functie 3 = holding registers, 4 = input registers).
bool modbusRead(const String& ip, uint16_t port, uint8_t unit, uint8_t fn, uint16_t reg, uint16_t count,
                uint16_t* out, String& err);

// Eén Modbus TCP-verbinding voor meerdere verzoeken (sommige apparaten weigeren snel opeenvolgende verbindingen).
#include <WiFiClient.h>
class ModbusConn {
 public:
  bool open(const String& ip, uint16_t port, String& err);
  bool read(uint8_t unit, uint8_t fn, uint16_t reg, uint16_t count, uint16_t* out, String& err);
  ~ModbusConn() { c_.stop(); }

 private:
  WiFiClient c_;
  uint16_t tid_ = 1;
};

inline int32_t mbS32(const uint16_t* r) { return (int32_t)(((uint32_t)r[0] << 16) | r[1]); }
inline uint32_t mbU32(const uint16_t* r) { return ((uint32_t)r[0] << 16) | r[1]; }
