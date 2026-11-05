#pragma once

#include <Arduino.h>
#include <Wire.h>

enum class I2cTarget : uint8_t { None, Sensor, Display };

#ifndef ENVIRO_BMP_SDA
#define ENVIRO_BMP_SDA 4
#endif
#ifndef ENVIRO_BMP_SCL
#define ENVIRO_BMP_SCL 3
#endif

namespace EnviroPins {
constexpr int BMP_SDA = ENVIRO_BMP_SDA;
constexpr int BMP_SCL = ENVIRO_BMP_SCL;
constexpr int LCD_SDA = 3;
constexpr int LCD_SCL = 2;
}

// Le C3 ne possede qu'un controleur I2C. Les deux paires de broches
// sont utilisees successivement, jamais par deux instances TwoWire concurrentes.
class I2cBusRouter {
 public:
  bool select(I2cTarget target) {
    if (target == active_ && target != I2cTarget::None) return true;
    if (!reset()) return false;
    if (target == I2cTarget::None) return true;
    const int sda = target == I2cTarget::Sensor ? EnviroPins::BMP_SDA : EnviroPins::LCD_SDA;
    const int scl = target == I2cTarget::Sensor ? EnviroPins::BMP_SCL : EnviroPins::LCD_SCL;
    lastSda_ = sda;
    lastScl_ = scl;
    if (!Wire.begin(sda, scl, 100000)) {
      Serial.printf("[ERREUR] Initialisation I2C impossible : SDA=GPIO%d, SCL=GPIO%d.\n", sda, scl);
      return false;
    }
    Wire.setTimeOut(50);
    active_ = target;
    return true;
  }

  bool reset() {
    active_ = I2cTarget::None;
    if (!Wire.end()) {
      Serial.println("[ERREUR] Impossible de liberer le controleur I2C.");
      return false;
    }
    // Dans Arduino-ESP32 2.x, Wire.end() supprime le pilote mais ne libere
    // pas les anciennes sorties GPIO de la matrice du controleur I2C.
    if (lastSda_ >= 0) pinMode(lastSda_, INPUT);
    if (lastScl_ >= 0) pinMode(lastScl_, INPUT);
    lastSda_ = lastScl_ = -1;
    return true;
  }

 private:
  I2cTarget active_ = I2cTarget::None;
  int lastSda_ = -1;
  int lastScl_ = -1;
};
