#include "Sx127xDiagnostics.h"

#if ENVIRO_ENABLE_LORA_TX
#include "WeatherLoRaTx.h"
void beginSx127xDiagnostic() { beginWeatherLoRaTx(); }
void pollSx127xDiagnostic() { pollWeatherLoRaTx(); }
bool sx127xSpiReady() { return weatherLoRaTxReady(); }
#elif ENVIRO_ENABLE_LORA
#include <Arduino.h>
#include <SPI.h>
#include "I2cBusRouter.h"

namespace {
constexpr int RADIO_SCK = 6;
constexpr int RADIO_MISO = 5;
constexpr int RADIO_MOSI = 7;
constexpr int RADIO_NSS = 10;
constexpr int RADIO_RESET = 0;
constexpr int RADIO_DIO0 = 1;  // Reserve pour les futurs evenements RX/TX.
constexpr uint8_t REG_OP_MODE = 0x01;
constexpr uint8_t REG_VERSION = 0x42;
constexpr uint8_t EXPECTED_VERSION = 0x12;
constexpr uint32_t CHECK_INTERVAL_MS = 10000;
const SPISettings radioSpiSettings(1000000, MSBFIRST, SPI_MODE0);

static_assert(EnviroPins::BMP_SDA != RADIO_SCK && EnviroPins::BMP_SDA != RADIO_MISO &&
              EnviroPins::BMP_SDA != RADIO_MOSI && EnviroPins::BMP_SDA != RADIO_NSS &&
              EnviroPins::BMP_SDA != RADIO_RESET && EnviroPins::BMP_SDA != RADIO_DIO0 &&
              EnviroPins::BMP_SCL != RADIO_SCK && EnviroPins::BMP_SCL != RADIO_MISO &&
              EnviroPins::BMP_SCL != RADIO_MOSI && EnviroPins::BMP_SCL != RADIO_NSS &&
              EnviroPins::BMP_SCL != RADIO_RESET && EnviroPins::BMP_SCL != RADIO_DIO0,
              "Les broches BMP180 et radio doivent etre distinctes");

bool ready = false;
uint32_t lastCheckMs = 0;

uint8_t transferRegister(uint8_t command, uint8_t value) {
  SPI.beginTransaction(radioSpiSettings);
  digitalWrite(RADIO_NSS, LOW);
  SPI.transfer(command);
  const uint8_t response = SPI.transfer(value);
  digitalWrite(RADIO_NSS, HIGH);
  SPI.endTransaction();
  return response;
}

uint8_t readRadioRegister(uint8_t address) {
  return transferRegister(address & 0x7F, 0);
}

void writeRadioRegister(uint8_t address, uint8_t value) {
  transferRegister(address | 0x80, value);
}

bool detectRadio() {
  digitalWrite(RADIO_RESET, LOW);
  delay(10);
  digitalWrite(RADIO_RESET, HIGH);
  delay(10);
  const uint8_t first = readRadioRegister(REG_VERSION);
  const uint8_t second = readRadioRegister(REG_VERSION);
  if (first != EXPECTED_VERSION || second != EXPECTED_VERSION) {
    Serial.printf("[ERREUR] LoRa SPI : RegVersion(0x42)=0x%02X/0x%02X, attendu 0x12.\n",
                  first, second);
    Serial.println("[INFO] Verifier VCC=3,3 V, GND, NSS=10, SCK=6, MISO=5, MOSI=7, RESET=0.");
    return false;
  }
  // Le reset laisse la famille SX127x en FSK. Seul le mode sommeil est ecrit :
  // aucun reglage de frequence, aucune commande TX ou RX dans ce diagnostic.
  writeRadioRegister(REG_OP_MODE, 0x00);
  delay(1);
  const uint8_t mode = readRadioRegister(REG_OP_MODE);
  if ((mode & 0x87) != 0 || readRadioRegister(REG_VERSION) != EXPECTED_VERSION) {
    Serial.printf("[ERREUR] LoRa SPI : verification du mode veille echouee, RegOpMode=0x%02X.\n", mode);
    return false;
  }
  Serial.println("[OK] LoRa : interface SPI SX127x detectee, version=0x12, mode veille verifie.");
  Serial.println("[INFO] Version 0x12 partagee par plusieurs SX127x ; modele et bande RF non identifies par ce registre.");
  Serial.println("[INFO] Diagnostic SPI uniquement : aucune emission, aucune reception radio configuree.");
  return true;
}
}  // namespace

void beginSx127xDiagnostic() {
  pinMode(RADIO_NSS, OUTPUT);
  digitalWrite(RADIO_NSS, HIGH);
  pinMode(RADIO_RESET, OUTPUT);
  digitalWrite(RADIO_RESET, HIGH);
  pinMode(RADIO_DIO0, INPUT);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
  Serial.println("[LoRa] SPI : SCK=GPIO6, MISO=GPIO5, MOSI=GPIO7, NSS=GPIO10, RESET=GPIO0.");
  ready = detectRadio();
  lastCheckMs = millis();
}

void pollSx127xDiagnostic() {
  if (uint32_t(millis() - lastCheckMs) < CHECK_INTERVAL_MS) return;
  lastCheckMs = millis();
  if (!ready) {
    Serial.println("[LoRa] Nouvelle tentative de detection SPI...");
    ready = detectRadio();
    return;
  }
  const uint8_t version = readRadioRegister(REG_VERSION);
  const uint8_t mode = readRadioRegister(REG_OP_MODE);
  if (version != EXPECTED_VERSION || (mode & 0x87) != 0) {
    ready = false;
    Serial.printf("[ERREUR] LoRa : controle SPI/veille invalide, version=0x%02X, mode=0x%02X.\n", version, mode);
    return;
  }
  Serial.println("[LoRa] Controle SPI OK ; module en veille.");
}
bool sx127xSpiReady() { return ready; }
#else
void beginSx127xDiagnostic() {}
void pollSx127xDiagnostic() {}
bool sx127xSpiReady() { return false; }
#endif
