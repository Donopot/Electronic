#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <assert.h>
#include <string.h>

namespace host {
uint32_t now = 0;
}
FakeSerial Serial;
FakeEsp ESP;
TwoWire Wire;
void setup();
void loop();

static void checkLast(const char* event, const char* status) {
    const std::string& line = Serial.lines.back();
    assert(line.find(std::string("\"event\":\"") + event + "\"") != std::string::npos);
    assert(line.find(std::string("\"status\":\"") + status + "\"") != std::string::npos);
}
int main(int argc, char** argv) {
    const uint32_t base = argc > 1 && strcmp(argv[1], "wrap") == 0 ? 0xFFFFFE0BU : 0;
    const bool screenPresent = plant::kOled && !(argc > 1 && strcmp(argv[1], "no_oled") == 0);
    const size_t oledOffset = plant::kOled ? 1 : 0;
    FakeWire::oledPresent() = screenPresent;
    host::now = base;
    setup();
    assert(Serial.lines.size() == 1 + oledOffset);
    assert(Serial.lines[0].find("\"event\":\"demarrage\"") != std::string::npos);
    if (plant::kOled) checkLast("oled", screenPresent ? "ok" : "erreur_bus");
    else {
        checkLast("demarrage", "conversion_en_cours");
        for (const auto& call : host::gpioCalls()) assert(call.find("mode36:") == std::string::npos && call.find("mode21:") == std::string::npos);
    }
    assert(host::oledFrames().size() == (screenPresent ? 1U : 0U));
    for (int i = 0; i < 112; ++i) loop();
    assert(Serial.lines.size() == 2 + oledOffset);
    checkLast("scan_ack", "ok");
    assert(Serial.lines.back().find("\"raw\":null,\"lux\":null") != std::string::npos);
    loop();
    assert(Serial.lines.size() == 4 + oledOffset && Wire.starts == 2);
    checkLast("configuration", "conversion_en_cours");
    assert(Serial.lines[2 + oledOffset].find("\"event\":\"scan_fin\"") != std::string::npos);

    host::now = base + 999;
    loop();
    assert(Serial.lines.size() == 4 + oledOffset);
    host::now = base + 1000;
    loop();
    assert(Serial.lines.size() == 5 + oledOffset);
    checkLast("mesure", "ok");
    if (screenPresent) assert(host::oledFrames().back().find("537.6") != std::string::npos);
    for (int i = 0; i < 10; ++i) loop();
    assert(Serial.lines.size() == 5 + oledOffset);

    host::now = base + 5500;
    loop();
    assert(Serial.lines.size() == 6 + oledOffset);
    for (int i = 0; i < 10; ++i) loop();
    assert(Serial.lines.size() == 6 + oledOffset); // aucune rafale de rattrapage
    host::now = base + 6499;
    loop();
    assert(Serial.lines.size() == 6 + oledOffset);
    host::now = base + 6500;
    loop();
    assert(Serial.lines.size() == 7 + oledOffset);

    Wire.nack = 2;
    host::now = base + 7500;
    loop();
    assert(Serial.lines.size() == 8 + oledOffset);
    checkLast("mesure", "i2c_nack");
    if (screenPresent) {
        assert(host::oledFrames().back().find("CAPTEUR ABSENT") != std::string::npos);
        assert(host::oledFrames().back().find("537.6") == std::string::npos);
    }
    host::now = base + 8500;
    loop();
    host::now = base + 9500;
    loop();
    assert(Serial.lines.size() == 10 + oledOffset && Wire.starts == 2);
    Wire.nack = 0;
    host::now = base + 10499;
    loop();
    assert(Serial.lines.size() == 10 + oledOffset && Wire.starts == 2);
    host::now = base + 10500;
    loop();
    assert(Serial.lines.size() == 11 + oledOffset && Wire.starts == 3);
    checkLast("mesure", "conversion_en_cours");
    host::now = base + 10649;
    loop();
    host::now = base + 10650;
    loop();
    assert(Serial.lines.size() == 11 + oledOffset);
    host::now = base + 11500;
    loop();
    assert(Serial.lines.size() == 12 + oledOffset);
    checkLast("mesure", "ok");
    if (screenPresent) assert(host::oledFrames().back().find("537.6") != std::string::npos);
    else assert(host::oledFrames().empty());
    for (const auto& line : Serial.lines) puts(line.c_str());
    puts("PASS vrai setup/loop : scan, echeances, boucles repetees, pause, wrap et reprise sans ancienne mesure");
}
