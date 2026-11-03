#pragma once
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <stdio.h>

// Un seul producteur du CDC0 : ne jamais appeler Serial.write(), dont la
// boucle attend sans limite si DTR reste actif devant un hote bloque.
class BoundedUsbLog : public Print {
public:
  size_t write(uint8_t value) override {
    if (value == '\n') {
      if (!tud_cdc_n_connected(0)) { length_ = 0; overflow_ = false; return 1; }
      if (overflow_) ++dropped_;
      else {
        if (dropped_) {
          char marker[80];
          const int count = snprintf(marker, sizeof(marker), "[SOIL_LOG] dropped_lines=%lu\r\n", static_cast<unsigned long>(dropped_));
          if (send(reinterpret_cast<const uint8_t*>(marker), static_cast<size_t>(count))) dropped_ = 0;
        }
        line_[length_++] = '\n';
        if (!send(line_, length_)) ++dropped_;
      }
      length_ = 0; overflow_ = false;
    } else if (length_ < sizeof(line_) - 1) line_[length_++] = value;
    else overflow_ = true;
    return 1;
  }
  size_t write(const uint8_t* data, size_t length) override {
    for (size_t i = 0; i < length; ++i) write(data[i]);
    return length;
  }
private:
  static bool send(const uint8_t* data, size_t length) {
    const uint32_t started = millis();
    while (tud_cdc_n_connected(0)) {
      if (tud_cdc_n_write_available(0) >= length) {
        const bool complete = tud_cdc_n_write(0, data, length) == length;
        tud_cdc_n_write_flush(0);
        return complete;
      }
      if (static_cast<uint32_t>(millis() - started) >= 4) return false;
      yield();
    }
    return false;
  }
  uint8_t line_[256];
  size_t length_ = 0;
  uint32_t dropped_ = 0;
  bool overflow_ = false;
};
