#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <assert.h>
#include <string.h>
#define IRAM_ATTR
constexpr int RADIOLIB_ERR_NONE = 0, RADIOLIB_ERR_PACKET_TOO_LONG = -4;
constexpr int RADIOLIB_ERR_CRC_MISMATCH = -7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE = 0x12;
namespace host {
struct Radio {
    int16_t beginCode = 0, readCode = 0, receiveCode = 0, sleepCode = 0, standbyCode = 0, headerCode = 0;
    bool frozen = false, hasCrc = true;
    unsigned begins = 0, receives = 0;
    void (*callback)() = nullptr;
    std::vector<uint8_t> packet;
};
inline Radio& radio() { static Radio state; return state; }
inline void inject(const uint8_t* data, size_t len) {
    radio().packet.assign(data, data + len);
    assert(radio().callback); radio().callback();
}
}
class Module {
public:
    Module(int nss, int dio1, int reset, int busy, FakeSpi&, SPISettings) {
        assert(nss == 8 && dio1 == 14 && reset == 12 && busy == 13);
    }
};
class SX1262 {
public:
    explicit SX1262(Module*) {}
    int16_t begin(float freq, float bandwidth, int sf, int cr, int sync, int power, int preamble, float tcxo, bool regulator) {
        assert(freq == 868.1f && bandwidth == 125.0f && sf == 7 && cr == 5 && sync == 0x12);
        assert(power == 0 && preamble == 8 && tcxo == 1.8f && !regulator);
        ++host::radio().begins;
        return host::radio().beginCode;
    }
    int16_t setDio2AsRfSwitch(bool value) { assert(value); return 0; }
    int16_t setCRC(int count) { assert(count == 2); return 0; }
    void setDio1Action(void (*callback)()) { host::radio().callback = callback; }
    void clearDio1Action() { host::radio().callback = nullptr; }
    int16_t startReceive() { ++host::radio().receives; host::radio().frozen = false; return host::radio().receiveCode; }
    int16_t standby() { host::radio().frozen = host::radio().standbyCode == 0; return host::radio().standbyCode; }
    size_t getPacketLength() { assert(host::radio().frozen); return host::radio().packet.size(); }
    int16_t getLoRaRxHeaderInfo(uint8_t*, bool* crc) { assert(host::radio().frozen); *crc = host::radio().hasCrc; return host::radio().headerCode; }
    int16_t readData(uint8_t* data, size_t length) { assert(host::radio().frozen); memcpy(data, host::radio().packet.data(), length); return host::radio().readCode; }
    int16_t sleep(bool retain) { assert(!retain); host::gpioCalls().push_back("radio_sleep"); return host::radio().sleepCode; }
};
