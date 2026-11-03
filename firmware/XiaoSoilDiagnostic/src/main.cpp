#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

namespace {
constexpr uint8_t kSda = 6, kScl = 7;
constexpr uint32_t kClockTimeoutUs = 2000;
constexpr uint16_t kConfig = 0xC383;  // A0/GND, +/-4.096 V, single-shot, 128 SPS.

// Bus logiciel lent, a sorties uniquement basses ou relachees. Chaque attente
// SCL est bornee, meme si une erreur de cablage maintient la ligne a la masse.
class DiagnosticBus {
 public:
  void release() {
    pinMode(kSda, INPUT_PULLUP);
    pinMode(kScl, INPUT_PULLUP);
  }

  bool probe(uint8_t address) {
    const bool acknowledged = start() && writeByte(address << 1);
    const bool stopped = stop();
    return acknowledged && stopped;
  }

  bool writeRegister(uint8_t address, uint8_t reg, uint16_t value) {
    const bool written = start() && writeByte(address << 1) &&
                         writeByte(reg) && writeByte(value >> 8) &&
                         writeByte(value & 0xFF);
    const bool stopped = stop();
    return written && stopped;
  }

  bool readRegister(uint8_t address, uint8_t reg, uint16_t& value) {
    const bool selected = start() && writeByte(address << 1) && writeByte(reg);
    const bool stopped = stop();
    if (!selected || !stopped) return false;
    uint8_t high = 0, low = 0;
    const bool received = start() && writeByte((address << 1) | 1) &&
                          readByte(high, true) && readByte(low, false);
    const bool finished = stop();
    if (!received || !finished) return false;
    value = (static_cast<uint16_t>(high) << 8) | low;
    return true;
  }

 private:
  static void pause() { delayMicroseconds(10); }
  static void driveLow(uint8_t pin) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  static void releasePin(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }

  bool clockHigh() {
    releasePin(kScl);
    const uint32_t started = micros();
    while (digitalRead(kScl) == LOW) {
      if (static_cast<uint32_t>(micros() - started) >= kClockTimeoutUs) {
        release();
        return false;
      }
    }
    pause();
    return true;
  }

  bool start() {
    releasePin(kSda);
    if (!clockHigh() || digitalRead(kSda) == LOW) return false;
    driveLow(kSda);
    pause();
    driveLow(kScl);
    pause();
    return true;
  }

  bool stop() {
    driveLow(kScl);
    driveLow(kSda);
    pause();
    const bool clockReady = clockHigh();
    releasePin(kSda);
    pause();
    const bool idle = digitalRead(kSda) == HIGH;
    release();
    return clockReady && idle;
  }

  bool writeByte(uint8_t value) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      if (value & mask) releasePin(kSda);
      else driveLow(kSda);
      pause();
      if (!clockHigh()) return false;
      driveLow(kScl);
      pause();
    }
    releasePin(kSda);
    pause();
    if (!clockHigh()) return false;
    const bool acknowledged = digitalRead(kSda) == LOW;
    driveLow(kScl);
    pause();
    return acknowledged;
  }

  bool readByte(uint8_t& value, bool acknowledge) {
    value = 0;
    releasePin(kSda);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      pause();
      if (!clockHigh()) return false;
      value = (value << 1) | (digitalRead(kSda) == HIGH ? 1 : 0);
      driveLow(kScl);
      pause();
    }
    if (acknowledge) driveLow(kSda);
    else releasePin(kSda);
    pause();
    if (!clockHigh()) return false;
    driveLow(kScl);
    pause();
    releasePin(kSda);
    return true;
  }
};

DiagnosticBus bus;
bool correctPinMap = false;
bool usbWasConnected = false;
uint32_t attempts = 0, readings = 0, errors = 0;
uint8_t adsAddress = 0;

void scanBus() {
  adsAddress = 0;
  bus.release();
  delay(1);
  Serial.print(F("[BUS] SDA=")); Serial.print(digitalRead(kSda));
  Serial.print(F(" SCL=")); Serial.println(digitalRead(kScl));
  if (digitalRead(kSda) == LOW || digitalRead(kScl) == LOW) {
    Serial.println(F("[ERREUR] Ligne I2C maintenue basse : verifier cablage et alimentation."));
    return;
  }
  uint8_t found = 0;
  uint8_t candidates = 0, candidate = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    if (bus.probe(address)) {
      ++found;
      Serial.print(F("[I2C] ACK adresse=0x")); Serial.println(address, HEX);
      if (address >= 0x48 && address <= 0x4B) {
        ++candidates;
        candidate = address;
      }
    }
  }
  Serial.print(F("[SCAN] peripheriques=")); Serial.println(found);
  Serial.println(F("[INFO] Un ACK seul ne permet pas d'identifier la puce."));
  if (candidates == 1) {
    adsAddress = candidate;
    Serial.print(F("[ADC] adresse_selectionnee=0x")); Serial.println(adsAddress, HEX);
    if (adsAddress != 0x48) {
      Serial.println(F("[INFO] Adresse differente de 0x48 : verifier le strap ADDR hors tension."));
    }
  } else {
    Serial.println(F("[ERREUR] Aucun candidat ADS unique entre 0x48 et 0x4B ; aucune configuration envoyee."));
  }
}

bool readA0(int16_t& raw) {
  if (!adsAddress) return false;
  if (!bus.writeRegister(adsAddress, 1, kConfig)) {
    Serial.println(F("[ERREUR] Ecriture configuration ADC : NACK ou bus bloque."));
    return false;
  }
  const uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < 40) {
    delay(2);
    uint16_t config = 0;
    if (!bus.readRegister(adsAddress, 1, config)) {
      Serial.println(F("[ERREUR] Lecture configuration interrompue."));
      return false;
    }
    if ((config & 0x7FFF) != (kConfig & 0x7FFF)) {
      Serial.println(F("[ERREUR] Configuration relue incompatible avec le diagnostic ADS1115."));
      return false;
    }
    if (config & 0x8000) {
      uint16_t conversion = 0;
      if (!bus.readRegister(adsAddress, 0, conversion)) {
        Serial.println(F("[ERREUR] Lecture conversion A0 interrompue."));
        return false;
      }
      raw = static_cast<int16_t>(conversion);
      return true;
    }
  }
  Serial.println(F("[ERREUR] Conversion non terminee apres 40 ms."));
  return false;
}
void announce() {
  Serial.println(F("\n[XIAO-SOIL-DIAG] v1 / USB / D6=P1.11 SDA / D7=P1.12 SCL"));
  Serial.println(F("[CONFIG] ADS1115 attendu 0x48, selection unique 0x48..0x4B, A0/GND, +/-4.096 V, 128 SPS."));
  Serial.println(F("[INFO] D0 relachee ; radio non initialisee ; aucun pourcentage sans etalonnage."));
  if (!correctPinMap) {
    Serial.println(F("[ERREUR] Variante Arduino incompatible : test GPIO interdit."));
  }
}
}  // namespace

void setup() {
  // D0 reste une entree sans tirage : aucune commande du bouton/P-MOS.
  pinMode(0, INPUT);
  correctPinMap = g_ADigitalPinMap[kSda] == 43 && g_ADigitalPinMap[kScl] == 44;
  if (correctPinMap) bus.release();
  Serial.begin(115200);
}

void loop() {
  delay(1000);
  if (!Serial) {
    usbWasConnected = false;
    return;
  }
  if (!usbWasConnected) {
    announce();
    usbWasConnected = true;
    if (correctPinMap) scanBus();
  }
  if (!correctPinMap) return;
  ++attempts;
  if (attempts == 1 || attempts % 10 == 0) scanBus();
  int16_t raw = 0;
  if (readA0(raw)) {
    ++readings;
    const float volts = raw * 0.000125f;
    Serial.print(F("[MESURE] t_ms=")); Serial.print(millis());
    Serial.print(F(" adresse=0x")); Serial.print(adsAddress, HEX);
    Serial.print(F(" A0_brut=")); Serial.print(raw);
    Serial.print(F(" A0_V=")); Serial.print(volts, 6);
    Serial.print(F(" etat="));
    // Quelques codes negatifs peuvent venir de l'offset, meme avec A0 a GND.
    if (raw < -8 || volts > 3.3f) Serial.println(F("HORS_PLAGE_NOMINALE"));
    else if (raw < 0) Serial.println(F("PROCHE_ZERO_OFFSET_POSSIBLE"));
    else Serial.println(F("DANS_PLAGE_NOMINALE"));
  } else {
    ++errors;
  }
  Serial.print(F("[BILAN] essais=")); Serial.print(attempts);
  Serial.print(F(" mesures=")); Serial.print(readings);
  Serial.print(F(" erreurs=")); Serial.println(errors);
}
