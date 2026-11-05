#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <math.h>
#include "../../WeatherRadioConfig.h"
#include "../../EnviroTerminalDiagnostic/WebDashboard.h"

// Brochage du recepteur confirme par l'utilisateur. Reception par sondage SPI :
// DIO0 est debranche pour liberer GPIO9/BOOT. Aucun capteur I2C n'est utilise.
constexpr int RADIO_SCK = 7;
constexpr int RADIO_MISO = 5;
constexpr int RADIO_MOSI = 6;
constexpr int RADIO_NSS = 8;
constexpr int RADIO_RESET = 10;
constexpr long RADIO_FREQUENCY = WeatherRadio::FREQUENCY_HZ;
static_assert(RADIO_FREQUENCY >= 137000000L && RADIO_FREQUENCY <= 525000000L,
              "Frequence hors plage SX1278");
constexpr uint32_t STATUS_INTERVAL_MS = 10000;
const SPISettings diagnosticSpi(100000, MSBFIRST, SPI_MODE0);
bool radioReady = false;
uint32_t lastStatusMs = 0;
uint32_t receivedPackets = 0;
uint32_t lastPacketMs = 0;
uint32_t rxDoneIrqs = 0, crcErrorIrqs = 0, rxTimeoutIrqs = 0;
uint32_t lastRxEventMs = 0;
uint8_t lastRxEventFlags = 0;
int lastRxEventRssi = 0;
float lastRxEventSnr = 0;
long lastRxEventFrequencyError = 0;

// Interface attendue par le tableau de bord commun, sans le diagnostic de la sonde.
bool sx127xSpiReady() { return radioReady; }

bool publishWeatherPacket(const uint8_t *payload, size_t count, int rssi, float snr) {
  StaticJsonDocument<384> document;
  const DeserializationError error = deserializeJson(document, payload, count);
  if (error) return false;
  if (!document["v"].is<int>() || document["v"].as<int>() != 1 ||
      !document["node"].is<const char *>() ||
      strcmp(document["node"].as<const char *>(), "meteo-1") != 0 ||
      !document["seq"].is<uint32_t>() || !document["uptime_ms"].is<uint32_t>() ||
      !document["temperature_c"].is<float>() || !document["pressure_hpa"].is<float>()) return false;
  const float temperature = document["temperature_c"].as<float>();
  const float pressure = document["pressure_hpa"].as<float>();
  if (!isfinite(temperature) || !isfinite(pressure) || temperature < -40 ||
      temperature > 85 || pressure < 300 || pressure > 1100) return false;
  const bmp180::Measurement measurement{true, int32_t(lroundf(temperature * 10)),
                                       int32_t(lroundf(pressure * 100))};
  publishWebRadioSignal(rssi, snr);
  publishWebMeasurement(measurement);
  return true;
}

uint8_t readRegister(uint8_t address) {
  SPI.beginTransaction(diagnosticSpi);
  digitalWrite(RADIO_NSS, LOW);
  SPI.transfer(address & 0x7F);
  const uint8_t value = SPI.transfer(0);
  digitalWrite(RADIO_NSS, HIGH);
  SPI.endTransaction();
  return value;
}

void writeRegister(uint8_t address, uint8_t value) {
  SPI.beginTransaction(diagnosticSpi);
  digitalWrite(RADIO_NSS, LOW);
  SPI.transfer(address | 0x80);
  SPI.transfer(value);
  digitalWrite(RADIO_NSS, HIGH);
  SPI.endTransaction();
}

String radioDiagnosticJson() {
  const uint8_t version = readRegister(0x42);
  const uint8_t mode = readRegister(0x01);
  const uint8_t modem1 = readRegister(0x1D);
  const uint8_t modem2 = readRegister(0x1E);
  const uint8_t sync = readRegister(0x39);
  const uint32_t frequency = (uint32_t(readRegister(0x06)) << 16) |
                             (uint32_t(readRegister(0x07)) << 8) | readRegister(0x08);
  const uint32_t nominalHz = uint32_t((uint64_t(frequency) * 32000000ULL) >> 19);
  const bool valid = WeatherRadio::rxConfigurationValid(version, mode, modem1, modem2, sync, frequency);
  String json;
  json.reserve(1100);
  json = "{\"configuration_ok\":" + String(valid ? "true" : "false");
  json += ",\"radio_ready\":" + String(radioReady ? "true" : "false");
  json += ",\"version\":" + String(version);
  json += ",\"op_mode\":" + String(mode);
  json += ",\"modem_config_1\":" + String(modem1);
  json += ",\"modem_config_2\":" + String(modem2);
  json += ",\"sync_word\":" + String(sync);
  json += ",\"frequency_word\":" + String(frequency);
  json += ",\"nominal_frequency_hz\":" + String(nominalHz);
  json += ",\"expected_frequency_hz\":" + String(RADIO_FREQUENCY);
  json += ",\"irq_flags\":" + String(readRegister(0x12));
  json += ",\"modem_status\":" + String(readRegister(0x18));
  json += ",\"current_rssi_dbm\":" + String(int(readRegister(0x1B)) - 164);
  json += ",\"valid_header_count\":" + String((uint16_t(readRegister(0x14)) << 8) | readRegister(0x15));
  json += ",\"hardware_packet_count\":" + String((uint16_t(readRegister(0x16)) << 8) | readRegister(0x17));
  json += ",\"iq_register\":" + String(readRegister(0x33));
  json += ",\"iq_register_2\":" + String(readRegister(0x3B));
  json += ",\"received_packets\":" + String(receivedPackets);
  json += ",\"rx_done_irq_count\":" + String(rxDoneIrqs);
  json += ",\"crc_error_irq_count\":" + String(crcErrorIrqs);
  json += ",\"rx_timeout_irq_count\":" + String(rxTimeoutIrqs);
  json += ",\"last_rx_event_flags\":" + String(lastRxEventFlags);
  json += ",\"last_rx_event_age_ms\":" + (rxDoneIrqs ? String(uint32_t(millis() - lastRxEventMs)) : String("null"));
  json += ",\"last_rx_event_rssi_dbm\":" + (rxDoneIrqs ? String(lastRxEventRssi) : String("null"));
  json += ",\"last_rx_event_snr_db\":" + (rxDoneIrqs ? String(lastRxEventSnr) : String("null"));
  json += ",\"last_rx_event_frequency_error_hz\":" + (rxDoneIrqs ? String(lastRxEventFrequencyError) : String("null")) + "}";
  return json;
}

void observeReceiveIrqs() {
  // Use the exact snapshot already read by parsePacket, before it clears IRQs.
  // No additional receive command or IRQ acknowledgement is issued here.
  const uint8_t flags = LoRa.lastParseIrqFlags();
  if (flags & 0x80) ++rxTimeoutIrqs;
  if (flags & 0x20) ++crcErrorIrqs;
  if (!(flags & 0x40)) return;
  ++rxDoneIrqs;
  lastRxEventMs = millis();
  lastRxEventFlags = flags;
  lastRxEventRssi = LoRa.packetRssi();
  lastRxEventSnr = LoRa.packetSnr();
  lastRxEventFrequencyError = LoRa.packetFrequencyError();
  Serial.printf("[RADIO] RX IRQ=0x%02X CRC_erreurs=%lu RSSI=%d dBm SNR=%.2f dB erreur_freq=%ld Hz.\n",
                flags, static_cast<unsigned long>(crcErrorIrqs), lastRxEventRssi,
                lastRxEventSnr, lastRxEventFrequencyError);
}

bool startRadio() {
  pinMode(RADIO_RESET, OUTPUT);
  digitalWrite(RADIO_RESET, LOW);
  delay(10);
  digitalWrite(RADIO_RESET, HIGH);
  delay(10);
  writeRegister(0x01, 0x08);
  delay(1);
  LoRa.setPins(RADIO_NSS, -1, -1);
  LoRa.setSPIFrequency(100000);
  if (!LoRa.begin(RADIO_FREQUENCY)) {
    Serial.printf("[ERREUR] XL1278 SPI : RegVersion=0x%02X, attendu 0x12.\n",
                  readRegister(0x42));
    Serial.println("Verifier 3V3/GND, SCK=7, MISO=5, MOSI=6, NSS=8, RESET=10 ; DIO0 debranche.");
    return false;
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(WeatherRadio::BANDWIDTH_HZ);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.disableInvertIQ();
  const uint8_t version = readRegister(0x42);
  const uint8_t mode = readRegister(0x01);
  const uint8_t config = readRegister(0x1D);
  if (version != 0x12 || (mode & 0x8F) != 0x89 || config != 0x42) {
    Serial.printf("[ERREUR] Configuration RX non verifiee : version=0x%02X mode=0x%02X modem=0x%02X.\n",
                  version, mode, config);
    return false;
  }
  // parsePacket() configure la reception et lit les IRQ en SPI, sans DIO0.
  LoRa.parsePacket();
  Serial.printf("[OK] SX127x version=0x%02X ; reception a %ld Hz.\n",
                readRegister(0x42), RADIO_FREQUENCY);
  Serial.println("[RX] SF7, BW=31250 Hz, CR=4/5, preambule=8, sync=0x12, CRC, entete explicite.");
  Serial.println("[RX] En attente de la sonde ; ce firmware n'emet aucun paquet.");
  return true;
}

void printPacket(int packetSize) {
  const int rssi = LoRa.packetRssi();
  const float snr = LoRa.packetSnr();
  // La FIFO est bornee a 255 octets. Pas de String ni de format printf fourni
  // par la radio : les octets non imprimables sont affiches en hexadecimal.
  uint8_t payload[255];
  size_t count = 0;
  while (LoRa.available() && count < sizeof(payload)) {
    const int value = LoRa.read();
    if (value < 0) break;
    payload[count++] = uint8_t(value);
  }
  if (count != size_t(packetSize)) {
    Serial.println("[ERREUR] Lecture du paquet incomplete ; paquet ignore.");
    return;
  }
  ++receivedPackets;
  lastPacketMs = millis();
  Serial.printf("[RX] paquet=%lu t=%lu ms taille=%u RSSI=%d dBm SNR=%.2f dB | ",
                static_cast<unsigned long>(receivedPackets),
                static_cast<unsigned long>(lastPacketMs), unsigned(count), rssi, snr);
  for (size_t i = 0; i < count; ++i) {
    if (payload[i] >= 32 && payload[i] <= 126 && payload[i] != '\\') {
      Serial.write(payload[i]);
    } else {
      Serial.printf("\\x%02X", unsigned(payload[i]));
    }
  }
  Serial.println();
  if (publishWeatherPacket(payload, count, rssi, snr)) {
    Serial.println("[WEB] Mesure LoRa valide publiee sur le tableau de bord.");
  } else {
    Serial.println("[RX] Paquet ignore par le tableau de bord : format, sonde ou mesure invalide.");
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && uint32_t(millis() - started) < 3000) delay(10);
  Serial.println("\n=== Recepteur meteo ESP32-C3 / XL1278-SMT ===");
  Serial.printf("[INFO] Frequence=%ld Hz.\n", RADIO_FREQUENCY);
  Serial.println("[SPI] SCK=7 MISO=5 MOSI=6 NSS=8 RESET=10 ; DIO0 debranche.");
  pinMode(RADIO_NSS, OUTPUT);
  digitalWrite(RADIO_NSS, HIGH);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
  radioReady = startRadio();
  setWebRadioDiagnosticProvider(radioDiagnosticJson);
  beginWebDashboard();
  lastStatusMs = millis();
}

void loop() {
  pollWebDashboard();
  if (radioReady) {
    const int packetSize = LoRa.parsePacket();
    observeReceiveIrqs();
    if (packetSize > 0) printPacket(packetSize);
  }
  if (uint32_t(millis() - lastStatusMs) >= STATUS_INTERVAL_MS) {
    lastStatusMs = millis();
    if (radioReady && readRegister(0x42) != 0x12) {
      radioReady = false;
      Serial.println("[ERREUR] Communication SPI perdue.");
    }
    if (!radioReady) {
      Serial.println("[INFO] Nouvelle tentative de detection du module...");
      radioReady = startRadio();
    } else if (receivedPackets == 0) {
      Serial.printf("[RX] SPI OK ; aucun paquet recu pour le moment (mode=0x%02X).\n", readRegister(0x01));
    } else {
      Serial.printf("[RX] SPI OK ; %lu paquet(s), dernier il y a %lu ms.\n",
                    static_cast<unsigned long>(receivedPackets),
                    static_cast<unsigned long>(uint32_t(millis() - lastPacketMs)));
    }
  }
  delay(1);
}
