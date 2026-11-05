#include <Arduino.h>
#include <Wire.h>
#include "Bmp180Compensation.h"
#include "Lcd1602.h"
#include "Sx127xDiagnostics.h"
#include "WeatherLoRaTx.h"
#include "WebDashboard.h"

constexpr uint8_t BMP_ADDRESS = 0x77;
constexpr uint32_t SAMPLE_INTERVAL_MS = 2000;
constexpr uint8_t OVERSAMPLING = 3;

bmp180::Calibration calibration{};
I2cBusRouter i2cBus;
bmp180::Measurement latestMeasurement{false, 0, 0};
bool sensorReady = false;
uint32_t lastSampleMs = 0;

const char *i2cError(uint8_t code) {
  switch (code) {
    case 1: return "tampon de transmission insuffisant";
    case 2: return "NACK : adresse non acquittee (capteur absent/debranche ?)";
    case 3: return "NACK : donnees non acquittees";
    case 4: return "erreur du bus I2C";
    case 5: return "delai I2C depasse (bus bloque ?)";
    default: return "erreur I2C inconnue";
  }
}

bool checkTransmission(uint8_t code, uint8_t reg) {
  if (code == 0) return true;
  Serial.printf("[ERREUR] I2C 0x77 registre 0x%02X : %s (code %u).\n",
                reg, i2cError(code), code);
  return false;
}

bool readRegisters(uint8_t reg, uint8_t *data, size_t count) {
  Wire.beginTransmission(BMP_ADDRESS);
  const size_t written = Wire.write(reg);
  // STOP accepte par le BMP180 ; permet de verifier aussi la selection du registre.
  const uint8_t status = Wire.endTransmission(true);
  if (!checkTransmission(status, reg)) return false;
  if (written != 1) {
    Serial.println("[ERREUR] I2C : impossible de preparer la lecture.");
    return false;
  }
  const size_t received = Wire.requestFrom(BMP_ADDRESS, count, true);
  if (received != count || Wire.available() != int(count)) {
    Serial.printf("[ERREUR] I2C lecture registre 0x%02X : %u/%u octets recus "
                  "(NACK, delai depasse ou lecture incomplete).\n",
                  reg, unsigned(received), unsigned(count));
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    const int value = Wire.read();
    if (value < 0) {
      Serial.printf("[ERREUR] I2C : octet manquant au registre 0x%02X.\n", reg);
      return false;
    }
    data[i] = uint8_t(value);
  }
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t bytes[] = {reg, value};
  Wire.beginTransmission(BMP_ADDRESS);
  const size_t written = Wire.write(bytes, sizeof(bytes));
  const uint8_t status = Wire.endTransmission(true);
  if (!checkTransmission(status, reg)) return false;
  if (written != sizeof(bytes)) {
    Serial.println("[ERREUR] I2C : ecriture incomplete dans le tampon.");
    return false;
  }
  return true;
}

bool startBus() {
  return i2cBus.select(I2cTarget::Sensor);
}

bool scanI2c() {
  Serial.println("[SCAN] Recherche I2C de 0x08 a 0x77...");
  unsigned found = 0;
  unsigned errors = 0;
  unsigned consecutiveBusErrors = 0;
  bool aborted = false;
  bool bmpFound = false;
  // Les adresses reservees 0x00..0x07 et 0x78..0x7F sont exclues.
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    const uint8_t status = Wire.endTransmission(true);
    if (status == 0) {
      consecutiveBusErrors = 0;
      ++found;
      Serial.printf("[SCAN] Peripherique a 0x%02X%s\n", address,
                    address == BMP_ADDRESS ? " (adresse BMP180 attendue)" : "");
      if (address == BMP_ADDRESS) bmpFound = true;
    } else if (status != 2) {  // NACK normal aux adresses inutilisees.
      ++errors;
      ++consecutiveBusErrors;
      Serial.printf("[ERREUR] Scan 0x%02X : %s (code %u).\n",
                    address, i2cError(status), status);
      if (consecutiveBusErrors >= 3) {
        aborted = true;
        Serial.println("[ERREUR] Scan interrompu : trois erreurs de bus consecutives.");
        Serial.printf("[DIAG] BMP180 : GPIO%d/SDA=%d, GPIO%d/SCL=%d (niveaux logiques).\n",
                      EnviroPins::BMP_SDA, digitalRead(EnviroPins::BMP_SDA),
                      EnviroPins::BMP_SCL, digitalRead(EnviroPins::BMP_SCL));
        break;
      }
    } else {
      consecutiveBusErrors = 0;
    }
    delay(1);
  }
  Serial.printf("[SCAN] %s : %u peripherique(s), %u erreur(s) de bus.\n",
                aborted ? "Incomplet" : "Termine", found, errors);
  if (aborted && !bmpFound) {
    Serial.println("[INFO] Presence a 0x77 non verifiee par le scan ; tentative directe au prochain cycle.");
  } else if (!bmpFound) {
    Serial.printf("[ERREUR] Aucun capteur a 0x77. Verifier 3,3 V, GND, SDA=%d et SCL=%d.\n",
                  EnviroPins::BMP_SDA, EnviroPins::BMP_SCL);
  }
  return bmpFound;
}

uint16_t wordAt(const uint8_t *data, size_t offset) {
  return (uint16_t(data[offset]) << 8) | data[offset + 1];
}

bool initializeSensor() {
  uint8_t chipId = 0;
  if (!readRegisters(0xD0, &chipId, 1)) return false;
  if (chipId != 0x55) {
    Serial.printf("[ERREUR] Identifiant a 0x77 : 0x%02X, attendu 0x55 (BMP180/BMP085).\n", chipId);
    return false;
  }
  uint8_t data[22];
  if (!readRegisters(0xAA, data, sizeof(data))) return false;
  bool allZero = true;
  bool allFF = true;
  for (uint8_t value : data) {
    allZero = allZero && value == 0;
    allFF = allFF && value == 0xFF;
  }
  calibration = {
    int16_t(wordAt(data, 0)), int16_t(wordAt(data, 2)), int16_t(wordAt(data, 4)),
    wordAt(data, 6), wordAt(data, 8), wordAt(data, 10),
    int16_t(wordAt(data, 12)), int16_t(wordAt(data, 14)), int16_t(wordAt(data, 16)),
    int16_t(wordAt(data, 18)), int16_t(wordAt(data, 20))
  };
  if (allZero || allFF || calibration.ac4 == 0 || calibration.ac5 == 0) {
    Serial.println("[ERREUR] Coefficients de calibration invalides : mesure abandonnee.");
    return false;
  }
  Serial.println("[OK] Capteur compatible BMP180, ID=0x55 ; calibration lue.");
  return true;
}

bool waitConversion(uint32_t timeoutMs) {
  const uint32_t startedMs = millis();
  do {
    delay(1);
    uint8_t control = 0;
    if (!readRegisters(0xF4, &control, 1)) return false;
    if ((control & 0x20) == 0) return true;
  } while (uint32_t(millis() - startedMs) < timeoutMs);
  Serial.println("[ERREUR] BMP180 : conversion non terminee dans le delai imparti.");
  return false;
}

bool measureAndPrint() {
  uint8_t raw[3];
  if (!writeRegister(0xF4, 0x2E) || !waitConversion(20) ||
      !readRegisters(0xF6, raw, 2)) return false;
  const uint16_t ut = wordAt(raw, 0);
  if (!writeRegister(0xF4, uint8_t(0x34 | (OVERSAMPLING << 6))) ||
      !waitConversion(50) || !readRegisters(0xF6, raw, 3)) return false;
  const uint32_t up = ((uint32_t(raw[0]) << 16) | (uint32_t(raw[1]) << 8) |
                       raw[2]) >> (8 - OVERSAMPLING);
  const auto result = bmp180::compensate(calibration, ut, up, OVERSAMPLING);
  if (!result.valid) {
    Serial.println("[ERREUR] BMP180 : calcul invalide ou mesure hors plage "
                   "(-40..85 deg C, 300..1100 hPa). Aucune valeur publiee.");
    return false;
  }
  latestMeasurement = result;
  Serial.printf("[OK] t=%lu ms | Temperature : %.1f °C | Pression : %.2f hPa\n",
                static_cast<unsigned long>(millis()), result.temperatureDeciC / 10.0,
                result.pressurePa / 100.0);
  return true;
}

void setup() {
  Serial.begin(115200);
  const uint32_t startedMs = millis();
  while (!Serial && uint32_t(millis() - startedMs) < 3000) delay(10);
#if ENVIRO_ENABLE_LORA_TX
  Serial.println("\n=== EnviroTerminal V1 : BMP180 + emetteur meteo LoRa ===");
#elif ENVIRO_ENABLE_LCD
  Serial.println("\n=== EnviroTerminal V1 : BMP180 + LCD 16x2 ===");
#elif ENVIRO_ENABLE_WEB
  Serial.println("\n=== EnviroTerminal V1 : BMP180 + LoRa SPI + site local ===");
#elif ENVIRO_ENABLE_LORA
  Serial.println("\n=== EnviroTerminal V1 : BMP180 + LoRa SPI ===");
#else
  Serial.println("\n=== EnviroTerminal V1 : diagnostic BMP180 seul ===");
#endif
  Serial.println("Alimentation capteur : 3,3 V ; mesures toutes les 2 secondes.");
  Serial.printf("BMP180 : SDA=GPIO%d, SCL=GPIO%d, 100 kHz, timeout=50 ms.\n",
                EnviroPins::BMP_SDA, EnviroPins::BMP_SCL);
#if ENVIRO_ENABLE_LCD
  Serial.println("LCD : SDA=GPIO3, SCL=GPIO2 ; niveaux I2C 3,3 V cote ESP32 requis.");
#else
  Serial.println("[INFO] LCD desactive.");
#endif
  if (startBus() && scanI2c()) sensorReady = initializeSensor();
  beginSx127xDiagnostic();
  beginWebDashboard();
  // Premiere mesure immediatement, puis cadence de 2 s (compatible debordement millis).
  lastSampleMs = millis() - SAMPLE_INTERVAL_MS;
}

void loop() {
  pollSx127xDiagnostic();
  pollWebDashboard();
  if (uint32_t(millis() - lastSampleMs) < SAMPLE_INTERVAL_MS) {
    delay(1);
    return;
  }
  lastSampleMs = millis();
  latestMeasurement = {false, 0, 0};
  if (!startBus()) {
    publishWebMeasurement(latestMeasurement);
    updateLcd1602(i2cBus, latestMeasurement);
    return;
  }
  if (!sensorReady) {
    Serial.println("[INFO] Nouvelle tentative de detection/calibration a 0x77...");
    sensorReady = initializeSensor();
    if (!sensorReady) {
      publishWebMeasurement(latestMeasurement);
      updateLcd1602(i2cBus, latestMeasurement);
      return;
    }
  }
  if (!measureAndPrint()) {
    sensorReady = false;
    // Reinitialiser aussi le controleur I2C apres un echec de mesure.
    i2cBus.reset();
    Serial.println("[INFO] Mesure abandonnee ; nouvelle tentative dans 2 s.");
  }
  publishWebMeasurement(latestMeasurement);
  publishWeatherLoRaTx(latestMeasurement);
  updateLcd1602(i2cBus, latestMeasurement);
}
