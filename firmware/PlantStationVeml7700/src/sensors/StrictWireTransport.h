#pragma once
#include "VEML7700Sensor.h"
#include "config.h"
#include <sfTk/sfDevVEML7700.h>

namespace plant {
// Meme implementation compilee sur cible et avec le faux Wire des tests hote.
template<class WirePort>
class StrictWireTransport : public SensorTransport, public sfTkIBus {
public:
    explicit StrictWireTransport(WirePort& wire) : wire_(wire) {}
    Status restart() override {
        wire_.end();
        if (!wire_.begin(kSda, kScl, kI2cHz)) return Status::BusError;
        wire_.setTimeOut(kTimeoutMs);
        return Status::Ok;
    }
    Status configure() override {
        last_ = Status::Ok;
        if (device_.begin(this) != ksfTkErrOk ||
            device_.setSensitivityMode(VEML7700_SENSITIVITY_x1_8) != ksfTkErrOk ||
            device_.setIntegrationTime(VEML7700_INTEGRATION_100ms) != ksfTkErrOk ||
            device_.enableInterrupt(false) != ksfTkErrOk ||
            device_.powerOn() != ksfTkErrOk) return last_;
        // Le pilote SparkFun n'expose pas le registre d'economie d'energie.
        const uint8_t reg = 3;
        const uint8_t off[2] = {0, 0};
        uint8_t address = reg;
        if (writeRegister(&address, 1, off, 2) != ksfTkErrOk) return last_;
        return Status::Ok;
    }
    Status verify() override {
        uint8_t reg = 0;
        uint8_t data[2] = {};
        size_t count = 0;
        if (readRegister(&reg, 1, data, 2, count, 0) != ksfTkErrOk) return last_;
        const uint16_t config = static_cast<uint16_t>(data[0] | (data[1] << 8));
        if (config != 0x1000) return Status::ConfigLost;
        reg = 3;
        if (readRegister(&reg, 1, data, 2, count, 0) != ksfTkErrOk) return last_;
        return (data[0] == 0 && data[1] == 0) ? Status::Ok : Status::ConfigLost;
    }
    Status ambient(uint16_t& raw) override {
        raw = 0;
        return device_.getAmbientLight(raw) == ksfTkErrOk ? Status::Ok : last_;
    }
    Status probe(uint8_t address) {
        wire_.beginTransmission(address);
        return transmission(wire_.endTransmission(true));
    }
    Status shutdown() {
        // ALS_SD=1 ; gain1/8 et integration100ms conserves.
        uint8_t reg = 0;
        const uint8_t asleep[2] = {0x01, 0x10};
        if (writeRegister(&reg, 1, asleep, 2) != ksfTkErrOk) return last_;
        uint8_t actual[2] = {};
        size_t count = 0;
        if (readRegister(&reg, 1, actual, 2, count, 0) != ksfTkErrOk) return last_;
        return (actual[0] & 1) ? Status::Ok : Status::ConfigLost;
    }
    sfTkError_t writeRegister(uint8_t* reg, size_t regLength,
                             const uint8_t* data, size_t length) override {
        wire_.beginTransmission(kAddress);
        const bool complete = wire_.write(reg, regLength) == regLength &&
                              wire_.write(data, length) == length;
        const Status rc = transmission(wire_.endTransmission(true));
        last_ = complete ? rc : Status::BusError;
        return last_ == Status::Ok ? ksfTkErrOk : ksfTkErrFail;
    }
    sfTkError_t readRegister(uint8_t* reg, size_t regLength, uint8_t* data,
                            size_t length, size_t& count, uint32_t readDelay = 0) override {
        count = 0;
        if (readDelay || regLength != 1 || length != 2) {
            last_ = Status::BusError; return ksfTkErrFail;
        }
        wire_.beginTransmission(kAddress);
        const bool complete = wire_.write(reg, regLength) == regLength;
        last_ = transmission(wire_.endTransmission(false));
        if (!complete) last_ = Status::BusError;
        if (last_ != Status::Ok) return ksfTkErrFail;
        // Une seule requete : ne jamais assembler deux conversions apres lecture courte.
        const size_t received = wire_.requestFrom(kAddress, static_cast<size_t>(2), true);
        if (received != 2 || wire_.available() != 2) {
            // Drain borne, meme si une implementation Wire signale available() errone.
            for (size_t i = 0; i < 2 && wire_.available(); ++i) wire_.read();
            last_ = received == 0 ? Status::BusError : Status::ShortRead;
            return ksfTkErrBusUnderRead;
        }
        const int low = wire_.read();
        const int high = wire_.read();
        if (low < 0 || high < 0) { last_ = Status::ShortRead; return ksfTkErrBusUnderRead; }
        data[0] = static_cast<uint8_t>(low);
        data[1] = static_cast<uint8_t>(high);
        count = 2;
        last_ = Status::Ok;
        return ksfTkErrOk;
    }
private:
    static Status transmission(uint8_t rc) {
        if (rc == 0) return Status::Ok;
        return (rc == 2 || rc == 3) ? Status::Nack : Status::BusError;
    }
    WirePort& wire_;
    sfDevVEML7700 device_;
    Status last_ = Status::Ok;
};
}
