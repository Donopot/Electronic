#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Transport diagnostic : identifiants et CRC n'authentifient pas l'emetteur.
namespace plantsoil {
constexpr uint8_t kVersion = 1, kNode = 1;
constexpr uint8_t kNodeCount = 4;
constexpr bool validNode(uint8_t node) { return node >= 1 && node <= kNodeCount; }
constexpr size_t kPacketSize = 20;
constexpr uint32_t kTransmitMs = 30UL * 60UL * 1000UL;
constexpr uint32_t kStaleMs = 3UL * kTransmitMs;
enum class AdcStatus : uint8_t { Ok = 0, Unavailable = 1, ReadError = 2, RangeError = 3 };
struct Packet {
    uint8_t node = kNode;
    uint32_t session = 0, sequence = 0, uptimeMs = 0;
    AdcStatus status = AdcStatus::Unavailable;
    uint8_t address = 0;
    int16_t raw = 0;
};
enum class Result { Accepted, Length, Header, Node, Session, Status, Address, Raw, Duplicate, OldSequence };
inline const char* resultName(Result result) {
    switch (result) {
    case Result::Accepted: return "accepted";
    case Result::Length: return "length";
    case Result::Header: return "header";
    case Result::Node: return "node";
    case Result::Session: return "session";
    case Result::Status: return "status";
    case Result::Address: return "address";
    case Result::Raw: return "raw";
    case Result::Duplicate: return "duplicate";
    default: return "old_sequence";
    }
}
inline const char* statusName(AdcStatus status) {
    switch (status) {
    case AdcStatus::Ok: return "ok";
    case AdcStatus::Unavailable: return "adc_unavailable";
    case AdcStatus::ReadError: return "adc_error";
    default: return "adc_range";
    }
}
inline Result validate(const Packet& packet) {
    if (!validNode(packet.node)) return Result::Node;
    if (!packet.session) return Result::Session;
    if (static_cast<uint8_t>(packet.status) > 3) return Result::Status;
    const bool adsAddress = packet.address >= 0x48 && packet.address <= 0x4B;
    if (packet.status == AdcStatus::Unavailable) {
        if (packet.address != 0) return Result::Address;
    } else if (!adsAddress) return Result::Address;
    // Entree simple nominale 0..3,3 V ; tolerance de huit codes negatifs d'offset.
    if (packet.status == AdcStatus::Ok) {
        if (packet.raw < -8 || packet.raw > 26400) return Result::Raw;
    } else if (packet.raw != 0) return Result::Raw;
    return Result::Accepted;
}
inline void put32(uint8_t* out, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}
inline uint32_t get32(const uint8_t* in) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) value |= static_cast<uint32_t>(in[i]) << (8 * i);
    return value;
}
inline Result encode(const Packet& packet, uint8_t* out, size_t size) {
    if (!out || size != kPacketSize) return Result::Length;
    const Result valid = validate(packet);
    if (valid != Result::Accepted) return valid;
    out[0] = 'P'; out[1] = 'S'; out[2] = kVersion; out[3] = packet.node;
    put32(out + 4, packet.session); put32(out + 8, packet.sequence); put32(out + 12, packet.uptimeMs);
    out[16] = static_cast<uint8_t>(packet.status); out[17] = packet.address;
    const uint16_t bits = static_cast<uint16_t>(packet.raw);
    out[18] = static_cast<uint8_t>(bits); out[19] = static_cast<uint8_t>(bits >> 8);
    return Result::Accepted;
}
inline Result decode(const uint8_t* in, size_t size, Packet& out) {
    if (!in || size != kPacketSize) return Result::Length;
    if (in[0] != 'P' || in[1] != 'S' || in[2] != kVersion) return Result::Header;
    Packet value;
    value.node = in[3];
    value.session = get32(in + 4); value.sequence = get32(in + 8); value.uptimeMs = get32(in + 12);
    value.status = static_cast<AdcStatus>(in[16]); value.address = in[17];
    const uint16_t bits = static_cast<uint16_t>(in[18]) | (static_cast<uint16_t>(in[19]) << 8);
    // Decodage signe explicite, sans conversion u16 -> i16 dependante du compilateur.
    value.raw = static_cast<int16_t>(bits <= 32767 ? static_cast<int32_t>(bits) : static_cast<int32_t>(bits) - 65536);
    const Result valid = validate(value);
    if (valid == Result::Accepted) out = value;
    return valid;
}
inline int32_t microvolts(int16_t raw) { return static_cast<int32_t>(raw) * 125; }
enum class Freshness { Waiting, Current, AdcFault, Stale };
class Receiver {
public:
    Result accept(const uint8_t* data, size_t size, uint32_t now) {
        Packet next;
        const Result parsed = decode(data, size, next);
        if (parsed != Result::Accepted) return parsed;
        if (seen_ && next.node != packet_.node) return Result::Node;
        if (seen_ && next.session == packet_.session) {
            const uint32_t delta = next.sequence - packet_.sequence;
            if (!delta) return Result::Duplicate;
            if (delta >= 0x80000000UL) return Result::OldSequence;
        }
        packet_ = next; receivedAt_ = now; seen_ = true; stale_ = false;
        return Result::Accepted;
    }
    Freshness state(uint32_t now) const {
        if (!seen_) return Freshness::Waiting;
        // Appeler a chaque tour : une expiration observee reste acquise meme
        // apres un tour complet de millis(), jusqu'au prochain paquet accepte.
        if (age(now) >= kStaleMs) stale_ = true;
        if (stale_) return Freshness::Stale;
        return packet_.status == AdcStatus::Ok ? Freshness::Current : Freshness::AdcFault;
    }
    uint32_t age(uint32_t now) const { return static_cast<uint32_t>(now - receivedAt_); }
    const Packet& packet() const { return packet_; }
    // Mesure locale du recepteur SX1262 sur le dernier paquet accepte. PS v1
    // ne transporte ni RSSI ni SNR : ces deux valeurs ne viennent pas de la
    // sonde et ne sont donc jamais persistees dans le journal.
    void noteSignal(int16_t rssi, int16_t snr, uint32_t now) {
        rssi_ = rssi; snr_ = snr; signalAt_ = now; signal_ = true;
    }
    bool hasSignal() const { return signal_; }
    int16_t rssi() const { return rssi_; }
    int16_t snr() const { return snr_; }
    uint32_t signalAge(uint32_t now) const { return static_cast<uint32_t>(now - signalAt_); }
private:
    Packet packet_;
    uint32_t receivedAt_ = 0;
    bool seen_ = false;
    mutable bool stale_ = false;
    int16_t rssi_ = 0, snr_ = 0;
    uint32_t signalAt_ = 0;
    bool signal_ = false;
};
// Compteurs de trames du concentrateur, publies tels quels par l'API locale.
struct Counters {
    uint32_t accepted = 0, rejected = 0;
    uint32_t length = 0, header = 0, node = 0, session = 0, status = 0, address = 0, raw = 0, duplicate = 0, oldSequence = 0;
};
inline void countRejected(Counters& counters, Result result) {
    ++counters.rejected;
    switch (result) {
    case Result::Length: ++counters.length; break;
    case Result::Header: ++counters.header; break;
    case Result::Node: ++counters.node; break;
    case Result::Session: ++counters.session; break;
    case Result::Status: ++counters.status; break;
    case Result::Address: ++counters.address; break;
    case Result::Raw: ++counters.raw; break;
    case Result::Duplicate: ++counters.duplicate; break;
    default: ++counters.oldSequence; break;
    }
}
// Chaque identifiant conserve son propre compteur, sa session et son expiration.
class ReceiverBank {
public:
    Result accept(const uint8_t* data, size_t size, uint32_t now) {
        Packet packet;
        const Result result = decode(data, size, packet);
        if (result != Result::Accepted) { countRejected(counters_, result); return result; }
        const Result accepted = nodes_[packet.node - 1].accept(data, size, now);
        if (accepted == Result::Accepted) ++counters_.accepted;
        else countRejected(counters_, accepted);
        return accepted;
    }
    void noteSignal(uint8_t id, int16_t rssi, int16_t snr, uint32_t now) {
        if (validNode(id)) nodes_[id - 1].noteSignal(rssi, snr, now);
    }
    const Receiver& node(uint8_t id) const {
        return validNode(id) ? nodes_[id - 1] : empty_;
    }
    const Counters& counters() const { return counters_; }
    void update(uint32_t now) const {
        for (const auto& value : nodes_) value.state(now);
    }
private:
    Receiver nodes_[kNodeCount];
    Receiver empty_;
    Counters counters_;
};
// Ligne commune pour comparer TX/RX sans modifier le schema JSON lux existant.
inline bool formatPacket(char* out, size_t size, const char* direction, const Packet& packet) {
    const int count = snprintf(out, size,
        "[SOIL_%s] authenticated=no node=SOIL-%02u session=%lu seq=%lu uptime_ms=%lu status=%s address=%u raw=%d uv=%ld",
        direction, static_cast<unsigned>(packet.node), static_cast<unsigned long>(packet.session), static_cast<unsigned long>(packet.sequence),
        static_cast<unsigned long>(packet.uptimeMs), statusName(packet.status), packet.address, packet.raw,
        static_cast<long>(packet.status == AdcStatus::Ok ? microvolts(packet.raw) : 0));
    return count >= 0 && static_cast<size_t>(count) < size;
}
}
