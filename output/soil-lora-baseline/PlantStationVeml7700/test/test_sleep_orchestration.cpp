#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <esp_sleep.h>
#include <assert.h>
#include <string.h>
#include <algorithm>
namespace host { uint32_t now = 0; }
FakeSerial Serial;
FakeEsp ESP;
TwoWire Wire;
void setup();
void loop();
int main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "ok";
    if (scenario == "absent_boot") Wire.nack = 2;
    if (scenario == "bus_start_failed") Wire.beginOk = false;
    setup();
    for (int i = 0; i < 113; ++i) loop();
    host::now = 40; loop();
    host::buttonPressed() = true;
    host::now = 100; loop();
    host::now = 140; loop();
    host::now = 3139; loop();
    host::now = 3140; loop();
    assert(Serial.lines.back().find("veille_demande") != std::string::npos);
    assert(host::oledFrames().back().find("Relacher PRG") != std::string::npos);
    const auto messages = Serial.lines.size();
    host::now = 4000; loop();
    assert(Serial.lines.size() == messages); // aucune mesure n'ecrase la demande
    host::buttonPressed() = false;
    host::now = 4100; loop();
    if (scenario == "sensor_absent") Wire.nack = 2;
    if (scenario == "readback_clear") Wire.ignoreShutdownWrite = true;
    if (scenario == "short_read") Wire.shortReg = 0;
    if (scenario == "request_failed") Wire.busReg = 0;
    host::now = 4140;
    try { loop(); assert(false); }
    catch (const SleepEntered&) {
        assert(Serial.flushed && Serial.lines.back().find("Entree en veille profonde") != std::string::npos);
        const auto& report = Serial.lines[Serial.lines.size()-2];
        if (scenario == "ok") assert((Wire.regs[0] & 1) == 1);
        else if (scenario == "sensor_absent" || scenario == "absent_boot") assert(report.find("i2c_nack") != std::string::npos);
        else if (scenario == "readback_clear") assert(report.find("configuration_perdue") != std::string::npos);
        else if (scenario == "short_read") assert(report.find("lecture_courte") != std::string::npos);
        else assert(report.find("erreur_bus") != std::string::npos);
        const auto& calls = host::gpioCalls();
        assert(std::find(calls.begin(), calls.end(), "oled_off") != calls.end());
        assert(std::find(calls.begin(), calls.end(), "hold36") != calls.end());
        assert(calls.back() == "deep_sleep");
    }
    puts("PASS vrai main : demande PRG, pause des mesures, shutdown capteur/OLED, sommeil meme sans capteur");
}
