#pragma once
#include "config.h"
#include "sensors/VEML7700Sensor.h"
#if PLANT_SOIL_LORA
#include <PlantSoilProtocol.h>
#endif
#if PLANT_OLED
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#endif

namespace plant {
class OledDisplay {
public:
#if PLANT_OLED
    bool begin();
    void show(const Measurement& value, uint64_t uptime);
#if PLANT_SOIL_LORA
    void showSoil(const Measurement& lux, const plantsoil::Receiver& soil, bool radioReady, uint32_t now);
#endif
    void showSleepPrompt();
    void sleep();
private:
    // Controleur 1 independant du VEML7700 sur Wire / controleur 0.
    TwoWire wire_{1};
    Adafruit_SSD1306 display_{128, 64, &wire_, -1, 100000, 100000};
    bool ready_ = false;
    uint32_t lastAttempt_ = 0;
#else
    bool begin() { return false; }
    void show(const Measurement&, uint64_t) {}
#if PLANT_SOIL_LORA
    void showSoil(const Measurement&, const plantsoil::Receiver&, bool, uint32_t) {}
#endif
    void showSleepPrompt() {}
    void sleep() {}
#endif
};
}
