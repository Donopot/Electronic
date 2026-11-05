#include "../firmware/EnviroTerminalDiagnostic/Bmp180Compensation.h"

// Vecteur de reference Bosch (OSS=0) : UT=27898, UP=23843.
constexpr bmp180::Calibration reference = {
  408, -72, -14383, 32741, 32757, 23153, 6190, 4, -32768, -8711, 2868
};
constexpr auto result = bmp180::compensate(reference, 27898, 23843, 0);
static_assert(result.valid, "Le vecteur Bosch doit etre valide");
static_assert(result.temperatureDeciC == 150, "Temperature Bosch : 15,0 C");
static_assert(result.pressurePa == 69964, "Pression Bosch : 699,64 hPa");

constexpr bmp180::Calibration zero{};
static_assert(!bmp180::compensate(zero, 27898, 23843, 0).valid,
              "Rejeter la calibration nulle");
constexpr bmp180::Calibration zeroDenominator = {
  408, -72, -14383, 32741, 32757, 27898, 6190, 4, -32768, -8711, 0
};
static_assert(!bmp180::compensate(zeroDenominator, 27898, 23843, 0).valid,
              "Rejeter la division par zero");
static_assert(!bmp180::compensate(reference, 27898, 0, 0).valid,
              "Rejeter une pression brute incoherente");
static_assert(!bmp180::compensate(reference, 27898, 23843, 4).valid,
              "Rejeter OSS invalide");
static_assert(!bmp180::compensate(reference, 27898, 0xFFFFFF, 3).valid,
              "Rejeter les donnees brutes hors plage");
