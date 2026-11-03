#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#ifndef SOIL_LORA_TX
#define SOIL_LORA_TX 0
#endif
#if SOIL_LORA_TX
#include <SPI.h>
#include <RadioLib.h>
#include <PlantSoilProtocol.h>
#include "BoundedUsbLog.h"
#endif

#if SOIL_LORA_TX
BoundedUsbLog diagnosticLog;
#define SOIL_LOG diagnosticLog
#else
#define SOIL_LOG Serial
#endif

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
#if SOIL_LORA_TX
SX1262 radio(new Module(4, 1, 2, 3, SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0)));
bool radioReady = false, sessionReady = false, transmitted = false;
uint32_t radioAttempt = 0, lastTransmit = 0;
plantsoil::Packet current;

bool makeSession(uint32_t& session) {
  session = 0;
  NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Enabled << RNG_CONFIG_DERCEN_Pos;
  NRF_RNG->EVENTS_VALRDY = 0;
  NRF_RNG->TASKS_START = 1;
  const uint32_t started = millis();
  uint8_t bytes = 0;
  while (bytes < 4 && static_cast<uint32_t>(millis() - started) < 25) {
    if (NRF_RNG->EVENTS_VALRDY) {
      const uint8_t value = NRF_RNG->VALUE;
      NRF_RNG->EVENTS_VALRDY = 0;
      session |= static_cast<uint32_t>(value) << (8 * bytes++);
    }
    yield();
  }
  NRF_RNG->TASKS_STOP = 1;
  return bytes == 4 && session != 0;
}
void initializeRadio() {
  radioAttempt = millis();
  radioReady = false;
  if (!sessionReady) sessionReady = makeSession(current.session);
  if (!sessionReady) {
    SOIL_LOG.println(F("[SOIL_RADIO] error=session_rng_timeout authenticated=no"));
    return;
  }
  // SPI explicite : le coeur ne touche pas SS/D7, deja occupee par SCL ADS.
  SPI.begin();
  radio.setRfSwitchPins(5, RADIOLIB_NC);
  int16_t code = radio.begin(868.1f, 125.0f, 7, 5,
      RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 0, 8, 1.8f, false);
  if (code == RADIOLIB_ERR_NONE) code = radio.setDio2AsRfSwitch(true);
  if (code == RADIOLIB_ERR_NONE) code = radio.setCRC(2);
  // RadioLib renvoie des microsecondes : refuser plus de 1 % sur 30 s.
  if (code == RADIOLIB_ERR_NONE && radio.getTimeOnAir(plantsoil::kPacketSize) > 300000) {
    SOIL_LOG.println(F("[SOIL_RADIO] error=airtime_exceeds_1_percent authenticated=no"));
    return;
  }
  radioReady = code == RADIOLIB_ERR_NONE;
  SOIL_LOG.print(F("[SOIL_RADIO] role=TX authenticated=no code=")); SOIL_LOG.println(code);
}
void serviceTransmit(bool valid, int16_t raw) {
  const uint32_t now = millis();
  if (!radioReady && static_cast<uint32_t>(now - radioAttempt) >= 10000) initializeRadio();
  if (transmitted && static_cast<uint32_t>(now - lastTransmit) < plantsoil::kTransmitMs) return;
  lastTransmit = now; transmitted = true; // aucun rattrapage ni renvoi immediat apres erreur
  ++current.sequence;
  current.uptimeMs = now;
  current.address = adsAddress;
  current.status = !adsAddress ? plantsoil::AdcStatus::Unavailable :
      !valid ? plantsoil::AdcStatus::ReadError :
      raw < -8 || raw > 26400 ? plantsoil::AdcStatus::RangeError : plantsoil::AdcStatus::Ok;
  current.raw = current.status == plantsoil::AdcStatus::Ok ? raw : 0;
  if (!radioReady) {
    SOIL_LOG.println(F("[SOIL_TX_ERROR] radio_unavailable authenticated=no"));
    return;
  }
  uint8_t bytes[plantsoil::kPacketSize];
  if (plantsoil::encode(current, bytes, sizeof(bytes)) != plantsoil::Result::Accepted) {
    SOIL_LOG.println(F("[SOIL_TX_ERROR] invalid_packet authenticated=no"));
    return;
  }
  const int16_t code = radio.transmit(bytes, sizeof(bytes));
  char line[240];
  if (code == RADIOLIB_ERR_NONE) {
    if (plantsoil::formatPacket(line, sizeof(line), "TX", current)) SOIL_LOG.println(line);
  } else {
    SOIL_LOG.print(F("[SOIL_TX_ERROR] code=")); SOIL_LOG.println(code);
    radioReady = false; radioAttempt = millis();
  }
}
#endif

void scanBus() {
  adsAddress = 0;
  bus.release();
  delay(1);
  SOIL_LOG.print(F("[BUS] SDA=")); SOIL_LOG.print(digitalRead(kSda));
  SOIL_LOG.print(F(" SCL=")); SOIL_LOG.println(digitalRead(kScl));
  if (digitalRead(kSda) == LOW || digitalRead(kScl) == LOW) {
    SOIL_LOG.println(F("[ERREUR] Ligne I2C maintenue basse : verifier cablage et alimentation."));
    return;
  }
  uint8_t found = 0;
  uint8_t candidates = 0, candidate = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    if (bus.probe(address)) {
      ++found;
      SOIL_LOG.print(F("[I2C] ACK adresse=0x")); SOIL_LOG.println(address, HEX);
      if (address >= 0x48 && address <= 0x4B) {
        ++candidates;
        candidate = address;
      }
    }
  }
  SOIL_LOG.print(F("[SCAN] peripheriques=")); SOIL_LOG.println(found);
  SOIL_LOG.println(F("[INFO] Un ACK seul ne permet pas d'identifier la puce."));
  if (candidates == 1) {
    adsAddress = candidate;
    SOIL_LOG.print(F("[ADC] adresse_selectionnee=0x")); SOIL_LOG.println(adsAddress, HEX);
    if (adsAddress != 0x48) {
      SOIL_LOG.println(F("[INFO] Adresse differente de 0x48 : verifier le strap ADDR hors tension."));
    }
  } else {
    SOIL_LOG.println(F("[ERREUR] Aucun candidat ADS unique entre 0x48 et 0x4B ; aucune configuration envoyee."));
  }
}

bool readA0(int16_t& raw) {
  if (!adsAddress) return false;
  if (!bus.writeRegister(adsAddress, 1, kConfig)) {
    SOIL_LOG.println(F("[ERREUR] Ecriture configuration ADC : NACK ou bus bloque."));
    return false;
  }
  const uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < 40) {
    delay(2);
    uint16_t config = 0;
    if (!bus.readRegister(adsAddress, 1, config)) {
      SOIL_LOG.println(F("[ERREUR] Lecture configuration interrompue."));
      return false;
    }
    if ((config & 0x7FFF) != (kConfig & 0x7FFF)) {
      SOIL_LOG.println(F("[ERREUR] Configuration relue incompatible avec le diagnostic ADS1115."));
      return false;
    }
    if (config & 0x8000) {
      uint16_t conversion = 0;
      if (!bus.readRegister(adsAddress, 0, conversion)) {
        SOIL_LOG.println(F("[ERREUR] Lecture conversion A0 interrompue."));
        return false;
      }
      raw = static_cast<int16_t>(conversion);
      return true;
    }
  }
  SOIL_LOG.println(F("[ERREUR] Conversion non terminee apres 40 ms."));
  return false;
}
void announce() {
  SOIL_LOG.println(F("\n[XIAO-SOIL-DIAG] v1 / USB / D6=P1.11 SDA / D7=P1.12 SCL"));
  SOIL_LOG.println(F("[CONFIG] ADS1115 attendu 0x48, selection unique 0x48..0x4B, A0/GND, +/-4.096 V, 128 SPS."));
#if SOIL_LORA_TX
  SOIL_LOG.println(F("[INFO] D0 relachee ; SOIL-01 LoRa 868.1 MHz / 0 dBm / 30 s ; diagnostic non authentifie ; aucun pourcentage."));
#else
  SOIL_LOG.println(F("[INFO] D0 relachee ; radio non initialisee ; aucun pourcentage sans etalonnage."));
#endif
  if (!correctPinMap) {
    SOIL_LOG.println(F("[ERREUR] Variante Arduino incompatible : test GPIO interdit."));
  }
}
}  // namespace

void setup() {
  // D0 reste une entree sans tirage : aucune commande du bouton/P-MOS.
  pinMode(0, INPUT);
  correctPinMap = g_ADigitalPinMap[kSda] == 43 && g_ADigitalPinMap[kScl] == 44;
  if (correctPinMap) bus.release();
  Serial.begin(115200);
#if SOIL_LORA_TX
  initializeRadio();
#endif
}

void loop() {
  delay(1000);
  if (!Serial) {
    usbWasConnected = false;
#if !SOIL_LORA_TX
    return;
#endif
  }
  if (Serial && !usbWasConnected) {
    announce();
    usbWasConnected = true;
    if (correctPinMap) scanBus();
  }
  if (!correctPinMap) {
#if SOIL_LORA_TX
    serviceTransmit(false, 0);
#endif
    return;
  }
  ++attempts;
  if (attempts == 1 || attempts % 10 == 0) scanBus();
  int16_t raw = 0;
  const bool valid = readA0(raw);
  if (valid) {
    ++readings;
    const float volts = raw * 0.000125f;
    SOIL_LOG.print(F("[MESURE] t_ms=")); SOIL_LOG.print(millis());
    SOIL_LOG.print(F(" adresse=0x")); SOIL_LOG.print(adsAddress, HEX);
    SOIL_LOG.print(F(" A0_brut=")); SOIL_LOG.print(raw);
    SOIL_LOG.print(F(" A0_V=")); SOIL_LOG.print(volts, 6);
    SOIL_LOG.print(F(" etat="));
    // Quelques codes negatifs peuvent venir de l'offset, meme avec A0 a GND.
    if (raw < -8 || volts > 3.3f) SOIL_LOG.println(F("HORS_PLAGE_NOMINALE"));
    else if (raw < 0) SOIL_LOG.println(F("PROCHE_ZERO_OFFSET_POSSIBLE"));
    else SOIL_LOG.println(F("DANS_PLAGE_NOMINALE"));
  } else {
    ++errors;
  }
  SOIL_LOG.print(F("[BILAN] essais=")); SOIL_LOG.print(attempts);
  SOIL_LOG.print(F(" mesures=")); SOIL_LOG.print(readings);
  SOIL_LOG.print(F(" erreurs=")); SOIL_LOG.println(errors);
#if SOIL_LORA_TX
  serviceTransmit(valid, raw);
#endif
}
