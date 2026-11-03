#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <RadioLib.h>
#include <PlantSoilProtocol.h>
#include <esp_sleep.h>
#include <freertos/task.h>
#include "radio/SoilRadio.h"
#include <assert.h>
#include <algorithm>
#include <string>
namespace host { uint32_t now = 0; }
FakeSerial Serial;
FakeEsp ESP;
TwoWire Wire;
FakeSpi SPI;
void setup();
void loop();
static bool hasLine(const std::string& text) {
    for (const auto& line : Serial.lines) if (line.find(text) != std::string::npos) return true;
    return false;
}
static void frame(const char* expected, const char* absent = "unused") {
    assert(host::oledFrames().back().find(expected) != std::string::npos);
    assert(host::oledFrames().back().find(absent) == std::string::npos);
}
int main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "normal";
    if (scenario == "radio_failed") host::radio().beginCode = -2;
    if (scenario == "sleep_failed") host::radio().sleepCode = -2;
    setup();
    if (scenario == "pending_init") {
        const auto initialClock = host::now;
        for (int i = 0; i < 113; ++i) loop();
        assert(host::now == initialClock && host::radio().begins == 0);
        host::now = 1000; loop(); frame("Lux 537.6");
        host::now = 11000; loop(); frame("Lux 537.6");
        assert(host::radio().begins == 0); // aucune attente de begin dans setup/loop
        host::buttonPressed() = true;
        host::now = 11100; loop(); host::now = 11140; loop();
        host::now = 14140; loop(); frame("Relacher PRG");
        host::buttonPressed() = false;
        host::now = 14200; loop(); host::now = 14240; loop();
        assert(host::radio().begins == 0 && !Serial.flushed);
        assert(std::find(host::gpioCalls().begin(), host::gpioCalls().end(), "spi_end") == host::gpioCalls().end());
        host::radio().beginCode = -2;
        host::finishInitialization();
        try { loop(); assert(false); } catch (const SleepEntered&) {}
        assert(Serial.flushed && host::radio().begins == 1);
        puts("PASS initialisation differee : lux/PRG apres 14s, boucle sans attente, sommeil attend le proprietaire SPI");
        return 0;
    }
    host::finishInitialization();
    for (int i = 0; i < 113; ++i) loop();
    host::now = 1000; loop();
    frame("Lux 537.6"); frame("ATTENTE SONDE");
    if (scenario == "radio_failed") {
        frame("ERREUR RADIO");
        assert(host::radio().begins == 1);
        host::now = 9999; loop(); assert(host::radio().begins == 1);
        host::radio().beginCode = 0;
        host::now = 10000; loop(); assert(host::radio().begins == 1);
        host::finishInitialization();
        loop(); assert(host::radio().begins == 2);
        host::now = 11000; loop(); frame("RADIO ECOUTE"); frame("Lux 537.6");
    }
    plantsoil::Packet packet;
    packet.session = 1234; packet.sequence = 1; packet.address = 0x48;
    packet.status = plantsoil::AdcStatus::Ok; packet.raw = 21565;
    uint8_t bytes[plantsoil::kPacketSize];
    assert(plantsoil::encode(packet, bytes, sizeof(bytes)) == plantsoil::Result::Accepted);
    host::now = 12000; host::inject(bytes, sizeof(bytes)); loop();
    assert(hasLine("[SOIL_RX] authenticated=no node=SOIL-01 session=1234 seq=1"));
    frame("Brut 21565"); frame("2.695625 V"); frame("Lux 537.6");
    host::now = 60000; host::inject(bytes, sizeof(bytes)); loop();
    assert(hasLine("reason=duplicate")); frame("Age 48 s");
    bytes[2] = 2;
    host::now = 90000; host::inject(bytes, sizeof(bytes)); loop();
    assert(hasLine("reason=header")); frame("Age 78 s");
    host::now = 101999; loop(); frame("2.695625 V");
    host::now = 102000; loop(); frame("MESURE PERIMEE", "2.695625 V"); frame("Lux 537.6");
    packet.session = 5678; packet.sequence = 0; packet.status = plantsoil::AdcStatus::ReadError; packet.raw = 0;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = 104000; host::inject(bytes, sizeof(bytes)); loop();
    frame("DEFAUT ADS", "21565"); frame("Valeur indisponible");
    packet.sequence++; packet.status = plantsoil::AdcStatus::Ok; packet.raw = 8000;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = 105000; host::inject(bytes, sizeof(bytes)); loop(); frame("1.000000 V");
    host::radio().readCode = RADIOLIB_ERR_CRC_MISMATCH;
    packet.sequence++; packet.raw = 16000;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = 106000; host::inject(bytes, sizeof(bytes)); loop(); frame("Age 1 s");
    frame("1.000000 V", "2.000000 V");
    host::radio().readCode = 0;
    host::radio().hasCrc = false;
    host::now = 106100; host::inject(bytes, sizeof(bytes)); loop();
    frame("1.000000 V", "2.000000 V");
    assert(hasLine("reason=header_crc"));
    host::now = 106999; loop(); frame("Age 1 s");
    host::radio().hasCrc = true;
    host::radio().standbyCode = -8;
    const auto receives = host::radio().receives;
    host::inject(bytes, sizeof(bytes)); loop();
    assert(host::radio().receives == receives + 1 && hasLine("reason=standby"));
    host::radio().standbyCode = 0;
    host::radio().receiveCode = -3;
    host::now = 107000; host::inject(bytes, sizeof(bytes)); loop(); frame("ERREUR RADIO"); frame("Lux 537.6");
    host::buttonPressed() = true;
    host::now = 107100; loop(); host::now = 107140; loop();
    host::now = 110140; loop(); frame("Relacher PRG");
    host::buttonPressed() = false;
    host::now = 110200; loop(); host::now = 110240;
    try { loop(); assert(false); } catch (const SleepEntered&) {}
    const auto& calls = host::gpioCalls();
    const auto radioSleep = std::find(calls.begin(), calls.end(), "radio_sleep");
    const auto spiEnd = std::find(calls.begin(), calls.end(), "spi_end");
    assert(radioSleep < spiEnd && spiEnd < std::find(calls.begin(), calls.end(), "deep_sleep"));
    assert(std::find(calls.begin(), calls.end(), scenario == "sleep_failed" ? "write12:0" : "write12:1") != calls.end());
    assert(Serial.flushed);
    puts("PASS vrais radio/main/OLED : RX, age inchange au rejet, peremption, defaut sans valeur, lux concurrents, reprise, PRG et sommeil/reset de secours");
}
