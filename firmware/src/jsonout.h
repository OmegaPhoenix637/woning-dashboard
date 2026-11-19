#pragma once
#include <Arduino.h>

// live.json (zelfde vorm als LiveData in web/src/lib/types.ts)
void writeLiveJson(Print& out);

// Hulpklassen om grote JSON te streamen zonder alles in het geheugen te houden.
class CountPrint : public Print {
 public:
  size_t n = 0;
  size_t write(uint8_t) override { return ++n, 1; }
  size_t write(const uint8_t*, size_t len) override { return n += len, len; }
};

class BufferedPrint : public Print {
 public:
  explicit BufferedPrint(Print& dst) : dst_(dst) {}
  size_t write(uint8_t b) override {
    buf_[len_++] = b;
    if (len_ == sizeof(buf_)) flush();
    return 1;
  }
  void flush() override {
    if (len_) dst_.write(buf_, len_);
    len_ = 0;
  }

 private:
  Print& dst_;
  uint8_t buf_[1024];
  size_t len_ = 0;
};

class Base64Print : public Print {
 public:
  explicit Base64Print(Print& dst) : dst_(dst) {}
  size_t write(uint8_t b) override {
    in_[n_++] = b;
    if (n_ == 3) emit();
    return 1;
  }
  void finish() {
    if (n_) emit();
  }

 private:
  void emit() {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t v = (uint32_t)in_[0] << 16 | (n_ > 1 ? (uint32_t)in_[1] << 8 : 0) | (n_ > 2 ? in_[2] : 0);
    dst_.write(T[(v >> 18) & 63]);
    dst_.write(T[(v >> 12) & 63]);
    dst_.write(n_ > 1 ? T[(v >> 6) & 63] : '=');
    dst_.write(n_ > 2 ? T[v & 63] : '=');
    n_ = 0;
  }
  Print& dst_;
  uint8_t in_[3];
  uint8_t n_ = 0;
};
