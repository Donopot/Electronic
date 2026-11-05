#pragma once
#include <stdint.h>

#ifndef PLANT_JSON
#define PLANT_JSON 0
#endif
#ifndef PLANT_SOIL_LORA
#define PLANT_SOIL_LORA 0
#endif
#ifndef PLANT_DASHBOARD
#define PLANT_DASHBOARD 0
#endif
#ifndef PLANT_LUX
#define PLANT_LUX 1
#endif
#ifndef PLANT_OLED
#define PLANT_OLED 1
#endif
#ifndef PLANT_SCAN
#define PLANT_SCAN 1
#endif
#ifndef PLANT_INTERVAL_MS
#define PLANT_INTERVAL_MS 1000
#endif
#ifndef PLANT_RETRY_MS
#define PLANT_RETRY_MS 3000
#endif
#ifndef PLANT_TIMEOUT_MS
#define PLANT_TIMEOUT_MS 25
#endif

namespace plant {
constexpr char kVersion[] = "1.2.0";
constexpr bool kOled = PLANT_OLED;
constexpr int kSda = 4;
constexpr int kScl = 5;
constexpr uint8_t kAddress = 0x10;
constexpr uint32_t kI2cHz = 100000;
constexpr uint32_t kIntervalMs = PLANT_INTERVAL_MS;
constexpr uint32_t kRetryMs = PLANT_RETRY_MS;
constexpr uint16_t kTimeoutMs = PLANT_TIMEOUT_MS;
// 100 ms + tolerance 30 % + demarrage 2,5 ms, arrondis avec marge.
constexpr uint32_t kConversionMs = 150;
// Plage nominale Vishay : ne pas publier une extrapolation du polynome hors plage.
constexpr double kMaxLux = 140000.0;
constexpr bool kJson = PLANT_JSON;
constexpr bool kScan = PLANT_SCAN;
// Le concentrateur de sondes peut etre monte sans VEML7700 : aucun scan I2C,
// aucune lecture de lux, et l'API publie lux=null avec lux_available=false.
constexpr bool kLux = PLANT_LUX;
static_assert(PLANT_LUX == 0 || PLANT_LUX == 1, "PLANT_LUX doit valoir 0 ou 1");
static_assert(PLANT_JSON == 0 || PLANT_JSON == 1, "PLANT_JSON doit valoir 0 ou 1");
static_assert(PLANT_SOIL_LORA == 0 || PLANT_SOIL_LORA == 1, "PLANT_SOIL_LORA doit valoir 0 ou 1");
static_assert(PLANT_OLED == 0 || PLANT_OLED == 1, "PLANT_OLED doit valoir 0 ou 1");
static_assert(PLANT_SCAN == 0 || PLANT_SCAN == 1, "PLANT_SCAN doit valoir 0 ou 1");
static_assert(PLANT_INTERVAL_MS >= 150 && PLANT_INTERVAL_MS < 0x80000000ULL,
              "Cadence hors bornes");
static_assert(PLANT_RETRY_MS >= 150 && PLANT_RETRY_MS < 0x80000000ULL,
              "Reprise hors bornes");
static_assert(PLANT_TIMEOUT_MS >= 1 && PLANT_TIMEOUT_MS <= 100, "Timeout hors bornes");
static_assert(kSda != kScl && kAddress == 0x10, "Broches/adresse invalides");

inline bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
    return static_cast<uint32_t>(now - since) >= interval;
}
// Appeler au moins une fois par tour de millis() (~49 jours).
class Uptime {
public:
    uint64_t update(uint32_t now) {
        total_ += static_cast<uint32_t>(now - previous_);
        previous_ = now;
        return total_;
    }
private:
    uint32_t previous_ = 0;
    uint64_t total_ = 0;
};
}
