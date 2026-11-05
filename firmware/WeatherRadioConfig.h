#pragma once
#include <stdint.h>

// Canal commun aux deux cartes XL1278-SMT 433 MHz.
// BW 31,25 kHz ; l'intervalle de 120 s laisse moins de 1 % d'occupation
// pour les paquets JSON bornes a 159 octets (SF7, CR4/5, CRC, preambule 8).
#ifndef WEATHER_FREQUENCY_HZ
#define WEATHER_FREQUENCY_HZ 433920000L
#endif
namespace WeatherRadio {
constexpr long FREQUENCY_HZ = WEATHER_FREQUENCY_HZ;
constexpr long BANDWIDTH_HZ = 31250;
constexpr uint32_t SEND_INTERVAL_MS = 120000;
// Consigne conduite nominale ; la puissance rayonnee depend aussi de l'antenne.
constexpr int TX_POWER_DBM = 8;
static_assert(TX_POWER_DBM >= 2 && TX_POWER_DBM <= 10,
              "Plage de puissance nominale retenue pour ce montage");
constexpr uint8_t TX_PA_CONFIG = 0x80 | (TX_POWER_DBM - 2);

constexpr bool txPowerRegistersValid(uint8_t paConfig, uint8_t paDac) {
  return paConfig == TX_PA_CONFIG && paDac == 0x84;
}

constexpr uint32_t frequencyWord(long frequencyHz) {
  return uint32_t((uint64_t(frequencyHz) << 19) / 32000000ULL);
}

constexpr bool modemConfigurationValid(uint8_t version, uint8_t modem1,
                                       uint8_t modem2, uint8_t syncWord,
                                       uint32_t frequency) {
  return version == 0x12 && modem1 == 0x42 && (modem2 & 0xFC) == 0x74 && syncWord == 0x12 &&
         frequency == frequencyWord(FREQUENCY_HZ);
}

// Valeurs attendues pour l'emetteur au repos (sommeil ou standby LoRa LF).
constexpr bool txConfigurationValid(uint8_t version, uint8_t mode,
                                    uint8_t modem1, uint8_t modem2,
                                    uint8_t syncWord, uint32_t frequency) {
  return (mode == 0x88 || mode == 0x89) &&
         modemConfigurationValid(version, modem1, modem2, syncWord, frequency);
}

constexpr bool rxConfigurationValid(uint8_t version, uint8_t mode,
                                    uint8_t modem1, uint8_t modem2,
                                    uint8_t syncWord, uint32_t frequency) {
  return (mode == 0x89 || mode == 0x8D || mode == 0x8E) &&
         modemConfigurationValid(version, modem1, modem2, syncWord, frequency);
}
}
