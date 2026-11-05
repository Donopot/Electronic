#pragma once
#include "config.h"
#include "sensors/VEML7700Sensor.h"
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
    void showSleepPrompt() {}
    void sleep() {}
#endif
};
}
