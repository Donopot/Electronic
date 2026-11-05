#pragma once
#include <stddef.h>
#include <stdint.h>
#include "sensors/VEML7700Sensor.h"

namespace plant {
class Diagnostics {
public:
    // Retourne false plutot que publier une ligne tronquee.
    static bool format(char* output, size_t capacity, bool json, const char* event,
                       uint64_t uptime, const Measurement& value,
                       const char* message = "", int address = -1);
};

struct ScanStep {
    uint8_t address;
    Status status;
    bool done;
};
// Adresses non reservees 0x08..0x77 ; une transaction par appel.
template<class Transport>
class I2cScanner {
public:
    explicit I2cScanner(bool enabled) : next_(enabled ? 8 : 120) {}
    bool done() const { return next_ >= 120; }
    ScanStep step(Transport& transport) {
        if (done()) return {0, Status::Ok, true};
        const uint8_t address = next_++;
        const Status rc = transport.probe(address);
        if (rc == Status::BusError) next_ = 120;
        return {address, rc, done()};
    }
private:
    uint8_t next_;
};
const char* diagnosticMessage(Status status);
}
