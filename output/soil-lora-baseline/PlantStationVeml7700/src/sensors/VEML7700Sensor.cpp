#include "VEML7700Sensor.h"
#include "config.h"
#include <math.h>

namespace plant {
const char* statusName(Status status) {
    switch (status) {
    case Status::Ok: return "ok";
    case Status::WarmingUp: return "conversion_en_cours";
    case Status::Nack: return "i2c_nack";
    case Status::ShortRead: return "lecture_courte";
    case Status::BusError: return "erreur_bus";
    case Status::ConfigLost: return "configuration_perdue";
    case Status::Saturated: return "saturation";
    }
    return "erreur_bus";
}
Status VEML7700Sensor::fail(Status reason, uint32_t now) {
    status_ = reason;
    configured_ = false;
    changed_ = clock_ ? clock_() : now;
    return reason;
}
Status VEML7700Sensor::begin(uint32_t now) {
    Status result = transport_.restart();
    if (result != Status::Ok) return fail(result, now);
    result = transport_.configure();
    if (result != Status::Ok) return fail(result, now);
    result = transport_.verify();
    if (result != Status::Ok) return fail(result, now);
    configured_ = true;
    changed_ = clock_ ? clock_() : now;
    return status_ = Status::WarmingUp;
}
void VEML7700Sensor::service(uint32_t now) {
    if (clock_) now = clock_();
    if (!configured_ && elapsed(now, changed_, kRetryMs)) { begin(now); return; }
    if (configured_ && status_ == Status::WarmingUp && elapsed(now, changed_, kConversionMs))
        status_ = Status::Ok;
}
double VEML7700Sensor::toLux(uint16_t raw) {
    const double linear = raw * 0.5376;
    if (linear <= 1000.0) return linear;
    return (((6.0135e-13 * linear - 9.3924e-9) * linear + 8.1488e-5) * linear + 1.0023) * linear;
}
Measurement VEML7700Sensor::measure(uint32_t now) {
    service(now);
    Measurement result;
    result.status = status_;
    if (!configured_ || status_ == Status::WarmingUp) return result;
    Status rc = transport_.verify();
    if (rc != Status::Ok) { result.status = fail(rc, now); return result; }
    uint16_t raw = 0;
    rc = transport_.ambient(raw);
    if (rc != Status::Ok) { result.status = fail(rc, now); return result; }
    // Verifier aussi apres lecture pour detecter une coupure/reinitialisation intercalee.
    rc = transport_.verify();
    if (rc != Status::Ok) { result.status = fail(rc, now); return result; }
    result.raw = raw;
    if (raw == 65535) { result.status = status_ = Status::Saturated; return result; }
    result.lux = toLux(raw);
    if (!isfinite(result.lux) || result.lux < 0) { result.status = fail(Status::BusError, now); return result; }
    if (result.lux > kMaxLux) {
        result.lux = 0;
        result.status = status_ = Status::Saturated;
        return result;
    }
    result.status = status_ = Status::Ok;
    return result;
}
}
