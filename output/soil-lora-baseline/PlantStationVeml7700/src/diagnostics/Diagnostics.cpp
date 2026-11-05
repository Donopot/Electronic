#include "Diagnostics.h"
#include "config.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

namespace plant {
const char* diagnosticMessage(Status status) {
    switch (status) {
    case Status::Ok: return "Luminosite ambiante";
    case Status::WarmingUp: return "Attendre la nouvelle conversion apres configuration";
    case Status::Nack: return "Aucun ACK : verifier VIN=3V3, GND, SDA=4 et SCL=5; reprise automatique";
    case Status::ShortRead: return "Lecture incomplete : verifier connexions et alimentation; reprise automatique";
    case Status::BusError: return "Bus I2C en erreur : verifier SDA/SCL, courts-circuits et rappels 3V3; reprise automatique";
    case Status::ConfigLost: return "Configuration perdue : coupure possible; reconfiguration automatique puis nouvelle conversion";
    case Status::Saturated: return "Saturation ALS=65535 ou estimation hors plage 140000 lux; reduire la lumiere";
    }
    return "Erreur inconnue";
}
namespace {
bool escape(const char* input, char* output, size_t capacity) {
    size_t used = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(input); *p; ++p) {
        if (*p < 32) {
            if (used + 6 >= capacity) return false;
            snprintf(output + used, capacity - used, "\\u%04x", *p); used += 6;
        } else {
            if (used + ((*p == '"' || *p == '\\') ? 2 : 1) >= capacity) return false;
            if (*p == '"' || *p == '\\') output[used++] = '\\';
            output[used++] = static_cast<char>(*p);
        }
    }
    output[used] = 0;
    return true;
}
}
bool Diagnostics::format(char* output, size_t capacity, bool json, const char* event,
                         uint64_t uptime, const Measurement& value,
                         const char* message, int address) {
    const bool measurement = strcmp(event, "mesure") == 0;
    const bool valid = measurement && value.status == Status::Ok && isfinite(value.lux) && value.lux >= 0;
    const Status status = measurement && value.status == Status::Ok && !valid ? Status::BusError : value.status;
    char lux[48] = "null";
    char raw[12] = "null";
    if (valid) snprintf(lux, sizeof(lux), "%.4f", value.lux);
    if (measurement && (valid || status == Status::Saturated)) snprintf(raw, sizeof(raw), "%u", value.raw);
    int count;
    if (json) {
        char safeEvent[80], safeMessage[240];
        if (!escape(event, safeEvent, sizeof(safeEvent)) ||
            !escape(message, safeMessage, sizeof(safeMessage))) return false;
        count = snprintf(output, capacity,
            "{\"type\":\"plant_station_veml7700\",\"sensor\":\"VEML7700\",\"event\":\"%s\",\"version\":\"%s\","
            "\"uptime_ms\":%llu,\"status\":\"%s\",\"raw\":%s,\"lux\":%s,"
            "\"address\":%d,\"message\":\"%s\"}",
            safeEvent, kVersion, static_cast<unsigned long long>(uptime), statusName(status),
            raw, lux, address, safeMessage);
    } else {
        char displayAddress[16];
        if (address < 0) snprintf(displayAddress, sizeof(displayAddress), "globale");
        else snprintf(displayAddress, sizeof(displayAddress), "0x%02X", static_cast<unsigned int>(address));
        count = snprintf(output, capacity,
            "[PlantStation VEML7700 v%s] uptime_ms=%llu evenement=%s statut=%s raw=%s lux=%s adresse=%s %s",
            kVersion, static_cast<unsigned long long>(uptime), event, statusName(status),
            raw, lux, displayAddress, message);
    }
    return count >= 0 && static_cast<size_t>(count) < capacity;
}
}
