#include "WeatherLoRaTx.h"
#include "Sx127xDiagnostics.h"

#if ENVIRO_ENABLE_LORA_TX
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "I2cBusRouter.h"
#include "../WeatherRadioConfig.h"
namespace {
// Brochage de la sonde d'origine, distinct du nouveau recepteur.
constexpr int SCK_PIN = 6, MISO_PIN = 5, MOSI_PIN = 7, NSS_PIN = 10, RESET_PIN = 0;
constexpr uint32_t SEND_INTERVAL_MS = WeatherRadio::SEND_INTERVAL_MS;
constexpr uint32_t TX_TIMEOUT_MS = 2000;
constexpr uint32_t RETRY_INTERVAL_MS = 10000;
constexpr uint32_t HEALTH_INTERVAL_MS = 10000;
static_assert(EnviroPins::BMP_SDA == 4 && EnviroPins::BMP_SCL == 3,
              "Profil emetteur prevu avec BMP180 sur GPIO4/3");
static_assert(WEATHER_FREQUENCY_HZ >= 137000000L && WEATHER_FREQUENCY_HZ <= 525000000L,
              "Frequence hors plage SX1278");
bool ready = false;
bool transmitting = false;
uint32_t lastAttemptMs = 0, lastSendMs = 0, txStartedMs = 0, sequence = 0;
uint32_t lastHealthMs = 0;
const SPISettings radioSpi(100000, MSBFIRST, SPI_MODE0);

uint8_t readRadioRegister(uint8_t address) {
  SPI.beginTransaction(radioSpi);
  digitalWrite(NSS_PIN, LOW);
  SPI.transfer(address & 0x7F);
  const uint8_t value = SPI.transfer(0);
  digitalWrite(NSS_PIN, HIGH);
  SPI.endTransaction();
  return value;
}

void writeRadioRegister(uint8_t address, uint8_t value) {
  SPI.beginTransaction(radioSpi);
  digitalWrite(NSS_PIN, LOW);
  SPI.transfer(address | 0x80);
  SPI.transfer(value);
  digitalWrite(NSS_PIN, HIGH);
  SPI.endTransaction();
}

bool verifyIdleRadio() {
  const uint8_t version = readRadioRegister(0x42);
  const uint8_t mode = readRadioRegister(0x01);
  const uint8_t modem1 = readRadioRegister(0x1D);
  const uint8_t modem2 = readRadioRegister(0x1E);
  const uint8_t sync = readRadioRegister(0x39);
  const uint32_t frequency = (uint32_t(readRadioRegister(0x06)) << 16) |
                             (uint32_t(readRadioRegister(0x07)) << 8) |
                             readRadioRegister(0x08);
  const uint8_t paConfig = readRadioRegister(0x09);
  const uint8_t paDac = readRadioRegister(0x4D);
  if (WeatherRadio::txConfigurationValid(version, mode, modem1, modem2, sync, frequency) &&
      WeatherRadio::txPowerRegistersValid(paConfig, paDac)) return true;
  Serial.printf("[ERREUR] LoRa TX : configuration perdue, version=0x%02X mode=0x%02X modem=%02X/%02X sync=%02X frequence=0x%06lX PA=%02X DAC=%02X.\n",
                version, mode, modem1, modem2, sync, static_cast<unsigned long>(frequency), paConfig, paDac);
  return false;
}

bool startRadio() {
  // Reprendre aussi le controleur SPI, pas seulement le reset du module.
  // Le bus est reserve a la radio ; les mesures BMP180 restent sur Wire.
  pinMode(NSS_PIN, OUTPUT);
  digitalWrite(NSS_PIN, HIGH);
  SPI.end();
  delay(1);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, NSS_PIN);
  LoRa.setSPI(SPI);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  delay(10);
  digitalWrite(RESET_PIN, HIGH);
  delay(10);
  const uint8_t version = readRadioRegister(0x42);
  const uint8_t secondVersion = readRadioRegister(0x42);
  Serial.printf("[TX] Apres reset SPI/radio : version=0x%02X/0x%02X, mode=0x%02X.\n",
                version, secondVersion, readRadioRegister(0x01));
  if (version != 0x12 || secondVersion != 0x12) {
    Serial.println("[ERREUR] LoRa TX : identification SPI invalide ; nouvelle tentative dans 10 s.");
    return false;
  }
  // Entrer d'abord en sommeil FSK avant la selection du mode LoRa.
  writeRadioRegister(0x01, 0x00);
  delay(10);
  const uint8_t sleepFsk = readRadioRegister(0x01);
  writeRadioRegister(0x01, 0x08);
  delay(10);
  const uint8_t sleepLf = readRadioRegister(0x01);
  writeRadioRegister(0x01, 0x88);
  delay(10);
  const uint8_t sleepLoRa = readRadioRegister(0x01);
  Serial.printf("[TX] Modes ecrits/relus : 00/%02X 08/%02X 88/%02X.\n", sleepFsk, sleepLf, sleepLoRa);
  if ((sleepLoRa & 0x8F) != 0x88) {
    Serial.println("[ERREUR] LoRa TX : ecriture SPI du mode non acquittee ; aucune emission.");
    return false;
  }
  LoRa.setPins(NSS_PIN, -1, -1);
  LoRa.setSPIFrequency(100000);
  if (!LoRa.begin(WEATHER_FREQUENCY_HZ)) {
    Serial.println("[ERREUR] LoRa TX : module absent sur SPI 6/5/7/10, RESET=0.");
    return false;
  }
  Serial.printf("[TX] Apres begin : version=0x%02X, mode=0x%02X.\n",
                readRadioRegister(0x42), readRadioRegister(0x01));
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(WeatherRadio::BANDWIDTH_HZ);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.disableInvertIQ();
  LoRa.setTxPower(WeatherRadio::TX_POWER_DBM);
  Serial.printf("[TX] Puissance : consigne=%d dBm, PA=0x%02X DAC=0x%02X.\n",
                WeatherRadio::TX_POWER_DBM, readRadioRegister(0x09), readRadioRegister(0x4D));
  LoRa.sleep();
  Serial.printf("[TX] Diagnostic : version=0x%02X, mode=0x%02X.\n",
                readRadioRegister(0x42), readRadioRegister(0x01));
  if (!verifyIdleRadio()) return false;
  Serial.printf("[OK] LoRa TX pret : %ld Hz, SF7, BW31.25kHz, CR4/5, sync=0x12, CRC, %d dBm.\n",
                long(WEATHER_FREQUENCY_HZ), WeatherRadio::TX_POWER_DBM);
  Serial.println("[TX] Premiere mesure valide puis au plus une emission toutes les deux minutes.");
  return true;
}
}  // namespace

void beginWeatherLoRaTx() {
  pinMode(NSS_PIN, OUTPUT);
  digitalWrite(NSS_PIN, HIGH);
  ready = startRadio();
  lastAttemptMs = millis();
  lastHealthMs = millis();
  lastSendMs = millis() - SEND_INTERVAL_MS;
}

void pollWeatherLoRaTx() {
  if (transmitting) {
    if (readRadioRegister(0x42) != 0x12) {
      transmitting = false;
      ready = false;
      lastAttemptMs = millis();
      Serial.println("[ERREUR] LoRa TX : communication SPI perdue.");
    } else if ((readRadioRegister(0x12) & 0x08) != 0) {  // RegIrqFlags : TxDone
      transmitting = false;
      LoRa.sleep();
      Serial.println("[TX] Emission terminee (sans acquittement du recepteur).");
    } else if (uint32_t(millis() - txStartedMs) >= TX_TIMEOUT_MS) {
      LoRa.sleep();
      transmitting = false;
      ready = false;
      lastAttemptMs = millis();
      Serial.println("[ERREUR] LoRa TX : delai de fin d'emission depasse.");
    }
  }
  if (ready && !transmitting && uint32_t(millis() - lastHealthMs) >= HEALTH_INTERVAL_MS) {
    lastHealthMs = millis();
    if (!verifyIdleRadio()) {
      ready = false;
      lastAttemptMs = millis();
      Serial.println("[TX] Reprise SPI/radio planifiee ; Wi-Fi et mesures capteur maintenus.");
    }
  }
  if (!ready && uint32_t(millis() - lastAttemptMs) >= RETRY_INTERVAL_MS) {
    lastAttemptMs = millis();
    ready = startRadio();
    lastHealthMs = millis();
  }
}

bool weatherLoRaTxReady() { return ready; }

void publishWeatherLoRaTx(const bmp180::Measurement &measurement) {
  if (!measurement.valid || !ready || transmitting ||
      uint32_t(millis() - lastSendMs) < SEND_INTERVAL_MS) return;
  char payload[160];
  const int count = snprintf(payload, sizeof(payload),
      "{\"v\":1,\"node\":\"meteo-1\",\"seq\":%lu,\"uptime_ms\":%lu,"
      "\"temperature_c\":%.1f,\"pressure_hpa\":%.2f}",
      static_cast<unsigned long>(sequence), static_cast<unsigned long>(millis()),
      measurement.temperatureDeciC / 10.0, measurement.pressurePa / 100.0);
  if (count <= 0 || size_t(count) >= sizeof(payload)) {
    Serial.printf("[ERREUR] LoRa TX : format JSON invalide (%d octets).\n", count);
    return;
  }
  if (!LoRa.beginPacket()) {
    Serial.printf("[ERREUR] LoRa TX : radio occupee, version=0x%02X, mode=0x%02X, IRQ=0x%02X.\n",
                  readRadioRegister(0x42), readRadioRegister(0x01), readRadioRegister(0x12));
    ready = false;
    lastAttemptMs = millis();
    return;
  }
  const size_t written = LoRa.write(reinterpret_cast<const uint8_t *>(payload), size_t(count));
  if (written != size_t(count)) {
    LoRa.sleep();
    Serial.println("[ERREUR] LoRa TX : paquet incomplet, emission abandonnee.");
    return;
  }
  // Pas d'attente bloquante : le BMP180 et le reste de la boucle continuent.
  LoRa.endPacket(true);
  transmitting = true;
  txStartedMs = millis();
  lastSendMs = millis();
  ++sequence;
  Serial.printf("[TX] %s\n", payload);
}
#else
void beginWeatherLoRaTx() {}
void pollWeatherLoRaTx() {}
bool weatherLoRaTxReady() { return false; }
void publishWeatherLoRaTx(const bmp180::Measurement &) {}
#endif
