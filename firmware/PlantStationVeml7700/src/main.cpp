#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>
#include "config.h"
#if PLANT_LUX
#include "sensors/StrictWireTransport.h"
#endif
#include "diagnostics/Diagnostics.h"
#include "display/OledDisplay.h"
#include "power/PrgButton.h"
#include "power/SleepPower.h"
#include "radio/SoilRadio.h"
#if PLANT_DASHBOARD
#include "ui/PlantView.h"
#include "web/PlantDashboard.h"
#include <math.h>
#endif

namespace {
#if PLANT_LUX
uint32_t readClock() { return millis(); }
plant::StrictWireTransport<TwoWire> transport(Wire);
plant::VEML7700Sensor sensor(transport, readClock);
#endif
plant::Uptime uptime;
plant::OledDisplay oled;
plant::PrgButton button;
#if PLANT_SOIL_LORA
plant::SoilRadio soilRadio;
plant::Measurement lastLux;
#if PLANT_DASHBOARD
plant::PlantDashboard dashboard;
plant::PlantConfig plantConfig[plantsoil::kNodeCount];
plant::Screen screen = plant::Screen::Face;
uint32_t lastDashboardDisplay = 0;
bool displayedSetupActive=false, displayedSetupSuccess=false, setupHeld=false;
#endif
plantsoil::Freshness displayedSoil[plantsoil::kNodeCount] = {};
uint32_t displayedSession[plantsoil::kNodeCount] = {}, displayedSequence[plantsoil::kNodeCount] = {};
bool displayedRadioReady = false;
void refreshSoil(uint32_t now, bool force = false, bool render = true) {
    const auto& readings = soilRadio.readings();
#if PLANT_DASHBOARD
    dashboard.configuration(plantConfig);
    auto view = plant::makePlantView(readings, plantConfig, soilRadio.ready(), now, uptime.update(now));
    view.luxValid = plant::kLux && lastLux.status == plant::Status::Ok && isfinite(lastLux.lux) && lastLux.lux >= 0 && lastLux.lux <= plant::kMaxLux;
    view.lux = view.luxValid ? lastLux.lux : 0;
    dashboard.publish(view);
    if(!render)return; // Keep button prompts visible while radio data still reaches the worker.
    const auto network=dashboard.network();
    if((network.provisioning&&!displayedSetupActive)||(network.setupSuccess&&!displayedSetupSuccess))screen=plant::Screen::Qr;
    displayedSetupActive=network.provisioning;displayedSetupSuccess=network.setupSuccess;
#endif
    if(!render)return;
    bool changed = force || soilRadio.ready() != displayedRadioReady;
    for (uint8_t i = 0; i < plantsoil::kNodeCount; ++i) {
        const auto& node = readings.node(i + 1);
        const auto state = node.state(now);
        const auto& packet = node.packet();
        changed |= state != displayedSoil[i] || packet.session != displayedSession[i] || packet.sequence != displayedSequence[i];
        displayedSoil[i] = state;
        displayedSession[i] = packet.session;
        displayedSequence[i] = packet.sequence;
    }
    if (changed) {
#if PLANT_DASHBOARD
        oled.showDashboard(view, screen, network);
        lastDashboardDisplay = now;
#else
        oled.showSoil(lastLux, readings, soilRadio.ready(), now);
#endif
        displayedRadioReady = soilRadio.ready();
    }
#if PLANT_DASHBOARD
    else if (plant::elapsed(now, lastDashboardDisplay, 1000)) {
        oled.showDashboard(view, screen, network);
        lastDashboardDisplay = now;
    }
#endif
}
#endif
bool sleepRequested = false;
bool sleepReleased = false;
#if PLANT_LUX
uint32_t lastSample = 0;
plant::I2cScanner<plant::StrictWireTransport<TwoWire>> scanner(plant::kScan);
bool started = false;
#endif
void emit(const char* event, const plant::Measurement& measurement, const char* message = "", int address = -1) {
    char line[640];
    if (plant::Diagnostics::format(line, sizeof(line), plant::kJson, event,
                                  uptime.update(millis()), measurement, message, address)) Serial.println(line);
}
#if PLANT_LUX
void startSensor(uint32_t now) {
    plant::Measurement result;
    result.status = sensor.begin(now);
    emit("configuration", result, result.status == plant::Status::WarmingUp ?
         "Configuration verifiee : gain 1/8, integration 100 ms, economie desactivee" :
         plant::diagnosticMessage(result.status), plant::kAddress);
    started = true;
    lastSample = now;
}
#endif
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
#if PLANT_LUX
    const plant::Status bus = transport.restart();
    if (bus != plant::Status::Ok) {
        state.status = bus;
        emit("bus", state, "Initialisation I2C impossible");
        startSensor(millis());
    }
    if (!plant::kScan && !started) startSensor(millis());
#endif
#if PLANT_SOIL_LORA
    soilRadio.begin();
#endif
#if PLANT_DASHBOARD
    plant::defaultPlants(plantConfig);
    dashboard.begin();
    refreshSoil(millis(), true);
#endif
}
void loop() {
    const uint32_t now = millis();
    uptime.update(now);
    const auto buttonEvent = button.update(now, digitalRead(0) == LOW, PLANT_DASHBOARD);
#if PLANT_DASHBOARD
    if (buttonEvent == plant::ButtonEvent::ShortPress) {
        // Trois ecrans seulement : visage, details, QR unique.
        screen=plant::nextScreen(screen);
        refreshSoil(now, true);
    }
    if(buttonEvent==plant::ButtonEvent::SetupHold) {
        sleepRequested=false;setupHeld=true;oled.showSetupPrompt();
    }
    if(buttonEvent==plant::ButtonEvent::ReleasedForSetup) {
        sleepRequested=setupHeld=false;screen=plant::Screen::Qr;dashboard.requestSetup();refreshSoil(now,true);
    }
#endif
    if (buttonEvent == plant::ButtonEvent::LongPress) {
        sleepRequested = true;
        plant::Measurement state;
        emit("veille_demande", state, "Appui long valide ; relacher PRG pour la veille");
        oled.showSleepPrompt(PLANT_DASHBOARD);
    }
    if (buttonEvent == plant::ButtonEvent::ReleasedForSleep) {
        sleepReleased = true;
#if PLANT_DASHBOARD
        dashboard.requestStop();
#endif
    }
    if (sleepReleased) {
#if PLANT_SOIL_LORA
        soilRadio.service(now);
        if (soilRadio.initializing()) { yield(); return; }
#endif
#if PLANT_DASHBOARD
        if (!dashboard.stopped()) { yield(); return; }
#endif
        plant::Measurement state;
#if PLANT_LUX
        state.status = transport.shutdown();
        emit("capteur_veille", state, state.status == plant::Status::Ok ? "VEML7700 en sommeil confirme" : "Sommeil capteur non confirme ; poursuite de la veille", plant::kAddress);
#else
        state.status = plant::Status::Ok;
        emit("capteur_veille", state, "Aucun capteur de lumiere monte sur ce concentrateur", plant::kAddress);
#endif
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
    if (sleepRequested
#if PLANT_DASHBOARD
        || setupHeld
#endif
       ) {
#if PLANT_DASHBOARD
        soilRadio.service(now);
        refreshSoil(now,false,false);
#endif
        yield(); return;
    }
#if PLANT_SOIL_LORA
    soilRadio.service(now);
    refreshSoil(now);
#endif
#if PLANT_LUX
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
#endif
    yield();
}
