#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>
#include "config.h"
#include "sensors/StrictWireTransport.h"
#include "diagnostics/Diagnostics.h"
#include "display/OledDisplay.h"
#include "power/PrgButton.h"
#include "power/SleepPower.h"
#include "radio/SoilRadio.h"

namespace {
uint32_t readClock() { return millis(); }
plant::StrictWireTransport<TwoWire> transport(Wire);
plant::VEML7700Sensor sensor(transport, readClock);
plant::Uptime uptime;
plant::OledDisplay oled;
plant::PrgButton button;
#if PLANT_SOIL_LORA
plant::SoilRadio soilRadio;
plant::Measurement lastLux;
plantsoil::Freshness displayedSoil = plantsoil::Freshness::Waiting;
uint32_t displayedSession = 0, displayedSequence = 0;
bool displayedRadioReady = false;
void refreshSoil(uint32_t now, bool force = false) {
    const auto& readings = soilRadio.readings();
    const auto state = readings.state(now);
    const auto& packet = readings.packet();
    if (force || state != displayedSoil || packet.session != displayedSession ||
        packet.sequence != displayedSequence || soilRadio.ready() != displayedRadioReady) {
        oled.showSoil(lastLux, readings, soilRadio.ready(), now);
        displayedSoil = state;
        displayedSession = packet.session;
        displayedSequence = packet.sequence;
        displayedRadioReady = soilRadio.ready();
    }
}
#endif
bool sleepRequested = false;
bool sleepReleased = false;
uint32_t lastSample = 0;
plant::I2cScanner<plant::StrictWireTransport<TwoWire>> scanner(plant::kScan);
bool started = false;
void emit(const char* event, const plant::Measurement& measurement, const char* message = "", int address = -1) {
    char line[640];
    if (plant::Diagnostics::format(line, sizeof(line), plant::kJson, event,
                                  uptime.update(millis()), measurement, message, address)) Serial.println(line);
}
void startSensor(uint32_t now) {
    plant::Measurement result;
    result.status = sensor.begin(now);
    emit("configuration", result, result.status == plant::Status::WarmingUp ?
         "Configuration verifiee : gain 1/8, integration 100 ms, economie desactivee" :
         plant::diagnosticMessage(result.status), plant::kAddress);
    started = true;
    lastSample = now;
}
}
void setup() {
    esp_log_level_set("*", ESP_LOG_NONE);
    Serial.begin(115200);
    plant::SleepPower::restore();
    button.begin(millis(), digitalRead(0) == LOW);
    plant::Measurement state;
    char hardware[180];
    snprintf(hardware, sizeof(hardware),
             "Heltec V3 cible; puce %s revision %u flash %lu octets; UART0 CP2102; SDA=4 SCL=5; sous-revision V3.x non determinee",
             ESP.getChipModel(), ESP.getChipRevision(), static_cast<unsigned long>(ESP.getFlashChipSize()));
    emit("demarrage", state, hardware);
    if (plant::kOled) {
        plant::Measurement screen;
        const bool ready = oled.begin();
        screen.status = ready ? plant::Status::Ok : plant::Status::BusError;
        emit("oled", screen, ready ? "OLED initialise sur I2C1 GPIO17/18" : "OLED indisponible; acquisition conservee", 0x3C);
    }
    const plant::Status bus = transport.restart();
    if (bus != plant::Status::Ok) {
        state.status = bus;
        emit("bus", state, "Initialisation I2C impossible");
        startSensor(millis());
    }
    if (!plant::kScan && !started) startSensor(millis());
#if PLANT_SOIL_LORA
    soilRadio.begin();
#endif
}
void loop() {
    const uint32_t now = millis();
    uptime.update(now);
    const auto buttonEvent = button.update(now, digitalRead(0) == LOW);
    if (buttonEvent == plant::ButtonEvent::LongPress) {
        sleepRequested = true;
        plant::Measurement state;
        emit("veille_demande", state, "Appui long valide ; relacher PRG pour la veille");
        oled.showSleepPrompt();
    }
    if (buttonEvent == plant::ButtonEvent::ReleasedForSleep) {
        sleepReleased = true;
    }
    if (sleepReleased) {
#if PLANT_SOIL_LORA
        soilRadio.service(now);
        if (soilRadio.initializing()) { yield(); return; }
#endif
        plant::Measurement state;
        state.status = transport.shutdown();
        emit("capteur_veille", state, state.status == plant::Status::Ok ? "VEML7700 en sommeil confirme" : "Sommeil capteur non confirme ; poursuite de la veille", plant::kAddress);
        oled.sleep();
        Wire.end();
#if PLANT_SOIL_LORA
        plant::SleepPower::prepare(soilRadio.sleep());
#else
        plant::SleepPower::prepare();
#endif
        emit("veille", state, "Entree en veille profonde ; RST pour redemarrer");
        plant::SleepPower::enter();
        return;
    }
    if (sleepRequested) { yield(); return; }
#if PLANT_SOIL_LORA
    soilRadio.service(now);
    refreshSoil(now);
#endif
    if (!started && !scanner.done()) {
        const auto step = scanner.step(transport);
        plant::Measurement state;
        state.status = step.status;
        if (state.status == plant::Status::Ok)
            emit("scan_ack", state, "ACK recu; identite du composant non garantie", step.address);
        if (state.status == plant::Status::BusError) {
            emit("scan_erreur", state, "Scan interrompu apres erreur de bus", step.address);
        }
        // Une seule adresse par tour; 112 adresses au maximum, timeout borne.
    } else if (!started) {
        plant::Measurement state;
        emit("scan_fin", state, "Fin du scanner I2C");
        startSensor(now);
    } else {
        sensor.service(now);
        if (plant::elapsed(now, lastSample, plant::kIntervalMs)) {
            lastSample = now; // pas de rafale de rattrapage apres une pause
            const auto value = sensor.measure(now);
            emit("mesure", value, plant::diagnosticMessage(value.status), plant::kAddress);
#if PLANT_SOIL_LORA
            lastLux = value;
            refreshSoil(millis(), true);
#else
            oled.show(value, uptime.update(millis()));
#endif
        }
    }
    yield();
}
