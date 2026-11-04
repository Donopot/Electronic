#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <RadioLib.h>
#include <PlantSoilProtocol.h>
#include <esp_sleep.h>
#include <freertos/task.h>
#include "radio/SoilRadio.h"
#include "display/OledDisplay.h"
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
static void frameNode(unsigned id, const char* expected, const char* absent = "unused") {
    char label[40]; snprintf(label, sizeof(label), "SOIL-%02u ", id);
    const auto& screen = host::oledFrames().back();
    const auto labelAt = screen.find(label); assert(labelAt != std::string::npos);
    const auto valueAt = labelAt + strlen(label);
    const auto row = screen.substr(valueAt, screen.find('\n', valueAt) - valueAt);
    assert(row.find(expected) != std::string::npos && row.find(absent) == std::string::npos);
    assert(row.size() <= 21); // police 6 pixels, ecran de 128 pixels
    const auto& draws = host::oledLayouts().back();
    assert(draws.size() == 8);
    const int rows[] = {0, 8, 16, 24, 32, 40, 48, 56};
    for (size_t i = 0; i < draws.size(); ++i) {
        assert(draws[i].x == 0 && draws[i].y == rows[i] && draws[i].size == 1);
        assert(draws[i].x + draws[i].text.size() * 6 <= 128 && draws[i].y + 8 <= 64);
    }
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
    frame("Lux 537.6"); frame("ATTENTE");
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
    frame("SONDES NON AUTH"); frame("SOIL-01"); frame("SOIL-02"); frame("SOIL-03"); frame("2.696 V"); frame("Lux 537.6");
    if (scenario == "layout") {
        plant::OledDisplay screen; assert(screen.begin());
        plantsoil::ReceiverBank bank;
        const int16_t raw[] = {-8, 26400, 20000, 21696};
        for (uint8_t id=1; id<=4; ++id) {
            packet.node=id; packet.raw=raw[id-1];
            assert(plantsoil::encode(packet, bytes, sizeof(bytes)) == plantsoil::Result::Accepted);
            assert(bank.accept(bytes, sizeof(bytes), 100) == plantsoil::Result::Accepted);
        }
        plant::Measurement lux; lux.status=plant::Status::WarmingUp;
        screen.showSoil(lux, bank, true, 100 + plantsoil::kStaleMs - 1);
        frameNode(1, "-0.001 V 89m"); frameNode(2, "3.300 V 89m"); frameNode(3, "2.500 V 89m"); frameNode(4, "2.712 V 89m");
        frame("Lux EN COURS");
        lux.status=plant::Status::BusError;
        screen.showSoil(lux, bank, false, 100 + plantsoil::kStaleMs - 1);
        frame("Lux ERREUR CAPTEUR"); frame("ERREUR RADIO"); frameNode(1, "-0.001 V 89m");
        puts("PASS disposition quatre sondes : valeurs extremes, age 89m, lux en cours/erreur et limites ecran");
        return 0;
    }
    if (scenario == "multi" || scenario == "three" || scenario == "four") {
        const auto injectNode = [&](uint8_t id, uint32_t session, uint32_t seq,
                                    plantsoil::AdcStatus status, int16_t raw, uint32_t now) {
            packet.node = id; packet.session = session; packet.sequence = seq;
            packet.status = status; packet.raw = raw;
            assert(plantsoil::encode(packet, bytes, sizeof(bytes)) == plantsoil::Result::Accepted);
            host::now = now; host::inject(bytes, sizeof(bytes)); loop();
        };
        injectNode(2, 1234, 1, plantsoil::AdcStatus::Ok, 8000, 13000);
        assert(hasLine("node=SOIL-02 session=1234 seq=1"));
        if (scenario == "three" || scenario == "four") {
            injectNode(3, 1234, 1, plantsoil::AdcStatus::Ok, 24000, 14000);
            frameNode(1, "2.696 V 0m"); frameNode(2, "1.000 V 0m"); frameNode(3, "3.000 V 0m");
        }
        if (scenario == "four") {
            injectNode(4, 1234, 1, plantsoil::AdcStatus::Ok, 20000, 15000);
            frameNode(1,"2.696 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"3.000 V 0m"); frameNode(4,"2.500 V 0m");
            injectNode(4,1234,1,plantsoil::AdcStatus::Ok,8000,16000);
            assert(hasLine("[SOIL_REJECT] node=SOIL-04 reason=duplicate")); frameNode(4,"2.500 V 0m");
            injectNode(4,1234,0,plantsoil::AdcStatus::Ok,8000,17000);
            assert(hasLine("[SOIL_REJECT] node=SOIL-04 reason=old_sequence"));
            injectNode(4,1234,2,plantsoil::AdcStatus::ReadError,0,18000);
            frameNode(1,"2.696 V"); frameNode(2,"1.000 V"); frameNode(3,"3.000 V"); frameNode(4,"DEFAUT ADS","2.500");
            injectNode(4,1234,3,plantsoil::AdcStatus::Ok,26400,19000);
            host::radio().receiveCode=-3;
            injectNode(4,1234,3,plantsoil::AdcStatus::Ok,8000,20000);
            frame("ERREUR RADIO"); frameNode(4,"3.300 V 0m");
            host::radio().receiveCode=0;
            host::now=30000; loop(); host::finishInitialization(); loop();
            frame("RADIO ECOUTE"); frameNode(1,"2.696 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"3.000 V 0m"); frameNode(4,"3.300 V 0m");
            injectNode(4,1234,2,plantsoil::AdcStatus::Ok,8000,79000);
            frameNode(1,"2.696 V 1m"); frameNode(2,"1.000 V 1m"); frameNode(3,"3.000 V 1m"); frameNode(4,"3.300 V 1m");
            const uint32_t expires4=19000+plantsoil::kStaleMs;
            injectNode(1,5678,1,plantsoil::AdcStatus::Ok,16000,expires4-100);
            injectNode(2,5678,1,plantsoil::AdcStatus::Ok,8000,expires4-90);
            injectNode(3,5678,1,plantsoil::AdcStatus::Ok,20000,expires4-80);
            host::now=expires4-1; loop();
            frameNode(1,"2.000 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"2.500 V 0m"); frameNode(4,"3.300 V 89m");
            host::now=expires4; loop(); frameNode(4,"PERIMEE","3.300");
            frameNode(1,"2.000 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"2.500 V 0m");
            injectNode(4,5678,1,plantsoil::AdcStatus::Ok,20000,expires4+10);
            frameNode(4,"2.500 V 0m");
            bytes[3]=5; host::now=expires4+20; host::inject(bytes,sizeof(bytes)); loop();
            assert(hasLine("reason=node")); frameNode(4,"2.500 V 0m"); frameNode(1,"2.000 V 0m");
            injectNode(4,9000,0xFFFFFFFF,plantsoil::AdcStatus::Ok,16000,expires4+30);
            frameNode(4,"2.000 V 0m");
            injectNode(4,9000,0,plantsoil::AdcStatus::Ok,24000,expires4+40);
            assert(hasLine("node=SOIL-04 session=9000 seq=0")); frameNode(4,"3.000 V 0m");
            injectNode(4,9000,0xFFFFFFFF,plantsoil::AdcStatus::Ok,8000,expires4+60040);
            frameNode(4,"3.000 V 1m"); frameNode(1,"2.000 V 1m"); frameNode(2,"1.000 V 1m"); frameNode(3,"2.500 V 1m");
            injectNode(4,9000,1,plantsoil::AdcStatus::ReadError,0,host::now+10);
            const uint32_t faultExpires=host::now+plantsoil::kStaleMs;
            injectNode(1,9001,1,plantsoil::AdcStatus::Ok,16000,faultExpires-100);
            injectNode(2,9001,1,plantsoil::AdcStatus::Ok,8000,faultExpires-90);
            injectNode(3,9001,1,plantsoil::AdcStatus::Ok,20000,faultExpires-80);
            host::now=faultExpires-1;loop();
            frameNode(4,"DEFAUT ADS"); frameNode(1,"2.000 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"2.500 V 0m");
            host::now=faultExpires;loop();
            frameNode(4,"PERIMEE"); frameNode(1,"2.000 V 0m"); frameNode(2,"1.000 V 0m"); frameNode(3,"2.500 V 0m");
            injectNode(4,9000,2,plantsoil::AdcStatus::Ok,20000,faultExpires+10);
            frameNode(4,"2.500 V 0m");
            puts("PASS quatre sondes : etats, compteurs, reprise radio, expiration exacte et huit lignes OLED");
            return 0;
        }
        frameNode(1, "2.696 V 0m"); frameNode(2, "1.000 V 0m");
        injectNode(2, 1234, 1, plantsoil::AdcStatus::Ok, 8000, 72000);
        assert(hasLine("reason=duplicate"));
        frameNode(1, "2.696 V 1m"); frameNode(2, "1.000 V 0m");
        injectNode(1, 1234, 2, plantsoil::AdcStatus::ReadError, 0, 73000);
        frameNode(1, "DEFAUT ADS", "2.696"); frameNode(2, "1.000 V 1m");
        bytes[3] = 5;
        host::now = 74000; host::inject(bytes, sizeof(bytes)); loop();
        assert(hasLine("reason=node")); frameNode(2, "1.000 V 1m");
        host::now = 13000 + plantsoil::kStaleMs - 1; loop();
        frameNode(1, "DEFAUT ADS"); frameNode(2, "1.000 V 89m");
        host::now = 13000 + plantsoil::kStaleMs; loop();
        frameNode(1, "DEFAUT ADS"); frameNode(2, "PERIMEE", "1.000");
        if (scenario == "three") {
            host::now=14000 + plantsoil::kStaleMs - 1; loop();
            frameNode(1, "DEFAUT ADS"); frameNode(2, "PERIMEE"); frameNode(3, "3.000 V 89m");
            host::now=14000 + plantsoil::kStaleMs; loop();
            frameNode(1, "DEFAUT ADS"); frameNode(2, "PERIMEE"); frameNode(3, "PERIMEE", "3.000");
        }
        injectNode(1, 5678, 1, plantsoil::AdcStatus::Ok, 16000, host::now + 10);
        frameNode(1, "2.000 V 0m"); frameNode(2, "PERIMEE");
        injectNode(2, 5678, 1, plantsoil::AdcStatus::Ok, 24000, host::now + 10);
        frameNode(1, "2.000 V 0m"); frameNode(2, "3.000 V 0m"); frame("Lux 537.6");
        injectNode(2, 5678, 2, plantsoil::AdcStatus::ReadError, 0, host::now + 10);
        frameNode(1, "2.000 V 0m"); frameNode(2, "DEFAUT ADS", "3.000");
        injectNode(2, 5678, 3, plantsoil::AdcStatus::Ok, 24000, host::now + 10);
        frameNode(1, "2.000 V 0m"); frameNode(2, "3.000 V 0m");
        host::radio().receiveCode = -3;
        injectNode(2, 5678, 3, plantsoil::AdcStatus::Ok, 8000, host::now + 10);
        const uint32_t restartFailedAt = host::now;
        frame("ERREUR RADIO"); frameNode(1, "2.000 V"); frameNode(2, "3.000 V");
        host::radio().receiveCode = 0;
        host::now = restartFailedAt + 10000; loop(); host::finishInitialization(); loop();
        frame("RADIO ECOUTE"); frameNode(1, "2.000 V"); frameNode(2, "3.000 V");
        injectNode(1, 5678, 1, plantsoil::AdcStatus::Ok, 8000, restartFailedAt + 60000);
        assert(hasLine("[SOIL_REJECT] node=SOIL-01 reason=duplicate"));
        frameNode(1, "2.000 V 1m"); frameNode(2, "3.000 V 1m");
        injectNode(2, 5678, 2, plantsoil::AdcStatus::Ok, 8000, host::now + 10);
        assert(hasLine("[SOIL_REJECT] node=SOIL-02 reason=old_sequence"));
        frameNode(1, "2.000 V 1m"); frameNode(2, "3.000 V 1m");
        if (scenario == "three") {
            frameNode(3, "PERIMEE", "3.000");
            injectNode(3, 5678, 1, plantsoil::AdcStatus::ReadError, 0, host::now + 10);
            frameNode(1, "2.000 V 1m"); frameNode(2, "3.000 V 1m"); frameNode(3, "DEFAUT ADS");
            injectNode(3, 5678, 2, plantsoil::AdcStatus::Ok, 20000, host::now + 10);
            frameNode(1, "2.000 V 1m"); frameNode(2, "3.000 V 1m"); frameNode(3, "2.500 V 0m");
            injectNode(3, 5678, 2, plantsoil::AdcStatus::Ok, 8000, host::now + 10);
            assert(hasLine("[SOIL_REJECT] node=SOIL-03 reason=duplicate")); frameNode(3, "2.500 V 0m");
        }
        puts(scenario == "three" ? "PASS trois sondes : etats, reprise, peremption individuelle et trois lignes OLED" :
             "PASS deux sondes : etats, sessions et ages isoles, peremption individuelle et deux valeurs OLED");
        return 0;
    }
    host::now = 60000; host::inject(bytes, sizeof(bytes)); loop();
    assert(hasLine("reason=duplicate")); frame("2.696 V 0m");
    bytes[2] = 2;
    host::now = 90000; host::inject(bytes, sizeof(bytes)); loop();
    assert(hasLine("reason=header")); frame("2.696 V 1m");
    host::now = 12000 + plantsoil::kTransmitMs; loop(); frame("2.696 V");
    const uint32_t staleAt = 12000 + plantsoil::kStaleMs;
    host::now = staleAt - 1; loop(); frame("2.696 V");
    host::now = staleAt; loop(); frame("PERIMEE", "2.696 V"); frame("Lux 537.6");
    packet.session = 5678; packet.sequence = 0; packet.status = plantsoil::AdcStatus::ReadError; packet.raw = 0;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = staleAt + 2000; host::inject(bytes, sizeof(bytes)); loop();
    frame("DEFAUT ADS", "2.696 V");
    packet.sequence++; packet.status = plantsoil::AdcStatus::Ok; packet.raw = 8000;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = staleAt + 3000; host::inject(bytes, sizeof(bytes)); loop(); frame("1.000 V");
    host::radio().readCode = RADIOLIB_ERR_CRC_MISMATCH;
    packet.sequence++; packet.raw = 16000;
    plantsoil::encode(packet, bytes, sizeof(bytes));
    host::now = staleAt + 4000; host::inject(bytes, sizeof(bytes)); loop(); frame("1.000 V 0m");
    frame("1.000 V", "2.000 V");
    host::radio().readCode = 0;
    host::radio().hasCrc = false;
    host::now = staleAt + 4100; host::inject(bytes, sizeof(bytes)); loop();
    frame("1.000 V", "2.000 V");
    assert(hasLine("reason=header_crc"));
    host::now = staleAt + 4999; loop(); frame("1.000 V 0m");
    host::radio().hasCrc = true;
    host::radio().standbyCode = -8;
    const auto receives = host::radio().receives;
    host::inject(bytes, sizeof(bytes)); loop();
    assert(host::radio().receives == receives + 1 && hasLine("reason=standby"));
    host::radio().standbyCode = 0;
    host::radio().receiveCode = -3;
    host::now = staleAt + 5000; host::inject(bytes, sizeof(bytes)); loop(); frame("ERREUR RADIO"); frame("Lux 537.6");
    host::buttonPressed() = true;
    host::now = staleAt + 5100; loop(); host::now = staleAt + 5140; loop();
    host::now = staleAt + 8140; loop(); frame("Relacher PRG");
    host::buttonPressed() = false;
    host::now = staleAt + 8200; loop(); host::now = staleAt + 8240;
    try { loop(); assert(false); } catch (const SleepEntered&) {}
    const auto& calls = host::gpioCalls();
    const auto radioSleep = std::find(calls.begin(), calls.end(), "radio_sleep");
    const auto spiEnd = std::find(calls.begin(), calls.end(), "spi_end");
    assert(radioSleep < spiEnd && spiEnd < std::find(calls.begin(), calls.end(), "deep_sleep"));
    assert(std::find(calls.begin(), calls.end(), scenario == "sleep_failed" ? "write12:0" : "write12:1") != calls.end());
    assert(Serial.flushed);
    puts("PASS vrais radio/main/OLED : RX, age inchange au rejet, peremption, defaut sans valeur, lux concurrents, reprise, PRG et sommeil/reset de secours");
}
