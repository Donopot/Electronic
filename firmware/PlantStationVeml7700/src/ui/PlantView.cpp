#include "config.h"
#if PLANT_DASHBOARD
#include "PlantView.h"
#include <stdio.h>
#include <string.h>
namespace plant {
void defaultPlants(PlantConfig* config) {
    for (unsigned i = 0; i < plantsoil::kNodeCount; ++i) {
        config[i] = PlantConfig{};
        snprintf(config[i].name, sizeof(config[i].name), "Plante %u", i + 1);
    }
}
bool validPlantName(const char* name) {
    if (!name || !*name || strlen(name) > 48 || *name == ' ') return false;
    unsigned characters = 0;
    const auto* p = reinterpret_cast<const unsigned char*>(name);
    while (*p) {
        uint32_t cp; unsigned n;
        if (*p < 0x80) { cp = *p; n = 1; }
        else if (*p >= 0xC2 && *p <= 0xDF) { cp = *p & 31; n = 2; }
        else if (*p >= 0xE0 && *p <= 0xEF) { cp = *p & 15; n = 3; }
        else if (*p >= 0xF0 && *p <= 0xF4) { cp = *p & 7; n = 4; }
        else return false;
        for (unsigned i = 1; i < n; ++i) {
            if (!p[i] || (p[i] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i] & 63);
        }
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) || cp < 0x20 ||
            (cp >= 0x7F && cp <= 0x9F)) return false;
        p += n;
        if (++characters > 24) return false;
    }
    return p[-1] != ' ';
}
const char* stateName(PlantState value) {
    static const char* names[] = {"waiting", "uncalibrated", "healthy", "dry", "adc_fault", "stale"};
    return names[static_cast<unsigned>(value)];
}
const char* stateLabel(PlantState value) {
    static const char* labels[] = {"En attente", "À étalonner", "Satisfaisant", "Sèche", "Défaut ADC", "Périmée"};
    return labels[static_cast<unsigned>(value)];
}
const char* moodName(Mood value) {
    static const char* names[] = {"neutral", "happy", "sad", "worried"};
    return names[static_cast<unsigned>(value)];
}
PlantView makePlantView(const plantsoil::ReceiverBank& bank, const PlantConfig* config,
                        bool radioReady, uint32_t now, uint64_t uptimeMs) {
    PlantView out;
    out.radioReady = radioReady; out.uptimeMs = uptimeMs;
    bool allHealthy = true;
    for (unsigned i = 0; i < plantsoil::kNodeCount; ++i) {
        auto& value = out.plants[i];
        const auto& source = bank.node(i + 1);
        value.id = i + 1;
        memcpy(value.name, config[i].name, sizeof(value.name));
        const auto freshness = source.state(now);
        value.seen = freshness != plantsoil::Freshness::Waiting;
        if (value.seen) {
            value.packet = source.packet(); value.ageMs = source.age(now);
            value.voltageValid = source.packet().status == plantsoil::AdcStatus::Ok;
            value.microvolts = plantsoil::microvolts(source.packet().raw);
        }
        if (freshness == plantsoil::Freshness::Waiting) value.state = PlantState::Waiting;
        else if (freshness == plantsoil::Freshness::Stale) value.state = PlantState::Stale;
        else if (freshness == plantsoil::Freshness::AdcFault) value.state = PlantState::AdcFault;
        else if (!config[i].calibrated) value.state = PlantState::Uncalibrated;
        else {
            const bool dry = config[i].dryAbove ? value.microvolts >= config[i].dryMicrovolts : value.microvolts <= config[i].dryMicrovolts;
            value.state = dry ? PlantState::Dry : PlantState::Healthy;
        }
        if (value.state == PlantState::Dry) out.dryMask |= 1U << i;
        if (value.state == PlantState::Waiting || value.state == PlantState::Stale || value.state == PlantState::AdcFault)
            out.warningMask |= 1U << i;
        allHealthy &= value.state == PlantState::Healthy;
    }
    out.mood = out.dryMask ? Mood::Sad : out.warningMask || !radioReady ? Mood::Worried : allHealthy ? Mood::Happy : Mood::Neutral;
    return out;
}
}

#endif
