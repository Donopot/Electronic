#pragma once
#include <stdint.h>

namespace plant {
enum class Status { Ok, WarmingUp, Nack, ShortRead, BusError, ConfigLost, Saturated };
const char* statusName(Status status);
struct Measurement {
    Status status = Status::WarmingUp;
    uint16_t raw = 0;
    double lux = 0;
};

class SensorTransport {
public:
    virtual ~SensorTransport() = default;
    virtual Status restart() = 0;
    virtual Status configure() = 0;
    virtual Status verify() = 0;
    virtual Status ambient(uint16_t& raw) = 0;
};

class VEML7700Sensor {
public:
    explicit VEML7700Sensor(SensorTransport& transport, uint32_t (*clock)() = nullptr)
        : transport_(transport), clock_(clock) {}
    Status begin(uint32_t now);
    void service(uint32_t now);
    Measurement measure(uint32_t now);
    Status status() const { return status_; }
    static double toLux(uint16_t raw);
private:
    Status fail(Status reason, uint32_t now);
    SensorTransport& transport_;
    uint32_t (*clock_)();
    Status status_ = Status::WarmingUp;
    uint32_t changed_ = 0;
    bool configured_ = false;
};
}
