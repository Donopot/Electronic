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
    const uint8_t legacy[] = {0x50, 0x53, 1, 1, 0x78, 0x56, 0x34, 0x12,
        42, 0, 0, 0, 0x98, 0xBA, 0xDC, 0xFE, 0, 0x48, 0x3D, 0x54};
    assert(memcmp(bytes, legacy, kPacketSize) == 0); // paquet SOIL-01 historique inchangé
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
    Packet one;
    one.session = 77; one.sequence = 1; one.status = AdcStatus::Ok; one.address = 0x48; one.raw = 8000;
    Packet two = one; two.node = 2; two.raw = 16000;
    uint8_t first[kPacketSize], second[kPacketSize];
    assert(encode(one, first, sizeof(first)) == Result::Accepted);
    assert(encode(two, second, sizeof(second)) == Result::Accepted && second[3] == 2);
    assert(decode(second, sizeof(second), decoded) == Result::Accepted && decoded.node == 2);
    assert(formatPacket(line, sizeof(line), "TX", two) && strstr(line, "node=SOIL-02"));
    ReceiverBank bank;
    assert(bank.accept(first, sizeof(first), base) == Result::Accepted);
    assert(bank.accept(second, sizeof(second), base + 1000) == Result::Accepted);
    assert(bank.node(1).packet().raw == 8000 && bank.node(2).packet().raw == 16000);
    assert(bank.accept(first, sizeof(first), base + 2000) == Result::Duplicate);
    assert(bank.node(1).age(base + 2000) == 2000 && bank.node(2).age(base + 2000) == 1000);
    bank.update(base + kStaleMs);
    assert(bank.node(1).state(base + kStaleMs) == Freshness::Stale);
    assert(bank.node(2).state(base + kStaleMs) == Freshness::Current);
    two.sequence++; two.status = AdcStatus::ReadError; two.raw = 0;
    encode(two, second, sizeof(second));
    assert(bank.accept(second, sizeof(second), base + kStaleMs + 10) == Result::Accepted);
    assert(bank.node(2).state(base + kStaleMs + 10) == Freshness::AdcFault);
    assert(bank.node(1).state(base + kStaleMs + 10) == Freshness::Stale);
    one.session++; one.sequence = 0; encode(one, first, sizeof(first));
    assert(bank.accept(first, sizeof(first), base + kStaleMs + 20) == Result::Accepted);
    assert(bank.node(1).state(base + kStaleMs + 20) == Freshness::Current);
    assert(bank.node(2).state(base + kStaleMs + 20) == Freshness::AdcFault);
    second[3] = 3;
    assert(bank.accept(second, sizeof(second), base + kStaleMs + 30) == Result::Node);
    assert(bank.node(2).age(base + kStaleMs + 30) == 20);
    assert(bank.node(0).state(0) == Freshness::Waiting && bank.node(3).state(0) == Freshness::Waiting);
    two.node = 3; assert(encode(two, second, sizeof(second)) == Result::Node);
    ReceiverBank wrapped;
    one.session = 999; one.sequence = 0xFFFFFFFF;
    two = one; two.node = 2; two.sequence = 0xFFFFFFFE; two.raw = 16000;
    encode(one, first, sizeof(first)); encode(two, second, sizeof(second));
    assert(wrapped.accept(first, sizeof(first), base) == Result::Accepted);
    assert(wrapped.accept(second, sizeof(second), base + 10) == Result::Accepted);
    one.sequence = 0; two.sequence = 0xFFFFFFFF;
    encode(one, first, sizeof(first)); encode(two, second, sizeof(second));
    assert(wrapped.accept(first, sizeof(first), base + 20) == Result::Accepted);
    assert(wrapped.accept(second, sizeof(second), base + 30) == Result::Accepted);
    one.sequence = 0xFFFFFFFF; two.sequence = 0xFFFFFFFE;
    encode(one, first, sizeof(first)); encode(two, second, sizeof(second));
    assert(wrapped.accept(first, sizeof(first), base + 40) == Result::OldSequence);
    assert(wrapped.accept(second, sizeof(second), base + 50) == Result::OldSequence);
    assert(wrapped.node(1).packet().sequence == 0 && wrapped.node(2).packet().sequence == 0xFFFFFFFF);
    assert(wrapped.node(1).age(base + 60) == 40 && wrapped.node(2).age(base + 60) == 30);
    one.session = 1000; one.sequence = 1; two.sequence = 0;
    encode(one, first, sizeof(first)); encode(two, second, sizeof(second));
    assert(wrapped.accept(first, sizeof(first), base + 70) == Result::Accepted);
    assert(wrapped.accept(second, sizeof(second), base + 80) == Result::Accepted);
    assert(wrapped.node(1).packet().session == 1000 && wrapped.node(2).packet().session == 999);
    puts("PASS protocole SOIL : codec compatible v1, noeuds 1/2 isoles, inconnu rejete, defauts, sessions, temps circulaires et frontiere 90min");
}
