#pragma once

#include <stdint.h>

namespace bmp180 {

struct Calibration {
  int16_t ac1, ac2, ac3;
  uint16_t ac4, ac5, ac6;
  int16_t b1, b2, mb, mc, md;
};

struct Measurement {
  bool valid;
  int32_t temperatureDeciC;
  int32_t pressurePa;
};

// Algorithme entier BMP180 : compensation avec les coefficients usine.
// Intermediaires 64 bits et multiplications pour eviter les decalages
// a gauche de valeurs signees negatives (comportement indefini en C++).
#if __cplusplus >= 201402L
constexpr
#else
inline  // Compatibilite Arduino-ESP32 2.x / C++11 ; tests en C++14.
#endif
Measurement compensate(const Calibration &c, uint16_t ut,
                                  uint32_t up, uint8_t oss) {
  if (oss > 3 || c.ac4 == 0 || c.ac5 == 0 ||
      up > (65535UL << oss)) {
    return {false, 0, 0};
  }
  int64_t x1 = ((int64_t(ut) - c.ac6) * c.ac5) >> 15;
  const int64_t denominator = x1 + c.md;
  if (denominator == 0) return {false, 0, 0};
  int64_t x2 = (int64_t(c.mc) * 2048) / denominator;
  const int64_t b5 = x1 + x2;
  const int64_t temperature = (b5 + 8) >> 4;
  if (temperature < -400 || temperature > 850) return {false, 0, 0};

  const int64_t b6 = b5 - 4000;
  const int64_t b6Squared = (b6 * b6) >> 12;
  x1 = (int64_t(c.b2) * b6Squared) >> 11;
  x2 = (int64_t(c.ac2) * b6) >> 11;
  int64_t x3 = x1 + x2;
  const int64_t b3 = ((int64_t(c.ac1) * 4 + x3) * (1 << oss) + 2) / 4;
  x1 = (int64_t(c.ac3) * b6) >> 13;
  x2 = (int64_t(c.b1) * b6Squared) >> 16;
  x3 = (x1 + x2 + 2) >> 2;
  if (x3 + 32768 <= 0 || int64_t(up) <= b3) return {false, 0, 0};
  const int64_t b4 = (int64_t(c.ac4) * (x3 + 32768)) >> 15;
  if (b4 == 0) return {false, 0, 0};
  const int64_t b7 = (int64_t(up) - b3) * (50000 >> oss);
  int64_t pressure = b7 < 0x80000000LL ? (b7 * 2) / b4 : (b7 / b4) * 2;
  // Borne avant le carre pour rejeter les coefficients/donnees incoherents.
  if (pressure <= 0 || pressure > 200000) return {false, 0, 0};
  x1 = ((pressure >> 8) * (pressure >> 8) * 3038) >> 16;
  x2 = (-7357 * pressure) >> 16;
  pressure += (x1 + x2 + 3791) >> 4;
  if (pressure < 30000 || pressure > 110000) return {false, 0, 0};
  return {true, int32_t(temperature), int32_t(pressure)};
}

}  // namespace bmp180
