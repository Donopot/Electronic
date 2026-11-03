#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <PlantSoilProtocol.h>
constexpr int RADIOLIB_ERR_NONE = 0, RADIOLIB_NC = -1, RADIOLIB_SX126X_SYNC_WORD_PRIVATE = 0x12;
namespace host {
struct Transmission { uint32_t time; plantsoil::Packet packet; int16_t result; };
struct Radio { int16_t beginCode = 0, transmitCode = 0; unsigned begins = 0; std::vector<Transmission> transmissions; };
inline Radio& radio() { static Radio value; return value; }
}
class Module {
public:
  Module(int nss, int dio1, int reset, int busy, FakeSpi&, SPISettings) { assert(nss == 4 && dio1 == 1 && reset == 2 && busy == 3); }
};
class SX1262 {
public:
  explicit SX1262(Module*) {}
  void setRfSwitchPins(int rx, int tx) { assert(rx == 5 && tx == RADIOLIB_NC); }
  int16_t begin(float, float, int, int, int, int power, int, float, bool) { assert(power == 0); ++host::radio().begins; return host::radio().beginCode; }
  int16_t setDio2AsRfSwitch(bool value) { assert(value); return 0; }
  int16_t setCRC(int value) { assert(value == 2); return 0; }
  uint32_t getTimeOnAir(size_t size) { assert(size == 20); return 56576; }
  int16_t transmit(const uint8_t* data, size_t size) {
    plantsoil::Packet packet;
    assert(plantsoil::decode(data, size, packet) == plantsoil::Result::Accepted);
    host::radio().transmissions.push_back({millis(), packet, host::radio().transmitCode});
    return host::radio().transmitCode;
  }
};
