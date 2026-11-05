#include <Arduino.h>
#include <Wire.h>

// Diagnostic temporaire : ACK uniquement, plus lecture de l'identifiant
// si un peripherique repond a 0x77. Aucune initialisation LCD/capteur.
struct PinPair {
  int sda;
  int scl;
};

constexpr PinPair pairs[] = {{3, 2}, {2, 3}, {4, 5}, {5, 4}};

void releasePins() {
  Wire.end();
  for (int pin = 2; pin <= 5; ++pin) pinMode(pin, INPUT);
}

void probePair(const PinPair &pair) {
  releasePins();
  pinMode(pair.sda, INPUT_PULLUP);
  pinMode(pair.scl, INPUT_PULLUP);
  delay(5);
  Serial.printf("[PROBE] SDA=GPIO%d SCL=GPIO%d | repos SDA=%d SCL=%d\n",
                pair.sda, pair.scl, digitalRead(pair.sda), digitalRead(pair.scl));
  if (!Wire.begin(pair.sda, pair.scl, 100000)) {
    Serial.println("[PROBE] Initialisation I2C impossible.");
    return;
  }
  Wire.setTimeOut(50);
  const uint8_t addresses[] = {0x3F, 0x77};
  for (uint8_t address : addresses) {
    Wire.beginTransmission(address);
    const uint8_t status = Wire.endTransmission(true);
    Serial.printf("[PROBE] SDA=%d SCL=%d adresse=0x%02X : %s (code %u)\n",
                  pair.sda, pair.scl, address,
                  status == 0 ? "ACK" : status == 2 ? "NACK" :
                  status == 5 ? "TIMEOUT" : "ERREUR", status);
    if (status == 0 && address == 0x77) {
      Wire.beginTransmission(address);
      Wire.write(uint8_t(0xD0));
      if (Wire.endTransmission(true) == 0 &&
          Wire.requestFrom(address, uint8_t(1)) == 1) {
        Serial.printf("[PROBE] Identifiant 0x77 = 0x%02X (BMP180 attendu : 0x55)\n", Wire.read());
      }
    }
  }
  releasePins();
}

void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && uint32_t(millis() - started) < 3000) delay(10);
}

void loop() {
  Serial.println("[PROBE] Debut test des quatre orientations I2C.");
  for (const auto &pair : pairs) probePair(pair);
  Serial.println("[PROBE] Fin ; prochain test dans 10 secondes.");
  delay(10000);
}
