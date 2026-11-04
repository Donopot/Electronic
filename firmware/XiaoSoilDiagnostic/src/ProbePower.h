#pragma once
#include <Arduino.h>

// D0 partage le bouton du Wio : aucune sortie haute ni rappel interne.
class ProbePower {
public:
  using Reader = bool (*)(int16_t&);
  void off() {
    pinMode(0, INPUT);
    powered_ = false;
    delayMicroseconds(200); // laisser le rappel externe ramener la grille
  }
  bool on() {
    if (powered_) return true;
    off();
    if (digitalRead(0) != HIGH) return false;
    digitalWrite(0, LOW);
    pinMode(0, OUTPUT);
    powered_ = true;
    return true;
  }
  bool powered() const { return powered_; }
  int gate() const { return digitalRead(0); }
  bool acquire(Reader reader, int16_t& raw) {
    raw = 0;
    if (!on()) return false;
    delay(1000);
    const bool valid = median(reader, raw);
    off(); // avant tout journal, retour d'erreur ou traitement radio
    return valid;
  }
  static bool median(Reader reader, int16_t& raw) {
    raw = 0;
    int16_t values[5];
    for (unsigned i = 0; i < 5; ++i) {
      if (!reader(values[i])) return false;
    }
    for (unsigned i = 1; i < 5; ++i) {
      const int16_t value = values[i];
      unsigned j = i;
      while (j && values[j - 1] > value) {
        values[j] = values[j - 1];
        --j;
      }
      values[j] = value;
    }
    raw = values[2];
    return true;
  }
private:
  bool powered_ = false;
};
