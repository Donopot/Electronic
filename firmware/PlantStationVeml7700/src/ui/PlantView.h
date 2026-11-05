#pragma once
#include <PlantSoilProtocol.h>
#include <stdint.h>
#include <stddef.h>

namespace plant {
enum class PlantState : uint8_t { Waiting, Uncalibrated, Healthy, Dry, AdcFault, Stale };
enum class Mood : uint8_t { Neutral, Happy, Sad, Worried };
enum class Screen : uint8_t { Face, Details, Qr };
inline Screen nextScreen(Screen value) { return static_cast<Screen>((static_cast<unsigned>(value) + 1) % 3); }
struct PlantConfig {
    char name[49] = {}; // 24 Unicode characters, at most 48 UTF-8 bytes.
    bool calibrated = false;
    bool dryAbove = true;
    int32_t dryMicrovolts = 0;
};
struct PlantReading {
    uint8_t id = 0;
    char name[49] = {};
    PlantState state = PlantState::Waiting;
    bool seen = false, voltageValid = false;
    // PS v1 has no battery telemetry. Its UI/API state is explicitly unknown.
    bool batteryKnown = false;
    // RSSI/SNR are measured locally by the hub receiver, never sent by the node.
    bool signalValid = false;
    int16_t rssiDbm = 0, snrDb = 0;
    uint32_t ageMs = 0;
    int32_t microvolts = 0;
    plantsoil::Packet packet;
};
struct PlantView {
    PlantReading plants[plantsoil::kNodeCount];
    plantsoil::Counters counters;
    Mood mood = Mood::Worried;
    uint8_t dryMask = 0, warningMask = 0;
    bool radioReady = false, luxValid = false;
    double lux = 0;
    uint64_t uptimeMs = 0;
};
void defaultPlants(PlantConfig* config);
bool validPlantName(const char* name);
const char* stateName(PlantState value);
const char* stateLabel(PlantState value);
const char* moodName(Mood value);
PlantView makePlantView(const plantsoil::ReceiverBank& bank, const PlantConfig* config,
                        bool radioReady, uint32_t now, uint64_t uptimeMs);
}
