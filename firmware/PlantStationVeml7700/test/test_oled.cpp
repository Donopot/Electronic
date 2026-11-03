#include "display/OledDisplay.h"
#include <Arduino.h>
#include <assert.h>
#include <string>
namespace host { uint32_t now = 0; }
FakeSerial Serial;
FakeEsp ESP;
TwoWire Wire;

int main() {
    plant::OledDisplay screen;
    assert(screen.begin());
    const std::vector<std::string> expected{
        "mode36:1", "write36:0", "delay100", "mode21:1",
        "write21:0", "delay10", "write21:1", "delay20"};
    assert(host::gpioCalls() == expected);
    plant::Measurement value;
    value.status = plant::Status::Ok;
    value.lux = 537.6;
    host::now = 1000;
    screen.show(value, 4294968000ULL);
    assert(host::oledFrames().back().find("537.6") != std::string::npos);
    assert(host::oledFrames().back().find("Depuis 4294968 s") != std::string::npos);
    const auto frames = host::oledFrames().size();
    FakeWire::oledPresent() = false;
    host::now = 2000;
    screen.show(value, 2000);
    assert(host::oledFrames().size() == frames);
    FakeWire::oledPresent() = true;
    value.status = plant::Status::Nack;
    host::now = 11999;
    screen.show(value, 11999);
    assert(host::oledFrames().size() == frames);
    host::now = 12000;
    screen.show(value, 12000);
    assert(host::oledFrames().size() == frames + 2); // attente puis nouvel etat
    assert(host::oledFrames().back().find("CAPTEUR ABSENT") != std::string::npos);
    assert(host::oledFrames().back().find("537.6") == std::string::npos);
    value.status = plant::Status::Ok;
    value.lux = 0;
    screen.show(value, 13000);
    assert(host::oledFrames().back().find("0.0") != std::string::npos);
    puts("PASS OLED reel : alimentation/reset, lux, uptime 64 bits, panne/reprise bornee et absence de valeur perimee");
}
