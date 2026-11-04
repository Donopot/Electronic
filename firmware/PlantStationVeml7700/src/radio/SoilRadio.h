#pragma once
#include "config.h"
#if PLANT_SOIL_LORA
#include <PlantSoilProtocol.h>
#include <RadioLib.h>
#include <SPI.h>
#include <atomic>
namespace plant {
class SoilRadio {
public:
    void begin();
    void service(uint32_t now);
    bool sleep();
    bool initializing() const { return initialization_.load(std::memory_order_acquire) != Initialization::Idle; }
    bool ready() const { return ready_; }
    const plantsoil::ReceiverBank& readings() const { return readings_; }
private:
    enum class Initialization { Idle, Running, Finished };
    void initialize();
    static void initializeTask(void* context);
    void fail(const char* operation, int16_t code);
    SX1262 radio_{new Module(8, 14, 12, 13, SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0))};
    plantsoil::ReceiverBank readings_;
    bool ready_ = false;
    uint32_t lastAttempt_ = 0;
    std::atomic<Initialization> initialization_{Initialization::Idle};
    int16_t initializationCode_ = 0;
};
}
#endif
