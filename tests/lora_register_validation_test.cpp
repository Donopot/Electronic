#include "../firmware/WeatherRadioConfig.h"

using WeatherRadio::txConfigurationValid;
constexpr uint32_t frequency = WeatherRadio::frequencyWord(WeatherRadio::FREQUENCY_HZ);

static_assert(WeatherRadio::txPowerRegistersValid(0x86, 0x84),
              "PA_BOOST a 8 dBm, mode PA_DAC normal");
static_assert(!WeatherRadio::txPowerRegistersValid(0x80, 0x84),
              "Detecter un retour a l'ancienne puissance 2 dBm");
static_assert(!WeatherRadio::txPowerRegistersValid(0x86, 0x87),
              "Rejeter le mode haute puissance 20 dBm non demande");

static_assert(WeatherRadio::frequencyWord(433920000L) == 0x6C7AE1,
              "Vecteur de synthese 433,92 MHz a quartz 32 MHz");
static_assert(txConfigurationValid(0x12, 0x88, 0x42, 0x74, 0x12, frequency),
              "Sommeil LoRa LF valide");
static_assert(txConfigurationValid(0x12, 0x89, 0x42, 0x74, 0x12, frequency),
              "Standby LoRa LF valide");
static_assert(!txConfigurationValid(0x12, 0x08, 0x42, 0x74, 0x12, frequency),
              "Rejeter le retour FSK meme si RegVersion reste correct");
static_assert(!txConfigurationValid(0xFF, 0x88, 0x42, 0x74, 0x12, frequency),
              "Rejeter la lecture SPI haute observee sur le montage");
static_assert(!txConfigurationValid(0x00, 0x88, 0x42, 0x74, 0x12, frequency),
              "Rejeter la lecture SPI nulle observee sur le montage");
static_assert(!txConfigurationValid(0x12, 0x88, 0x42, 0x74, 0x12, 0x6C8000),
              "Rejeter la frequence 434 MHz de retour apres reset");
static_assert(!txConfigurationValid(0x12, 0x88, 0x42, 0x70, 0x12, frequency),
              "Rejeter la perte du CRC");
static_assert(!txConfigurationValid(0x12, 0x88, 0x42, 0x7C, 0x12, frequency),
              "Rejeter le mode TX continu non demande");
static_assert(!txConfigurationValid(0x12, 0x88, 0x42, 0x74, 0x34, frequency),
              "Rejeter un autre sync word");
static_assert(WeatherRadio::rxConfigurationValid(0x12, 0x8E, 0x42, 0x74, 0x12, frequency),
              "Reception LoRa LF valide");
static_assert(WeatherRadio::rxConfigurationValid(0x12, 0x89, 0x42, 0x74, 0x12, frequency),
              "Standby apres timeout RX valide");
static_assert(!WeatherRadio::rxConfigurationValid(0x12, 0x09, 0x42, 0x74, 0x12, frequency),
              "Version correcte ne suffit pas apres reset du modem");
static_assert(!WeatherRadio::rxConfigurationValid(0x12, 0x8E, 0x42, 0x74, 0x12, 0x6C8000),
              "Version correcte ne suffit pas si la frequence a change");
static_assert(!txConfigurationValid(0x12, 0x88, 0x42, 0x74, 0x12, 0x2A5C00),
              "Rejeter l'ancien canal 169 MHz sur la sonde");
static_assert(!WeatherRadio::rxConfigurationValid(0x12, 0x8E, 0x42, 0x74, 0x12, 0x2A5C00),
              "Rejeter l'ancien canal 169 MHz sur le recepteur");
