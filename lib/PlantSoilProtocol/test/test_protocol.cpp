#include <PlantSoilProtocol.h>
#include <assert.h>
#include <string.h>
#include <initializer_list>
using namespace plantsoil;
int main() {
    static_assert(kTransmitMs == 1800000UL, "Cadence attendue : trente minutes");
    static_assert(kStaleMs == 5400000UL, "Peremption attendue : quatre-vingt-dix minutes");
    Packet source;
    source.session = 0x12345678; source.sequence = 42; source.uptimeMs = 0xFEDCBA98;
    source.address = 0x48; source.status = AdcStatus::Ok; source.raw = 21565;
    uint8_t bytes[kPacketSize], changed[kPacketSize + 1];
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    assert(bytes[4] == 0x78 && bytes[7] == 0x12 && bytes[8] == 42);
    Packet decoded;
    assert(decode(bytes, sizeof(bytes), decoded) == Result::Accepted);
    assert(decoded.raw == 21565 && microvolts(decoded.raw) == 2695625);
    assert(decoded.session == source.session && decoded.uptimeMs == source.uptimeMs);
    for (size_t len = 0; len <= kPacketSize + 1; ++len)
        if (len != kPacketSize) assert(decode(bytes, len, decoded) == Result::Length);
    for (size_t index : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(16), size_t(17)}) {
        memcpy(changed, bytes, kPacketSize); changed[index] = 255;
        const Packet before = decoded;
        assert(decode(changed, kPacketSize, decoded) != Result::Accepted);
        assert(decoded.raw == before.raw && decoded.session == before.session);
    }
    memcpy(changed, bytes, kPacketSize); memset(changed + 4, 0, 4);
    assert(decode(changed, kPacketSize, decoded) == Result::Session);
    source.raw = -8;
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    assert(decode(bytes, sizeof(bytes), decoded) == Result::Accepted && decoded.raw == -8);
    source.raw = -9; assert(encode(source, bytes, sizeof(bytes)) == Result::Raw);
    source.raw = 26400; assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    source.raw = 26401; assert(encode(source, bytes, sizeof(bytes)) == Result::Raw);
    source.raw = 21565;
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    Receiver receiver;
    assert(receiver.state(0) == Freshness::Waiting);
    const uint32_t base = 0xFFFFF000;
    assert(receiver.accept(bytes, sizeof(bytes), base) == Result::Accepted);
    assert(receiver.state(base + kTransmitMs) == Freshness::Current);
    assert(receiver.state(base + kStaleMs - 1) == Freshness::Current);
    assert(receiver.state(base + kStaleMs) == Freshness::Stale);
    assert(receiver.state(base + 100) == Freshness::Stale); // expiration acquise apres un tour complet
    assert(receiver.age(base + kStaleMs + 1) == kStaleMs + 1);
    assert(receiver.accept(bytes, sizeof(bytes), base + 50000) == Result::Duplicate);
    assert(receiver.age(base + 50000) == 50000);
    source.sequence = 41; encode(source, bytes, sizeof(bytes));
    assert(receiver.accept(bytes, sizeof(bytes), base + 60000) == Result::OldSequence);
    memcpy(changed, bytes, kPacketSize); changed[16] = 255;
    assert(receiver.accept(changed, kPacketSize, base + 70000) == Result::Status);
    assert(receiver.state(base + kStaleMs) == Freshness::Stale);
    source.session++; source.sequence = 0xFFFFFFFF; source.status = AdcStatus::ReadError; source.raw = 0;
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    assert(receiver.accept(bytes, sizeof(bytes), 100) == Result::Accepted);
    assert(receiver.state(100) == Freshness::AdcFault);
    source.sequence = 0; source.status = AdcStatus::Ok; source.raw = 8000;
    encode(source, bytes, sizeof(bytes));
    assert(receiver.accept(bytes, sizeof(bytes), 200) == Result::Accepted);
    source.sequence = 0x80000000; encode(source, bytes, sizeof(bytes));
    assert(receiver.accept(bytes, sizeof(bytes), 300) == Result::OldSequence);
    source.session++; source.sequence = 1; source.status = AdcStatus::Unavailable; source.address = 0; source.raw = 0;
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    assert(receiver.accept(bytes, sizeof(bytes), 400) == Result::Accepted);
    assert(receiver.state(400) == Freshness::AdcFault);
    source.raw = 1; assert(encode(source, bytes, sizeof(bytes)) == Result::Raw);
    source.raw = 0; source.address = 0x48; assert(encode(source, bytes, sizeof(bytes)) == Result::Address);
    source.status = AdcStatus::RangeError;
    assert(encode(source, bytes, sizeof(bytes)) == Result::Accepted);
    char line[240]; assert(formatPacket(line, sizeof(line), "TX", source));
    assert(strstr(line, "authenticated=no") && strstr(line, "status=adc_range") && strstr(line, "raw=0 uv=0"));
    assert(!formatPacket(line, 10, "RX", source));
    puts("PASS protocole SOIL : codec strict, brut signe, defauts, session, compteurs/temps circulaires, cadence 30min, frontiere 90min, rejet sans rafraichir l'age");
}
